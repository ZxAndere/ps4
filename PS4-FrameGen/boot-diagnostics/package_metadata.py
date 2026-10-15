"""Strict SFO parsing and published PKG metadata extraction (no console claims)."""
import struct,subprocess
from pathlib import Path

def read_sfo(path):
    raw=Path(path).read_bytes()
    if len(raw)<20:raise ValueError('Truncated SFO')
    magic,version,keys,data,count=struct.unpack_from('<5I',raw)
    if magic!=0x46535000 or version!=0x101 or keys>data or data>len(raw) or 20+count*16>keys:raise ValueError('Invalid SFO header')
    out={}
    for i in range(count):
        key,fmt,size,capacity,offset=struct.unpack_from('<HHIII',raw,20+i*16)
        if keys+key>=data or size>capacity or data+offset+capacity>len(raw):raise ValueError('Invalid SFO bounds')
        end=raw.find(b'\0',keys+key,data)
        if end<0:raise ValueError('Invalid SFO key')
        name=raw[keys+key:end].decode('ascii');value=raw[data+offset:data+offset+size]
        if name in out:raise ValueError('Duplicate SFO key')
        if fmt==0x204:
            if not value or value[-1]!=0:raise ValueError('Unterminated SFO text')
            value=value[:-1].decode('utf-8')
        elif fmt==0x404:
            if size!=4 or capacity!=4:raise ValueError('Invalid integer SFO')
            value=struct.unpack('<I',value)[0]
        else:raise ValueError('Unsupported SFO format')
        out[name]=value
    return out

def extract_metadata(tool,pkg,output,env):
    listing=subprocess.check_output([str(tool),'pkg_listentries',str(pkg)],env=env,text=True)
    (output/'pkg-entries.txt').write_text(listing)
    entries={}
    for line in listing.splitlines():
        pieces=line.split()
        if len(pieces)>=5 and pieces[0].startswith('0x'):entries[pieces[-1]]=int(pieces[3])
    names={'PARAM_SFO':'param-packaged.sfo','ICON0_PNG':'icon-packaged.png'}
    for name,target in names.items():
        if name in entries:
            subprocess.run([str(tool),'pkg_extractentry',str(pkg),str(entries[name]),str(output/target)],env=env,check=True)
    if 'PARAM_SFO' not in entries:raise ValueError('PKG has no PARAM_SFO')
    return read_sfo(output/'param-packaged.sfo'),entries
