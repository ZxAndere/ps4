#!/usr/bin/env python3
"""Archive a concrete release without replacing older files or reports."""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

ROOT=Path(__file__).resolve().parents[1]

def main():
    destination=ROOT/'artifacts/releases/0.2'
    destination.mkdir(parents=True,exist_ok=False)
    build=ROOT/'build-ps4-v02-final'
    pkg=build/'IV0000-FGEN00001_00-PS4FRAMEGEN00001.pkg'
    prx=build/'package/framegen_bridge.prx'
    verification=json.loads((ROOT/'docs/validation-v02/verification.json').read_text())
    if hashlib.sha256(pkg.read_bytes()).hexdigest()!=verification['pkg_sha256']:
        raise RuntimeError('Package differs from verified artifact')
    if hashlib.sha256(prx.read_bytes()).hexdigest()!=verification['payloads']['framegen_bridge.prx']:
        raise RuntimeError('PRX differs from verified artifact')
    manifest={'version':'0.2','console_executed':False,'files':{}}
    for source in [pkg,prx]:
        target=destination/source.name;shutil.copy2(source,target)
        manifest['files'][target.name]=hashlib.sha256(target.read_bytes()).hexdigest()
    sources=[]
    for source in ROOT.rglob('*'):
        if not source.is_file():continue
        parents=source.relative_to(ROOT).parts[:-1]
        if any(p.startswith('build') or p in ('artifacts','data','__pycache__','.git') for p in parents):continue
        sources.append(source)
    # Keep the exact v0.1 binaries needed by its manifest and the benchmark executable/source snapshot.
    old=json.loads((ROOT/'docs/build-manifest.json').read_text())
    historical=[ROOT/name for name in old['files']]
    historical += [ROOT/'artifacts/optimization-v02/baseline/framegen_cli',
                   ROOT/'artifacts/optimization-v02/baseline/source-before-optimization.zip']
    files=sorted(set(sources+historical))
    archive=destination/'PS4-FrameGen-0.2-project.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for source in files:z.write(source,'PS4-FrameGen/'+source.relative_to(ROOT).as_posix())
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None:raise RuntimeError('Archive CRC check failed')
        for required in ['CMakeLists.txt','scripts/build-ps4.sh','scripts/verify-ps4-artifacts.py',
                         'goldhen/plugin/bridge.cpp','docs/OPTIMIZATION_V02.md','tests/bridge_tests.cpp']:
            if 'PS4-FrameGen/'+required not in z.namelist():raise RuntimeError('Missing release source')
    manifest['files'][archive.name]=hashlib.sha256(archive.read_bytes()).hexdigest()
    manifest['source_files_in_zip']=len(files)
    (destination/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (destination/'README.txt').write_text('PS4 FrameGen 0.2. Código, referencias 0.1 y resultados en el ZIP.\n'
        'PKG y PRX compilados y verificados estructuralmente; sin ejecutar en consola.\n'
        'No se acredita compatibilidad con firmware 13.52/GoldHEN beta 12 ni integración en juegos.\n'
        'Consultar docs/OPTIMIZATION_V02.md, PKG_V02.md y GOLDHEN_V02.md.\n')
    print('Archived',len(files),'project/history files;',archive.stat().st_size,'ZIP bytes;',destination)

if __name__=='__main__':main()
