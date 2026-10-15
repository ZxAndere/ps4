#!/usr/bin/env python3
"""Read-only probes. Presence is not proof of console or GPU execution."""
import argparse,hashlib,json,os,pathlib,shutil,subprocess
p=argparse.ArgumentParser();p.add_argument('report',type=pathlib.Path);a=p.parse_args()
root=pathlib.Path(os.environ.get('FRAMEGEN_TOOLS_ROOT','/workspace/framegen-toolchains'))
sdk=pathlib.Path(os.environ.get('OO_PS4_TOOLCHAIN',root/'OpenOrbis/PS4Toolchain'))
header=sdk/'include/orbis/GnmDriver.h'
def commit(name):
 d=root/name
 if not d.is_dir():return None
 r=subprocess.run(['git','-C',str(d),'rev-parse','HEAD'],text=True,capture_output=True)
 return r.stdout.strip() if r.returncode==0 else None
out={'gpu_backend_implemented':False,'gpu_backend_executed':False,'console_executed':False,
 'dri_device_directory':pathlib.Path('/dev/dri').exists(),
 'emulators_found':{n:shutil.which(n) is not None for n in ['shadPS4','shadps4','fpPS4']},
 'console_endpoint_variables_present':{n:bool(os.environ.get(n)) for n in ['PS4_IP','PS4_HOST','PS4_PORT']},
 'gnm_header_sha256':hashlib.sha256(header.read_bytes()).hexdigest() if header.is_file() else None,
 'incomplete_compute_prototypes':[line.strip() for line in header.read_text().splitlines() if any(n in line for n in ['sceGnmDispatchDirect()','sceGnmMapComputeQueue()'])] if header.is_file() else [],
 'sources':{name:commit(name) for name in ['openorbis-source','goldhen-sdk','goldhen-plugins-v02']},
 'firmware_13_52_verified':False,'goldhen_beta12_verified':False}
with a.report.open('x') as f:json.dump(out,f,indent=2);f.write('\n')
print('Capability evidence saved. No console compatibility certified.')
