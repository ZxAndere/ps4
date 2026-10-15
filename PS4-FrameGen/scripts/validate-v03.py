#!/usr/bin/env python3
"""Compare frozen v0.2 with v0.3 against genuinely withheld 25-Hz frames."""
import argparse,hashlib,json,math,pathlib,subprocess
import numpy as np
from PIL import Image
R=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,default=R/'artifacts/v03/quality');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=False)
frames=R/'artifacts/final/decoded'
old=R/'artifacts/v03/recovery-v02/framegen_cli';new=R/'build-v03/framegen_cli'
# Every case has a valid withheld reference. Never use guessed timing for x3/x4.
cases=[]
for res in ('720','540','360'):
 cases.extend([(f'v02-{res}',old,2,res,[]),(f'v03-{res}',new,2,res,[])])
for res in ('720','360'):
 cases.append((f'one-iteration-{res}',new,2,res,['--warp-iterations','1']))
for filt in ('nearest','bilinear','bicubic'):
 cases.append((f'{filt}-360',new,2,'360',['--filter',filt]))
cases += [('sharp25-360',new,2,'360',['--sharpness','25']),('v03-1080',new,2,'1080',[])]
for m in (3,4):
 for version,binary in [('v02',old),('v03',new)]:cases.append((f'{version}-720-x{m}',binary,m,'720',[]))
summary=[]
reference=a.output/'reference'
subprocess.run([str(new),'--input',str(frames/'0.ppm'),'--next',str(frames/'2.ppm'),'--truth',str(frames/'1.ppm'),
 '--source-fps','12.5','--process','720','--output','720','--output-dir',str(reference)],check=True,stdout=subprocess.DEVNULL)
for name,binary,m,res,options in cases:
 dest=a.output/name
 cmd=[str(binary),'--input',str(frames/'0.ppm'),'--next',str(frames/f'{m}.ppm'),'--source-fps',str(25/m),
      '--multiplier',str(m),'--process',res,'--output','720','--repeats','9','--output-dir',str(dest)]
 for i in range(1,m):cmd.extend(['--truth',str(frames/f'{i}.ppm')])
 subprocess.run(cmd+options,check=True,stdout=subprocess.DEVNULL)
 metrics=json.loads((dest/'metrics.json').read_text());metrics['name']=name;quality=[]
 for i in range(1,m):
  # Fixed area-filtered reference across every reconstruction filter.
  truth=reference/'truth_1.ppm' if i==1 else a.output/f'v03-720-x{m}'/f'truth_{i}.ppm'
  if not truth.exists():
   # old/new same temporal reference with Area; either is valid for m=3/4.
   truth=dest/f'truth_{i}.ppm'
  pred=np.asarray(Image.open(dest/f'frame_{i}.ppm'),dtype=np.float64)
  gt=np.asarray(Image.open(truth),dtype=np.float64)
  err=float(np.mean((pred-gt)**2));edge=float(np.mean(np.abs(np.diff(pred,axis=1)-np.diff(gt,axis=1))))
  q={'t':i/m,'psnr_db':10*math.log10(255**2/err) if err else None,'mae':float(np.mean(np.abs(pred-gt))),'horizontal_edge_mae':edge}
  quality.append(q)
  if i==1:
   Image.open(dest/f'frame_{i}.ppm').save(dest/'generated.png')
   Image.open(truth).save(dest/'reference.png')
 metrics['fixed_reference_quality']=quality
 if name.startswith('v03-') and res in ('720','540','360') and m==2:
  baseline=a.output/f'v02-{res}'/'frame_1.ppm'
  x=np.asarray(Image.open(baseline),dtype=np.int16);y=np.asarray(Image.open(dest/'frame_1.ppm'),dtype=np.int16)
  metrics['v02_max_rgb_error']=int(np.max(np.abs(x-y)))
  if metrics['v02_max_rgb_error']!=0:raise RuntimeError('v0.2 pixel regression: '+name)
 summary.append(metrics);print(name,round(metrics['median_pair_ms'],3),'ms',round(quality[0]['psnr_db'],4),'dB',flush=True)
# Validate all intermediate positions for x3/x4 too.
for m in (3,4):
 for i in range(1,m):
  old_image=np.asarray(Image.open(a.output/f'v02-720-x{m}'/f'frame_{i}.ppm'),dtype=np.int16)
  new_image=np.asarray(Image.open(a.output/f'v03-720-x{m}'/f'frame_{i}.ppm'),dtype=np.int16)
  if np.max(np.abs(old_image-new_image))!=0:raise RuntimeError('Temporal pixel regression')
report={'dataset':'OpenCV 1920x1080.avi @ 25Hz; input 0,M; truth 1..M-1','console_executed':False,
 'cases':summary,'all_same_algorithm_v02_pixels_identical':True,'psnr_is_not_perceptual_quality_guarantee':True}
(a.output/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
# A standalone, local comparator uses actual originals, references, generated images and differences.
import sys
subprocess.run([sys.executable,str(R/'scripts/build-comparison.py'),str(a.output),str(a.output/'comparison.html')],check=True)
