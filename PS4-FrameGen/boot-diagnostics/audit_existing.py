#!/usr/bin/env python3
"""Audit the published v0.3 PKG independently of its build-time source SFO."""
import argparse,hashlib,json,os,re,struct,subprocess
from pathlib import Path
from inspect_orbis import inspect
from package_metadata import read_sfo,extract_metadata
R=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--original-build',type=Path,default=R/'build-ps4-v03-final')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    root=args.original_build.resolve();pkg=R.parent/'downloads/v0.3/IV0000-FGEN00003_00-PS4FRAMEGEN00003.pkg'
    env=os.environ.copy();env['DOTNET_SYSTEM_GLOBALIZATION_INVARIANT']='1';env['DOTNET_BUNDLE_EXTRACT_BASE_DIR']=str(out/'dotnet')
    env['LD_LIBRARY_PATH']=os.environ['FRAMEGEN_PKG_LIBDIR'];tool=Path(os.environ['OO_PS4_TOOLCHAIN'])/'bin/linux/PkgTool.Core'
    digest=hashlib.sha256(pkg.read_bytes()).hexdigest();assert digest=='68db8a909882f4eb2e4c2dea5fafa6af61adc8215c266a120a2891491cab27bc'
    with (out/'pkg-validate.txt').open('w') as f:subprocess.run([str(tool),'pkg_validate',str(pkg)],env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
    validation=(out/'pkg-validate.txt').read_text();assert '[OK]' in validation and '[FAIL]' not in validation
    with (out/'pkg-extract.txt').open('w') as f:subprocess.run([str(tool),'pkg_extract',str(pkg),str(out/'extracted')],env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
    sfo,entries=extract_metadata(tool,pkg,out,env);original=read_sfo(root/'package/sce_sys/param.sfo')
    for key,val in original.items():assert sfo[key]==val,key
    assert re.fullmatch('[A-Z]{4}[0-9]{5}',sfo['TITLE_ID'])
    assert sfo['CONTENT_ID']=='IV0000-'+sfo['TITLE_ID']+'_00-PS4FRAMEGEN00003'
    payloads={}
    for name in ['eboot.bin','framegen_bridge.prx']:
        matches=list((out/'extracted').rglob(name));assert len(matches)==1
        assert matches[0].read_bytes()==(root/'package'/name).read_bytes()
        payloads[name]=hashlib.sha256(matches[0].read_bytes()).hexdigest()
    binary=inspect(root/'framegen.elf',root/'framegen.oelf',root/'package/eboot.bin')
    f=(root/'package/eboot.bin').read_bytes();num=struct.unpack_from('<H',f,24)[0];elf=32+num*32;phnum=struct.unpack_from('<H',f,elf+56)[0]
    extended=(elf+64+phnum*56+15)&~15;paid,kind,version,firmware=struct.unpack_from('<4Q',f,extended)
    result={'published_pkg_sha256':digest,'pkg_hashes_and_fake_signatures_valid':True,'packaged_sfo':sfo,
      'source_sfo_values_preserved':True,'package_entries':entries,'payloads':payloads,'self_paid':hex(paid),'self_type':kind,
      'self_app_version_field':version,'self_firmware_field':firmware,'icon0_present':'ICON0_PNG' in entries,
      'executable':binary,'confirmed_console_result':'installed, listed, launch error, no UI (user report)',
      'console_copied_file_hash_verified':False,'root_cause_of_console_message_confirmed':False}
    (out/'audit.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Published package crypto/identity/payloads verified. Executable static_valid:',binary['static_valid'])
if __name__=='__main__':main()
