#!/usr/bin/env python3
"""Static Orbis/package checks. Does not execute or certify a console firmware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--build',type=Path,default=ROOT/'build-ps4-v03-final')
    parser.add_argument('--output',type=Path,default=ROOT/'artifacts/v03/pkg-verification')
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    required={'fg_bridge_api','plugin_load','plugin_unload','g_pluginName','g_pluginDesc','g_pluginAuth','g_pluginVersion'}
    exports=subprocess.check_output(['nm','-D','--defined-only',str(args.build/'framegen_bridge.elf')],text=True)
    names={line.split()[-1] for line in exports.splitlines() if line.split()}
    if not required.issubset(names):raise RuntimeError('Missing PRX dynamic exports: '+str(required-names))
    (args.output/'prx-exports.txt').write_text(exports)
    dependencies={}
    for name in ['framegen.elf','framegen_bridge.elf']:
        header=subprocess.check_output(['readelf','-h',str(args.build/name)],text=True)
        if 'Advanced Micro Devices X86-64' not in header:raise RuntimeError('Wrong architecture')
        dynamic=subprocess.check_output(['readelf','-d',str(args.build/name)],text=True)
        deps=re.findall(r'Shared library: \[(.*?)\]',dynamic)
        if set(deps)-{'libkernel.so','libSceVideoOut.so','libScePad.so','libSceUserService.so'}:
            raise RuntimeError('Unexpected host dependency')
        dependencies[name]=deps
        (args.output/(name+'.txt')).write_text(header+'\n'+dynamic)
    sdk=Path(os.environ['OO_PS4_TOOLCHAIN'])
    tool=sdk/'bin/linux/PkgTool.Core'
    env=os.environ.copy()
    env['DOTNET_BUNDLE_EXTRACT_BASE_DIR']=str(args.build/'dotnet')
    env['DOTNET_SYSTEM_GLOBALIZATION_INVARIANT']='1'
    env['LD_LIBRARY_PATH']=os.environ.get('FRAMEGEN_PKG_LIBDIR','')+(':'+os.environ['LD_LIBRARY_PATH'] if os.environ.get('LD_LIBRARY_PATH') else '')
    packages=list(args.build.glob('*.pkg'))
    if len(packages)!=1:raise RuntimeError('Expected exactly one selected PKG')
    pkg=packages[0]
    with (args.output/'sfo.txt').open('w') as f:
        subprocess.run([str(tool),'sfo_listentries',str(args.build/'package/sce_sys/param.sfo')],env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
    sfo=(args.output/'sfo.txt').read_text()
    if 'FGEN00003' not in sfo or 'IV0000-FGEN00003_00-PS4FRAMEGEN00003' not in sfo:
        raise RuntimeError('Unexpected selected package identity')
    with (args.output/'pkg-validate.txt').open('w') as f:
        subprocess.run([str(tool),'pkg_validate',str(pkg)],env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
    validation=(args.output/'pkg-validate.txt').read_text()
    if '[OK]' not in validation or '[FAIL]' in validation:raise RuntimeError('Package validator did not confirm success')
    extraction=args.output/'extracted'
    with (args.output/'pkg-extract.txt').open('w') as f:
        subprocess.run([str(tool),'pkg_extract',str(pkg),str(extraction)],env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
    hashes={}
    for name in ['eboot.bin','framegen_bridge.prx']:
        paths=list(extraction.rglob(name))
        expected=args.build/'package'/name
        if len(paths)!=1 or paths[0].read_bytes()!=expected.read_bytes():raise RuntimeError('Payload mismatch: '+name)
        if expected.read_bytes()[:4]!=bytes.fromhex('4f153d1d'):raise RuntimeError('Expected fake SELF format')
        hashes[name]=hashlib.sha256(expected.read_bytes()).hexdigest()
    old=json.loads((ROOT/'docs/build-manifest.json').read_text())
    for name,checksum in old['files'].items():
        if hashlib.sha256((ROOT/name).read_bytes()).hexdigest()!=checksum:raise RuntimeError('v0.1 artifact changed: '+name)
    old_v02=json.loads((ROOT/'docs/build-manifest-v02.json').read_text())
    for name,checksum in old_v02['files'].items():
        if hashlib.sha256((ROOT/name).read_bytes()).hexdigest()!=checksum:raise RuntimeError('v0.2 artifact changed: '+name)
    preserved=json.loads((ROOT/'artifacts/v03/recovery-v02/sha256.json').read_text())
    for name,checksum in preserved.items():
        if name.startswith('artifacts/releases/0.2/') or name.startswith('artifacts/v03/recovery-v02/'):
            if hashlib.sha256((ROOT/name).read_bytes()).hexdigest()!=checksum:raise RuntimeError('Preserved artifact changed: '+name)
    result={'pkg_sha256':hashlib.sha256(pkg.read_bytes()).hexdigest(),'payloads':hashes,
        'dynamic_exports':sorted(required),'dependencies':dependencies,'previous_manifest_unchanged':True,'v02_release_unchanged':True,
        'console_executed':False,'firmware_13_52_verified':False,'goldhen_beta12_verified':False}
    (args.output/'verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: PRX exports, Orbis dependencies, PKG hashes/signatures and extracted payload equality. Console execution unverified.')

if __name__=='__main__':main()
