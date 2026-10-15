#!/usr/bin/env python3
"""Regression checks on the real binaries; mutations are confined to a temporary directory."""
import hashlib,json,os,struct,tempfile,unittest
from pathlib import Path
from inspect_orbis import ELF,inspect
from package_metadata import read_sfo
R=Path(__file__).resolve().parents[1]
B=Path(os.environ.get('FRAMEGEN_BOOT_BUILD_DIR',str(R/'build-boot-audit-02')))
V=Path(os.environ.get('FRAMEGEN_V03_BUILD_DIR',str(R/'build-ps4-v03-final')))
RELINK=Path(os.environ.get('FRAMEGEN_BOOT_RELINK_DIR',str(B/'relinked-v03')))

class BootAuditTests(unittest.TestCase):
    def test_published_v03_failure_is_detected(self):
        p=V;a=inspect(p/'framegen.elf',p/'framegen.oelf',p/'package/eboot.bin')
        self.assertFalse(a['static_valid']);self.assertEqual(a['uncovered_sections'][0]['section'],'.bss')
        before=ELF((p/'framegen.elf').read_bytes());after=ELF((p/'framegen.oelf').read_bytes());s=before.section('.bss')
        end=max(x['address']+x['memory_size'] for x in after.ph if x['type']==1 and x['flags']==6)
        self.assertEqual(s['address']+s['size']-end,8)
    def test_relinked_original_objects_have_complete_mapping(self):
        p=RELINK;a=inspect(p/'framegen.elf',p/'framegen.oelf',p/'eboot.bin')
        self.assertTrue(a['static_valid']);self.assertFalse(a['uncovered_sections']);self.assertFalse(a['warnings'])
    def test_all_progressive_packages_have_valid_self_and_no_extra_dependencies(self):
        for stage in range(1,5):
            p=B/f'boot{stage:02}';a=inspect(p/'boot.elf',p/'boot.oelf',p/'package/eboot.bin')
            self.assertTrue(a['static_valid'],a);self.assertFalse(a['warnings'])
            expected={'libkernel.prx'}
            if stage>=2:expected.add('libSceVideoOut.prx')
            if stage>=3:expected.update(['libScePad.prx','libSceUserService.prx'])
            self.assertEqual(set(a['needed_orbis_modules']),expected)
            files=[x.name for x in (p/'package').rglob('*') if x.is_file()]
            self.assertEqual(set(files),{'eboot.bin','param.sfo','icon0.png','project.gp4'})
    def test_stage_one_has_no_engine_cxx_or_graphics_symbols(self):
        e=ELF((B/'boot01/boot.elf').read_bytes());names=e.symbols()
        self.assertFalse(any('sceVideoOut' in n or 'scePad' in n or n.startswith('_ZN') or 'pthread_create' in n for n in names))
        self.assertTrue(e.mapped(e.entry,1,1))
    def test_packaged_sfo_identity_is_distinct_and_preserved(self):
        for i in range(1,5):
            p=B/f'boot{i:02}';packaged=read_sfo(p/'param-packaged.sfo');source=read_sfo(p/'package/sce_sys/param.sfo')
            for key,value in source.items():self.assertEqual(packaged[key],value)
            self.assertEqual(packaged['TITLE_ID'],f'FGBT{i:05}');self.assertNotEqual(packaged['TITLE_ID'],'FGEN00003')
    def test_bad_header_is_rejected(self):
        with self.assertRaises(ValueError):ELF(b'not an executable')
    def test_truncated_program_table_is_rejected(self):
        b=bytearray((B/'boot01/boot.oelf').read_bytes());struct.pack_into('<Q',b,32,len(b)-1)
        with self.assertRaises(ValueError):ELF(b)
    def test_changed_entry_point_is_rejected(self):
        p=B/'boot01'
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'bad.oelf';b=bytearray((p/'boot.oelf').read_bytes());struct.pack_into('<Q',b,24,0xffffffffffff)
            f.write_bytes(b);a=inspect(p/'boot.elf',f,p/'package/eboot.bin');self.assertFalse(a['static_valid'])
            self.assertTrue(any('Entry' in x for x in a['errors']))
    def test_shrunken_memory_coverage_is_rejected(self):
        p=B/'boot02'
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'bad.oelf';b=bytearray((p/'boot.oelf').read_bytes());e=ELF(b)
            index=next(i for i,x in enumerate(e.ph) if x['type']==1 and x['flags']==6)
            ph=e.ph[index];struct.pack_into('<Q',b,e.phoff+index*56+40,ph['file_size'])
            f.write_bytes(b);a=inspect(p/'boot.elf',f,p/'package/eboot.bin');self.assertFalse(a['static_valid'])
            self.assertTrue(a['uncovered_sections'])
    def test_corrupted_self_payload_is_rejected(self):
        p=B/'boot01'
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'bad.bin';b=bytearray((p/'package/eboot.bin').read_bytes());count=struct.unpack_from('<H',b,24)[0]
            offset=next(struct.unpack_from('<QQQQ',b,32+i*32)[1] for i in range(count) if struct.unpack_from('<Q',b,32+i*32)[0]&(1<<11))
            b[offset]^=1;f.write_bytes(b);a=inspect(p/'boot.elf',p/'boot.oelf',f)
            self.assertIn('SELF segment data mismatch',a['errors'])
    def test_bad_sfo_bounds_are_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'bad.sfo';b=bytearray((B/'boot01/param-packaged.sfo').read_bytes());struct.pack_into('<I',b,16,0xffffffff);f.write_bytes(b)
            with self.assertRaises(ValueError):read_sfo(f)
    def test_all_old_release_hashes_remain_unchanged(self):
        for filename in ['docs/build-manifest.json','docs/build-manifest-v02.json','docs/build-manifest-v03.json']:
            for name,expected in json.loads((R/filename).read_text())['files'].items():self.assertEqual(hashlib.sha256((R/name).read_bytes()).hexdigest(),expected,name)
        for name,expected in json.loads((R/'artifacts/boot-recovery-v03/preserved-sha256.json').read_text()).items():
            self.assertEqual(hashlib.sha256((R.parent/name).read_bytes()).hexdigest(),expected,name)
if __name__=='__main__':unittest.main(verbosity=2)
