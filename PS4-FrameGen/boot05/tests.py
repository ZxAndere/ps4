#!/usr/bin/env python3
"""BOOT05 checks and negative cases; never execute BOOT02-04 or modify old files."""
import json
import os
from pathlib import Path
import shutil
import struct
import tempfile
import unittest

from audit import (BASELINE_ELF_SHA256, BASELINE_SELF_SHA256, CONTENT_ID,
                   STUB_HASHES, audit_self, sha, verify_package_tree, verify_sfo)
from package_metadata import read_sfo

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get('FRAMEGEN_BOOT05_BUILD_DIR', ROOT / 'build-boot05-03'))
BASE = BUILD / 'baseline'
BASE_EBOOT = next((BASE / 'extracted').rglob('eboot.bin'))


class Boot05Tests(unittest.TestCase):
    def test_published_executable_is_exactly_preserved(self):
        self.assertEqual(sha(BUILD / 'package/eboot.bin'), BASELINE_SELF_SHA256)
        self.assertEqual((BUILD / 'package/eboot.bin').read_bytes(), BASE_EBOOT.read_bytes())
        self.assertEqual(sha(BUILD / 'boot.elf'), BASELINE_ELF_SHA256)

    def test_packaged_pfs_contains_exact_control_and_two_official_stubs(self):
        eboot = next((BUILD / 'extracted').rglob('eboot.bin'))
        expected = {'eboot.bin', 'sce_sys/keystone', *STUB_HASHES}
        actual = {str(p.relative_to(eboot.parent)) for p in eboot.parent.rglob('*') if p.is_file()}
        self.assertEqual(actual, expected)
        self.assertEqual(sha(eboot), BASELINE_SELF_SHA256)
        for name, digest in STUB_HASHES.items():
            self.assertEqual(sha(eboot.parent / name), digest)

    def test_package_identity_and_controlled_sfo_changes(self):
        source = read_sfo(BUILD / 'package/sce_sys/param.sfo')
        packaged = read_sfo(BUILD / 'param-packaged.sfo')
        verify_sfo(source, packaged, read_sfo(BASE / 'param-packaged.sfo'))
        self.assertEqual(packaged['CONTENT_ID'], CONTENT_ID)

    def test_actual_self_chain_is_orbis_and_has_no_authinfo_change(self):
        for name in ['eboot.bin', *STUB_HASHES]:
            a = audit_self(BUILD / 'package' / name)
            self.assertFalse(a['authinfo_present'])
            self.assertEqual(a['self_type'], 1)
            self.assertEqual(a['orbis_type'], '0xfe10' if name == 'eboot.bin' else '0xfe18')
        self.assertEqual(audit_self(BUILD / 'package/eboot.bin')['needed_modules'], ['libkernel.prx'])

    def test_validation_report_has_no_failed_checks(self):
        report = (BUILD / 'pkg-validate.txt').read_text()
        self.assertIn('[OK]', report)
        self.assertNotIn('[FAIL]', report)
        self.assertTrue(json.loads((BUILD / 'executable-audit.json').read_text())['static_valid'])

    def mutate_tree(self, mutation):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / 'package'
            shutil.copytree(BUILD / 'package', root)
            mutation(root)
            with self.assertRaises(ValueError):
                verify_package_tree(root, BASE_EBOOT, BASE / 'icon-packaged.png')

    def test_missing_stub_is_rejected(self):
        # Temporary copy only. No versioned artifacts are removed.
        self.mutate_tree(lambda root: (root / 'sce_module/libc.prx').rename(root / 'unexpected.prx'))

    def test_modified_stub_is_rejected(self):
        self.mutate_tree(lambda root: (root / 'sce_module/libSceFios2.prx').write_bytes(b'not the official stub'))

    def test_recompiled_or_changed_control_is_rejected(self):
        self.mutate_tree(lambda root: (root / 'eboot.bin').write_bytes((BUILD / 'recompiled-eboot.bin').read_bytes()))

    def test_framegen_bridge_or_other_payload_is_rejected(self):
        self.mutate_tree(lambda root: (root / 'framegen_bridge.prx').write_bytes(b'extra payload'))

    def test_changed_title_id_is_rejected(self):
        source = read_sfo(BUILD / 'package/sce_sys/param.sfo')
        source['TITLE_ID'] = 'FGBT00001'
        with self.assertRaises(ValueError):
            verify_sfo(source, source, read_sfo(BASE / 'param-packaged.sfo'))

    def test_invalid_self_length_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'bad.self'
            data = bytearray((BUILD / 'package/eboot.bin').read_bytes())
            struct.pack_into('<Q', data, 16, len(data) - 1)
            path.write_bytes(data)
            with self.assertRaises(ValueError):
                audit_self(path)

    def test_no_old_source_or_release_was_modified(self):
        snapshot = ROOT / 'docs/boot05/preservation/preserved-sha256.json'
        for name, digest in json.loads(snapshot.read_text()).items():
            # The repository landing page is intentionally updated with the latest test status.
            if name != 'README.md':
                self.assertEqual(sha(ROOT.parent / name), digest, name)


if __name__ == '__main__':
    unittest.main(verbosity=2)
