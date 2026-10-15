#!/usr/bin/env python3
"""Build fresh, separate boot PKGs; never rebuild previous FrameGen versions."""
import argparse,hashlib,json,os,shutil,struct,subprocess,sys,zlib
from pathlib import Path
from package_metadata import read_sfo,extract_metadata

ROOT=Path(__file__).resolve().parents[1];HERE=Path(__file__).resolve().parent

def png_icon(path,stage):
    # Original deterministic diagnostic icon, no external textures/fonts/modules.
    width=512;rows=[]
    segments=((160,100,180,24),(340,124,24,126),(340,274,24,126),(160,400,180,24),(136,274,24,126),(136,124,24,126),(160,250,180,24))
    mask=(0x06,0x5b,0x4f,0x66)[stage-1]
    for y in range(width):
        line=bytearray()
        for x in range(width):
            lit=any(mask&(1<<i) and a<=x<a+w and b<=y<b+h for i,(a,b,w,h) in enumerate(segments))
            line.extend((85,237,112,255) if lit else (12,24,42,255))
        rows.append(b'\0'+line)
    def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,width,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b''))

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build-boot-audit-01');a=p.parse_args();base=a.output.resolve()
    base.mkdir(parents=True,exist_ok=False)
    sdk=Path(os.environ['OO_PS4_TOOLCHAIN']);cc=Path(os.environ['FRAMEGEN_PS4_CXX']);ld=os.environ['FRAMEGEN_PS4_LD']
    env=os.environ.copy();env['DOTNET_SYSTEM_GLOBALIZATION_INVARIANT']='1';env['DOTNET_BUNDLE_EXTRACT_BASE_DIR']=str(base/'dotnet')
    env['LD_LIBRARY_PATH']=os.environ.get('FRAMEGEN_PKG_LIBDIR','')+(':'+os.environ['LD_LIBRARY_PATH'] if os.environ.get('LD_LIBRARY_PATH') else '')
    tool=sdk/'bin/linux/PkgTool.Core';manifest={'series':'boot-audit-01','physical_boot_confirmed':False,'frame_generation_enabled':False,'prx_packaged':False,'packages':{}}
    common=['--target=x86_64-pc-freebsd12-elf','-O1','-msse2','-fPIC','-funwind-tables','-fno-stack-protector','-ffunction-sections','-fdata-sections',
      '-D__PS4__','-D_GNU_SOURCE','-isysroot',str(sdk),'-isystem',str(sdk/'include'),'-I',str(ROOT),'-I',str(HERE)]
    def run(args,cwd,log):
        with log.open('a') as output:
            output.write('COMMAND '+repr([str(x) for x in args])+'\n');output.flush()
            subprocess.run([str(x) for x in args],cwd=cwd,env=env,stdout=output,stderr=subprocess.STDOUT,check=True)
    all_engine=['engine/capture/image.cpp','engine/motion/block_match.cpp','engine/interpolation/interpolate.cpp','engine/scaling/scale.cpp',
      'engine/performance/pipeline.cpp','engine/presentation/monitor.cpp','engine/optimized/processor.cpp','overlay/menu/draw.cpp']
    for stage in range(1,5):
        d=base/f'boot{stage:02}';(d/'obj').mkdir(parents=True);pkgroot=d/'package';(pkgroot/'sce_sys').mkdir(parents=True);log=d/'build.txt'
        sources=[HERE/'platform.c',HERE/(f'boot{stage:02}.c' if stage<3 else 'boot_cpp.cpp')]
        if stage==3:sources.extend(ROOT/s for s in ['engine/capture/image.cpp','overlay/menu/draw.cpp'])
        if stage==4:sources.extend(ROOT/s for s in all_engine)
        objects=[]
        for i,source in enumerate(sources):
            obj=d/'obj'/f'{i:02}-{source.stem}.o';flags=common.copy()
            if source.suffix=='.cpp':flags+=['-std=c++17','-isystem',str(sdk/'include/c++/v1')]
            else:flags+=['-std=c11','-x','c']
            run([cc,*flags,f'-DBOOT_STAGE={stage}','-c',source,'-o',obj],ROOT,log);objects.append(obj)
        libraries=['-lc','-lkernel']
        if stage>=2:libraries+=['-lSceVideoOut']
        if stage>=3:libraries+=['-lScePad','-lSceUserService','-lc++']
        elf=d/'boot.elf';oelf=d/'boot.oelf';eboot=pkgroot/'eboot.bin'
        run([ld,'-m','elf_x86_64','-pie','--script',HERE/'link-boot.x','--eh-frame-hdr','-e','_start','--gc-sections','--build-id=none',
             '-z','max-page-size=0x4000','-z','common-page-size=0x4000','-L'+str(sdk/'lib'),*objects,sdk/'lib/crt1.o','--start-group',*libraries,'--end-group','-o',elf],ROOT,log)
        run([sdk/'bin/linux/create-fself','-in='+str(elf),'-out='+str(oelf),'--eboot',eboot,'--paid','0x3800000000000011'],ROOT,log)
        run([sys.executable,HERE/'inspect_orbis.py',elf,oelf,eboot,d/'executable-audit.json'],ROOT,log)
        title_id=f'FGBT{stage:05}';content=f'IV0000-{title_id}_00-BOOTDIAG{stage:08}'
        run([tool,'sfo_new','sce_sys/param.sfo'],pkgroot,log)
        ints={'APP_TYPE':1,'ATTRIBUTE':0,'DOWNLOAD_DATA_SIZE':0,'SYSTEM_VER':0}
        for name,value in ints.items():run([tool,'sfo_setentry','sce_sys/param.sfo',name,'--type','Integer','--maxsize','4','--value',str(value)],pkgroot,log)
        for name,value,size in [('APP_VER','01.00',8),('VERSION','01.00',8),('CATEGORY','gd',4),('CONTENT_ID',content,48),('TITLE_ID',title_id,12),('TITLE',f'FrameGen BOOT{stage:02} DIAGNOSTIC',128)]:
            run([tool,'sfo_setentry','sce_sys/param.sfo',name,'--type','Utf8','--maxsize',str(size),'--value',value],pkgroot,log)
        png_icon(pkgroot/'sce_sys/icon0.png',stage)
        run([sdk/'bin/linux/create-gp4','-out','project.gp4','--content-id='+content,'--files','eboot.bin sce_sys/param.sfo sce_sys/icon0.png'],pkgroot,log)
        run([tool,'pkg_build','project.gp4',d],pkgroot,log)
        run([tool,'pkg_validate',d/(content+'.pkg')],ROOT,d/'pkg-validate.txt')
        validation=(d/'pkg-validate.txt').read_text()
        if '[OK]' not in validation or '[FAIL]' in validation:
            raise RuntimeError('PKG integrity validation failed: '+str(d))
        run([tool,'pkg_extract',d/(content+'.pkg'),d/'extracted'],ROOT,d/'pkg-extract.txt')
        run([tool,'sfo_listentries','sce_sys/param.sfo'],pkgroot,d/'sfo.txt')
        candidates=list((d/'extracted').rglob('eboot.bin'));assert len(candidates)==1
        assert candidates[0].read_bytes()==eboot.read_bytes()
        packaged_sfo,entries=extract_metadata(tool,d/(content+'.pkg'),d,env)
        for key,value in read_sfo(pkgroot/'sce_sys/param.sfo').items():assert packaged_sfo[key]==value,key
        assert (d/'icon-packaged.png').read_bytes()==(pkgroot/'sce_sys/icon0.png').read_bytes()
        (d/'packaged-sfo.json').write_text(json.dumps(packaged_sfo,indent=2)+'\n')
        pkg=d/(content+'.pkg');manifest['packages'][title_id]={'stage':stage,'file':pkg.name,'bytes':pkg.stat().st_size,
          'sha256':hashlib.sha256(pkg.read_bytes()).hexdigest(),'eboot_sha256':hashlib.sha256(eboot.read_bytes()).hexdigest(),
          'executable_audit':json.loads((d/'executable-audit.json').read_text()),'physical_boot_confirmed':False}
        print(f'BOOT{stage:02} built, Orbis-checked, PKG-validated and extracted: {pkg.name}',flush=True)
    (base/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('All diagnostic PKGs compiled. Physical launch is still pending.')
if __name__=='__main__':main()
