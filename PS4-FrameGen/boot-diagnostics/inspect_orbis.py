#!/usr/bin/env python3
"""Read-only ELF/OELF/fake SELF checks, including post-conversion mapped ranges.
The loader/firmware and physical console behavior cannot be certified by this tool.
"""
import argparse,hashlib,json,struct
from pathlib import Path

LOAD,RELRO,DYNLIB,PROC=1,0x61000010,0x61000000,0x61000001

def bounded(data,off,size):
    if off<0 or size<0 or off+size>len(data):raise ValueError('Binary range exceeds file')
    return data[off:off+size]

def cstring(data,offset):
    if offset>=len(data):raise ValueError('Invalid string offset')
    end=data.find(b'\0',offset)
    if end<0:raise ValueError('Unterminated string')
    return data[offset:end].decode('utf-8',errors='strict')

class ELF:
    def __init__(self,data,sections=True):
        self.data=data
        if len(data)<64 or data[:7]!=b'\x7fELF\x02\x01\x01':raise ValueError('Expected little-endian ELF64')
        self.osabi=data[7];self.kind,self.machine=struct.unpack_from('<HH',data,16)
        self.entry,self.phoff,self.shoff=struct.unpack_from('<QQQ',data,24)
        phsize,num,shsize,shnum,shstr=struct.unpack_from('<HHHHH',data,54)
        if phsize!=56:raise ValueError('Unsupported program header size')
        self.ph=[]
        for i in range(num):
            p=struct.unpack('<IIQQQQQQ',bounded(data,self.phoff+i*phsize,phsize))
            self.ph.append(dict(zip(['type','flags','offset','address','physical','file_size','memory_size','align'],p)))
        self.sections=[]
        if sections and shnum:
            if shsize!=64 or shstr>=shnum:raise ValueError('Bad section table')
            raw=[struct.unpack('<IIQQQQIIQQ',bounded(data,self.shoff+i*64,64)) for i in range(shnum)]
            strings=bounded(data,raw[shstr][4],raw[shstr][5])
            for s in raw:self.sections.append({'name':cstring(strings,s[0]),'type':s[1],'flags':s[2],'address':s[3],'offset':s[4],'size':s[5],'link':s[6],'entry_size':s[9]})
    def mapped(self,address,size,permission=0):
        return any(p['type'] in (LOAD,RELRO) and p['address']<=address and address+size<=p['address']+p['memory_size'] and
                   (p['flags']&permission)==permission for p in self.ph)
    def section(self,name):return next((s for s in self.sections if s['name']==name),None)
    def tags(self):
        p=next(p for p in self.ph if p['type']==2)
        raw=bounded(self.data,p['offset'],p['file_size']);out=[]
        for off in range(0,len(raw),16):
            tag,value=struct.unpack_from('<QQ',raw,off);out.append((tag,value))
            if not tag:return out
        raise ValueError('Dynamic table has no terminator')
    def symbols(self):
        out={}
        for s in self.sections:
            if s['type']!=2:continue
            strings=self.sections[s['link']];names=bounded(self.data,strings['offset'],strings['size'])
            for off in range(s['offset'],s['offset']+s['size'],24):
                name,info,other,idx,val,size=struct.unpack_from('<IBBHQQ',self.data,off)
                if name:out[cstring(names,name)]=(val,size,idx)
        return out

