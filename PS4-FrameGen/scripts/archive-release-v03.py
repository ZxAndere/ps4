#!/usr/bin/env python3
"""Freeze a verified v0.3 release while retaining reproducible earlier baselines."""
import hashlib,json,pathlib,shutil,zipfile
R=pathlib.Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 d=R/'artifacts/releases/0.3'
 if d.exists():raise RuntimeError('Release already exists; do not replace it')
 verify=json.loads((R/'docs/validation-v03/verification.json').read_text())
 b=R/'build-ps4-v03-final';pkg=b/'IV0000-FGEN00003_00-PS4FRAMEGEN00003.pkg';prx=b/'package/framegen_bridge.prx'
 assert sha(pkg)==verify['pkg_sha256'] and sha(prx)==verify['payloads']['framegen_bridge.prx']
 for manifest in ['docs/build-manifest.json','docs/build-manifest-v02.json','docs/build-manifest-v03.json']:
  for name,expected in json.loads((R/manifest).read_text())['files'].items():assert sha(R/name)==expected,name
 for name,expected in json.loads((R/'artifacts/v03/recovery-v02/sha256.json').read_text()).items():
  if name.startswith('artifacts/releases/0.2/') or name.startswith('artifacts/v03/recovery-v02/'):
   assert sha(R/name)==expected,name
 assert '100% tests passed' in (R/'docs/validation-v03/sanitizers-release.txt').read_text()
 assert '100% tests passed' in (R/'docs/validation-v03/ctest-release.txt').read_text()
 windows=json.loads((R/'docs/validation-v03/windows-binaries.json').read_text())
 for n,record in windows.items():assert sha(R/'build-windows-v03'/n)==record['sha256'],n
 d.mkdir(parents=True);(d/'windows').mkdir()
 manifest={'version':'0.3','console_executed':False,'windows_executed':False,'gpu_backend_executed':False,'files':{}}
 def copy(src,target):
  shutil.copy2(src,target);manifest['files'][target.relative_to(d).as_posix()]=sha(target)
 copy(pkg,d/pkg.name);copy(prx,d/prx.name)
 for name in windows:copy(R/'build-windows-v03'/name,d/'windows'/name)
 copy(R/'scripts/test-windows-release.ps1',d/'windows/test-windows-release.ps1')
 copy(R/'artifacts/v03/comparison-final.html',d/'comparison.html')
 windows_archive=d/'PS4-FrameGen-0.3-windows.zip'
 with zipfile.ZipFile(windows_archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for f in sorted((d/'windows').iterdir()):z.write(f,f.name)
 with zipfile.ZipFile(windows_archive) as z:assert z.testzip() is None
 manifest['files'][windows_archive.name]=sha(windows_archive)
 files=[]
 for source in R.rglob('*'):
  if not source.is_file():continue
  parts=source.relative_to(R).parts
  if any(p.startswith('build') or p in ('artifacts','data','__pycache__','.git') for p in parts[:-1]):continue
  files.append(source)
 # Retain the exact files cited by the old manifests, plus complete previous source recovery points.
 for name in ['docs/build-manifest.json','docs/build-manifest-v02.json','docs/build-manifest-v03.json']:
  files.extend(R/key for key in json.loads((R/name).read_text())['files'])
 for name in ['artifacts/v03/recovery-v02','artifacts/v03/replay-final']:
  files.extend(p for p in (R/name).rglob('*') if p.is_file())
 files.extend([R/'artifacts/optimization-v02/baseline/source-before-optimization.zip',
               R/'artifacts/optimization-v02/baseline/framegen_cli',
               R/'artifacts/v03/quality/summary.json',R/'artifacts/v03/interleaved-verified.json',
               R/'artifacts/v03/capabilities.json',R/'artifacts/v03/benchmark-notes.json'])
 for i in range(5):files.append(R/f'artifacts/final/decoded/{i}.ppm')
 for n in ['framegen_cli','framegen_tests','framegen_runtime_tests','framegen_bridge_tests','libframegen_bridge.so',
           'framegen_inspect','framegen_replay','framegen_benchmark_v03','framegen_ablation']:
  files.append(R/'build-v03'/n)
 files.extend([pkg,prx,b/'framegen.elf',b/'framegen_bridge.elf'])
 for p in (R/'build-windows-v03').glob('*'):
  if p.suffix in ('.exe','.dll'):files.append(p)
 archive=d/'PS4-FrameGen-0.3-project.zip';files=sorted(set(files))
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for source in files:z.write(source,'PS4-FrameGen/'+source.relative_to(R).as_posix())
 with zipfile.ZipFile(archive) as z:
  assert z.testzip() is None
  for name,expected in json.loads((R/'docs/build-manifest-v03.json').read_text())['files'].items():
   assert hashlib.sha256(z.read('PS4-FrameGen/'+name)).hexdigest()==expected,name
  for name in ['engine/runtime.hpp','tests/runtime_tests.cpp','docs/V03.md','artifacts/v03/recovery-v02/sha256.json']:
   assert 'PS4-FrameGen/'+name in z.namelist()
 manifest['files'][archive.name]=sha(archive);manifest['source_and_artifact_files_in_zip']=len(files)
 (d/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 (d/'README.txt').write_text('PS4 FrameGen 0.3. Ver docs/V03.md, RESULTS_V03.md e INSTALL_V03.md dentro del ZIP.\n'
  'PKG/PRX compilados y verificados estructuralmente, sin prueba física de PS4.\n'
  'Sin compatibilidad acreditada con firmware 13.52/GoldHEN beta 12 ni juegos externos.\n'
  'Windows: EXE/DLL compilados; ejecutar test-windows-release.ps1 en esa plataforma para verificar.\n'
  'comparison.html: comparador local de imágenes reales con referencias retenidas.\n')
 print('Archived',len(files),'files;',archive.stat().st_size,'ZIP bytes;',d)
if __name__=='__main__':main()
