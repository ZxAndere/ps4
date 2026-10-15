#!/usr/bin/env python3
"""Paired ablation against retained v0.1 code. Refuses to replace an existing report."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def raster(path):
    with path.open('rb') as f:
        if f.readline().strip() != b'P6':
            raise ValueError('Expected P6')
        size = list(map(int, f.readline().split()))
        if f.readline().strip() != b'255':
            raise ValueError('Expected 8 bit')
        data = f.read()
        if len(data) != size[0]*size[1]*3:
            raise ValueError('Raster length mismatch')
        return size, data

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', type=Path, default=ROOT/'build-opt/framegen_cli')
    parser.add_argument('--baseline', type=Path, default=ROOT/'artifacts/optimization-v02/baseline/framegen_cli')
    parser.add_argument('--frames', type=Path, default=ROOT/'artifacts/final/decoded')
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/optimization-v02/final-comparison')
    parser.add_argument('--repeats', type=int, default=9)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    # Change one factor at each named step; compare quality outside all timed regions.
    common = ['--kernels','fast','--threads','1','--simd','off','--reuse','off','--direct','off','--motion','full']
    def options(**changes):
        values = dict(zip(common[::2], common[1::2]))
        values.update({'--'+k:str(v) for k,v in changes.items()})
        return [item for pair in values.items() for item in pair]
    stages = [
        ('retained-v01',args.baseline,[], '720',2),
        ('reference-v02',args.executable,['--kernels','reference','--threads','1'],'720',2),
        ('scalar-kernels',args.executable,options(),'720',2),
        ('sse2',args.executable,options(simd='on'),'720',2),
        ('direct-buffers',args.executable,options(simd='on',direct='on'),'720',2),
        ('reused-storage',args.executable,options(simd='on',direct='on',reuse='on'),'720',2),
        ('workers-2',args.executable,options(simd='on',direct='on',reuse='on',threads=2),'720',2),
        ('workers-4',args.executable,options(simd='on',direct='on',reuse='on',threads=4),'720',2),
        ('motion-early',args.executable,options(simd='on',direct='on',reuse='on',threads=4,motion='early'),'720',2),
    ]
    fast = options(simd='on',direct='on',reuse='on',threads=4,motion='early')
    stages += [(f'retained-v01-x{m}',args.baseline,[],'720',m) for m in (3,4)]
    for multiplier in (2,3,4):
        for processing in ('1080','540','360'):
            stages.append((f'process-{processing}-x{multiplier}',args.executable,fast,processing,multiplier))
        if multiplier != 2:
            stages.append((f'process-720-x{multiplier}',args.executable,fast,'720',multiplier))
    records=[]
    baselines={}
    for name,executable,flags,processing,multiplier in stages:
        destination=args.output/name
        command=[executable,'--input',args.frames/'0.ppm','--next',args.frames/f'{multiplier}.ppm',
                 '--multiplier',multiplier,'--process',processing,'--output','720','--source-fps',25/multiplier,
                 '--repeats',args.repeats,'--output-dir',destination,*flags]
        for i in range(1,multiplier):command+=['--truth',args.frames/f'{i}.ppm']
        subprocess.run([str(x) for x in command],check=True)
        record=json.loads((destination/'metrics.json').read_text())
        record['stage']=name
        if name.startswith('retained-v01'):
            baselines[multiplier]={'record':record,'rasters':[raster(destination/f'frame_{i}.ppm') for i in range(1,multiplier)]}
        size,data=raster(destination/'frame_1.ppm')
        if size!=record['output_buffer']:raise ValueError('Reported size mismatch')
        if multiplier==2 and processing=='720':
            baseline_data=baselines[multiplier]['rasters'][0][1]
            if data==baseline_data: maximum=0;difference=0
            else:
                maximum=max(abs(a-b) for a,b in zip(data,baseline_data))
                difference=sum((a-b)**2 for a,b in zip(data,baseline_data))/len(data)
            record['max_channel_error_vs_v01']=maximum
            record['mse_vs_v01']=difference
            if maximum>1:raise ValueError(f'Quality equivalence failed for {name}: {maximum}')
            if records and abs(record['quality'][0]['psnr_db']-records[0]['quality'][0]['psnr_db'])>.01:
                raise ValueError('PSNR regression in equal-resolution optimization')
        if processing=='720':
            maximum=0
            for i in range(1,multiplier):
                _,data=raster(destination/f'frame_{i}.ppm');_,original=baselines[multiplier]['rasters'][i-1]
                if data!=original:maximum=max(maximum,max(abs(a-b) for a,b in zip(data,original)))
                if abs(record['quality'][i-1]['psnr_db']-baselines[multiplier]['record']['quality'][i-1]['psnr_db'])>.01:
                    raise ValueError('Equal-resolution PSNR regression')
            record['max_channel_error_vs_v01']=maximum
            if maximum>1:raise ValueError('Equal-resolution pixel regression')
        record['speedup_vs_v01']=baselines[multiplier]['record']['median_pair_ms']/record['median_pair_ms']
        records.append(record)
    result={'host':'AMD EPYC 9V74, Debian 13, quota 4 CPU; not PS4',
        'repeats':args.repeats,'warmup_pairs':1,'display_presented':False,
        'inputs':{f'{i}.ppm':hashlib.sha256((args.frames/f'{i}.ppm').read_bytes()).hexdigest() for i in range(5)},
        'cases':records}
    (args.output/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Completed',len(records),'comparisons. No display FPS measured.')

if __name__=='__main__':main()