def inspect(original,converted,self_path):
    src=ELF(Path(original).read_bytes());out=ELF(Path(converted).read_bytes())
    errors=[];warnings=[]
    if src.machine!=62 or out.machine!=62:errors.append('Wrong machine, expected x86-64')
    if out.kind!=0xfe10 or out.osabi!=9:errors.append('Expected Orbis ET_SCE_EXEC_ASLR / FreeBSD ABI')
    if out.entry!=src.entry or not out.mapped(out.entry,1,1):errors.append('Entry is not preserved in an executable mapping')
    start=src.symbols().get('_start')
    if not start or start[0]!=src.entry:errors.append('Entry does not match SDK _start')
    for p in out.ph:
        bounded(out.data,p['offset'],p['file_size'])
        if p['type'] in (LOAD,RELRO):
            if p['file_size']>p['memory_size']:errors.append('Segment filesz exceeds memsz')
            if p['align']!=0x4000 or p['offset']%0x4000!=p['address']%0x4000:errors.append('Bad Orbis load alignment')
    uncovered=[]
    for s in src.sections:
        # The converter moves .dynamic into SCE_DYNLIBDATA. TLS has its own loader contract.
        if not s['size'] or not (s['flags']&2) or (s['flags']&0x400) or s['name']=='.dynamic':continue
        if not out.mapped(s['address'],s['size']):
            uncovered.append({'section':s['name'],'address':s['address'],'size':s['size'],'end':s['address']+s['size']})
    if uncovered:errors.append('Allocated input sections are not fully covered after conversion')
    proc=next((p for p in out.ph if p['type']==PROC),None)
    if not proc or proc['file_size']<0x50 or not out.mapped(proc['address'],proc['file_size']):errors.append('Missing/unmapped process parameters')
    else:
        param=bounded(out.data,proc['offset'],proc['file_size'])
        if struct.unpack_from('<Q',param)[0]!=0x50 or param[8:12]!=b'ORBI':errors.append('Invalid process parameter size/magic')
    tags=out.tags();t=dict(tags);dyn=next(p for p in out.ph if p['type']==DYNLIB)
    string_data=bounded(out.data,dyn['offset']+t[0x61000035],t[0x61000037])
    needed=[cstring(string_data,val) for tag,val in tags if tag==1]
    allowed={'libkernel.prx','libSceVideoOut.prx','libScePad.prx','libSceUserService.prx'}
    if set(needed)-allowed:errors.append('Unexpected dependency: '+str(set(needed)-allowed))
    symbol_count=t[0x6100003f]//24
    imported=[]
    for i in range(symbol_count):
        fields=struct.unpack('<IBBHQQ',bounded(out.data,dyn['offset']+t[0x61000039]+i*24,24))
        if fields[0] and fields[3]==0:imported.append(cstring(string_data,fields[0]))
    for offset_tag,size_tag in [(0x61000029,0x6100002d),(0x6100002f,0x61000031)]:
        raw=bounded(out.data,dyn['offset']+t[offset_tag],t[size_tag])
        if len(raw)%24:errors.append('Bad relocation table length')
        for i in range(0,len(raw),24):
            address,info,addend=struct.unpack_from('<QQq',raw,i)
            if not out.mapped(address,8):errors.append('Relocation target outside loaded memory: '+hex(address))
            if info>>32>=symbol_count:errors.append('Relocation symbol index out of range')
    tls=[]
    for p in out.ph:
        if p['type']==7:
            before=next(x for x in src.ph if x['type']==7)
            tls.append({'input':before,'converted':p})
            if before['align']!=p['align']:warnings.append('Converter changed TLS alignment; needs runtime verification')
    # SELF header/table bounds and each data entry versus the corresponding converted segment.
    f=Path(self_path).read_bytes()
    if len(f)<32 or f[:4]!=bytes.fromhex('4f153d1d'):raise ValueError('Expected fake SELF')
    header=struct.unpack_from('<I4BIHHQHH',f);count=header[9];elf_base=32+32*count
    if header[8]!=len(f):errors.append('SELF file size field mismatch')
    if bounded(f,elf_base,64+56*len(out.ph))!=out.data[:64+56*len(out.ph)]:errors.append('SELF embedded headers differ from OELF')
    verified=[]
    for i in range(count):
        properties,offset,size,memory=struct.unpack_from('<QQQQ',f,32+i*32)
        payload=bounded(f,offset,size)
        if properties&(1<<11):
            index=(properties>>20)&0xffff
            if index>=len(out.ph):errors.append('SELF segment index out of bounds');continue
            p=out.ph[index]
            if payload!=bounded(out.data,p['offset'],p['file_size']):errors.append('SELF segment data mismatch')
            verified.append(index)
    expected={i for i,p in enumerate(out.ph) if p['type'] in (LOAD,RELRO,DYNLIB)}
    if set(verified)!=expected:errors.append('SELF segment coverage mismatch')
    return {'static_valid':not errors,'errors':errors,'warnings':warnings,'uncovered_sections':uncovered,'needed_orbis_modules':needed,
      'imported_nids':imported,'elf_entry':out.entry,'sdk_start_symbol':start[0] if start else None,'tls':tls,
      'self_segments_verified':verified,'self_bytes':len(f),'self_sha256':hashlib.sha256(f).hexdigest(),
      'physical_boot_confirmed':False}

def main():
    p=argparse.ArgumentParser();p.add_argument('elf');p.add_argument('oelf');p.add_argument('self');p.add_argument('report',type=Path);p.add_argument('--expect-invalid',action='store_true');a=p.parse_args()
    result=inspect(a.elf,a.oelf,a.self)
    with a.report.open('x') as f:json.dump(result,f,indent=2);f.write('\n')
    print('PASS' if result['static_valid'] else 'FAIL',result['errors'],result['warnings'])
    return 0 if result['static_valid']!=a.expect_invalid else 1
if __name__=='__main__':raise SystemExit(main())
