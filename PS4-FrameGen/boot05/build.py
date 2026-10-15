#!/usr/bin/env python3
"""Reproduce BOOT01 exactly, then test the missing OpenOrbis package stubs in BOOT05."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from audit import (BASELINE_ELF_SHA256, BASELINE_PKG_SHA256, BASELINE_SELF_SHA256, CONTENT_ID,
                   STUB_HASHES, TITLE_ID, audit_self, require, sha,
                   verify_package_tree, verify_sfo)
from inspect_orbis import inspect
from package_metadata import extract_metadata, read_sfo

ROOT = Path(__file__).resolve().parents[1]
OLD_SOURCE = ROOT / 'boot-diagnostics'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    sdk = Path(os.environ['OO_PS4_TOOLCHAIN'])
    cc = os.environ['FRAMEGEN_PS4_CXX']
    ld = os.environ['FRAMEGEN_PS4_LD']
    baseline = ROOT.parent / 'downloads/boot-audit-01/IV0000-FGBT00001_00-BOOTDIAG00000001.pkg'
    require(sha(baseline) == BASELINE_PKG_SHA256, 'Published BOOT01 package hash mismatch')
    for path, digest in STUB_HASHES.items():
        require(sha(sdk / 'samples/hello_world' / path) == digest, 'SDK stub hash mismatch: ' + path)
    out.mkdir(parents=True, exist_ok=False)
    log = out / 'build.txt'
    env = os.environ.copy()
    env['DOTNET_SYSTEM_GLOBALIZATION_INVARIANT'] = '1'
    env['DOTNET_BUNDLE_EXTRACT_BASE_DIR'] = str(out / 'dotnet')
    env['LD_LIBRARY_PATH'] = os.environ.get('FRAMEGEN_PKG_LIBDIR', '')
    tool = sdk / 'bin/linux/PkgTool.Core'

    def run(command, cwd=ROOT, logfile=log):
        command = list(map(str, command))
        with logfile.open('a') as stream:
            stream.write('COMMAND ' + repr(command) + '\n')
            stream.flush()
            subprocess.run(command, cwd=cwd, env=env, stdout=stream,
                           stderr=subprocess.STDOUT, check=True)

    def validate(pkg, report):
        run([tool, 'pkg_validate', pkg], logfile=report)
        text = report.read_text()
        require('[OK]' in text and '[FAIL]' not in text, 'Package integrity failed')

    # Recover the actual released control from its PKG, rather than trusting an old build folder.
    old = out / 'baseline'
    old.mkdir()
    validate(baseline, old / 'pkg-validate.txt')
    run([tool, 'pkg_extract', baseline, old / 'extracted'], logfile=old / 'pkg-extract.txt')
    baseline_sfo, _ = extract_metadata(tool, baseline, old, env)
    baseline_eboots = list((old / 'extracted').rglob('eboot.bin'))
    require(len(baseline_eboots) == 1, 'Ambiguous baseline executable')
    baseline_eboot = baseline_eboots[0]
    require(sha(baseline_eboot) == BASELINE_SELF_SHA256, 'Extracted BOOT01 SELF hash mismatch')

    # This intentionally reproduces the reviewed BOOT01 compile/link recipe.
    # Exact input ELF equality is a gate, not a claim that this recipe boots on the console.
    objdir = out / 'obj'
    objdir.mkdir()
    common = ['--target=x86_64-pc-freebsd12-elf', '-O1', '-msse2', '-fPIC',
              '-funwind-tables', '-fno-stack-protector', '-ffunction-sections',
              '-fdata-sections', '-D__PS4__', '-D_GNU_SOURCE', '-isysroot', sdk,
              '-isystem', sdk / 'include', '-I', ROOT, '-I', OLD_SOURCE,
              '-std=c11', '-x', 'c', '-DBOOT_STAGE=1']
    objects = []
    for i, name in enumerate(['platform.c', 'boot01.c']):
        obj = objdir / f'{i:02}-{Path(name).stem}.o'
        run([cc, *common, '-c', OLD_SOURCE / name, '-o', obj])
        objects.append(obj)
    elf, oelf = out / 'boot.elf', out / 'boot.oelf'
    package = out / 'package'
    (package / 'sce_sys').mkdir(parents=True)
    (package / 'sce_module').mkdir()
    recompiled_self = out / 'recompiled-eboot.bin'
    run([ld, '-m', 'elf_x86_64', '-pie', '--script', OLD_SOURCE / 'link-boot.x',
         '--eh-frame-hdr', '-e', '_start', '--gc-sections', '--build-id=none',
         '-z', 'max-page-size=0x4000', '-z', 'common-page-size=0x4000',
         '-L' + str(sdk / 'lib'), *objects, sdk / 'lib/crt1.o', '--start-group',
         '-lc', '-lkernel', '--end-group', '-o', elf])
    run([sdk / 'bin/linux/create-fself', '-in=' + str(elf), '-out=' + str(oelf),
         '--eboot', recompiled_self, '--paid', '0x3800000000000011'])
    require(sha(elf) == BASELINE_ELF_SHA256, 'Compiled ELF differs from the original BOOT01 input')
    executable = inspect(elf, oelf, recompiled_self)
    require(executable['static_valid'] and not executable['warnings'], 'Executable audit failed')
    (out / 'executable-audit.json').write_text(json.dumps(executable, indent=2) + '\n')

    # create-fself embeds its full input path in DT_SCE_FILENAME. A conversion in
    # another directory changes the container. Use the verified published SELF so
    # this package tests only the compatibility modules, not filename metadata.
    eboot = package / 'eboot.bin'
    shutil.copy2(baseline_eboot, eboot)

    for name in STUB_HASHES:
        shutil.copy2(sdk / 'samples/hello_world' / name, package / name)
    shutil.copy2(old / 'icon-packaged.png', package / 'sce_sys/icon0.png')
    run([tool, 'sfo_new', 'sce_sys/param.sfo'], cwd=package)
    strings = dict(baseline_sfo)
    strings.update(TITLE_ID=TITLE_ID, CONTENT_ID=CONTENT_ID, TITLE='FrameGen BOOT05 MODULE CONTROL')
    capacities = {'APP_VER': 8, 'VERSION': 8, 'CATEGORY': 4, 'CONTENT_ID': 48, 'TITLE_ID': 12, 'TITLE': 128}
    for name, value in strings.items():
        if name in ('PUBTOOLINFO', 'PUBTOOLVER'):
            continue
        is_int = isinstance(value, int)
        run([tool, 'sfo_setentry', 'sce_sys/param.sfo', name,
             '--type', 'Integer' if is_int else 'Utf8', '--maxsize',
             4 if is_int else capacities[name], '--value', value], cwd=package)
    files = ['eboot.bin', 'sce_sys/param.sfo', 'sce_sys/icon0.png', *STUB_HASHES]
    run([sdk / 'bin/linux/create-gp4', '-out', 'project.gp4', '--content-id=' + CONTENT_ID,
         '--files', ' '.join(files)], cwd=package)
    verify_package_tree(package, baseline_eboot, old / 'icon-packaged.png')
    run([tool, 'pkg_build', 'project.gp4', out], cwd=package)
    pkg = out / (CONTENT_ID + '.pkg')
    validate(pkg, out / 'pkg-validate.txt')
    run([tool, 'pkg_extract', pkg, out / 'extracted'], logfile=out / 'pkg-extract.txt')
    actual_sfo, _ = extract_metadata(tool, pkg, out, env)
    verify_sfo(read_sfo(package / 'sce_sys/param.sfo'), actual_sfo, baseline_sfo)
    require((out / 'icon-packaged.png').read_bytes() == (old / 'icon-packaged.png').read_bytes(),
            'Packaged icon differs from the control')
    extracted_roots = list((out / 'extracted').rglob('eboot.bin'))
    require(len(extracted_roots) == 1, 'Ambiguous extracted BOOT05 executable')
    pfs = extracted_roots[0].parent
    expected_pfs = {'eboot.bin', 'sce_sys/keystone', *STUB_HASHES}
    actual_pfs = {str(p.relative_to(pfs)) for p in pfs.rglob('*') if p.is_file()}
    require(actual_pfs == expected_pfs, 'Unexpected files in the actual packaged PFS')
    for name in ['eboot.bin', *STUB_HASHES]:
        require((pfs / name).read_bytes() == (package / name).read_bytes(), 'Packaged payload changed: ' + name)
    detailed_self = {name: audit_self(package / name) for name in ['eboot.bin', *STUB_HASHES]}
    (out / 'self-chain.json').write_text(json.dumps(detailed_self, indent=2) + '\n')
    inputs = [OLD_SOURCE / n for n in ['boot01.c', 'platform.c', 'platform.h', 'link-boot.x']]
    inputs += [sdk / n for n in ['lib/crt1.o', 'lib/libc.a', 'lib/libkernel.so',
                                'bin/linux/create-fself', 'bin/linux/create-gp4', 'bin/linux/PkgTool.Core']]
    manifest = {'series': 'boot05', 'file': pkg.name, 'sha256': sha(pkg), 'bytes': pkg.stat().st_size,
                'title_id': TITLE_ID, 'content_id': CONTENT_ID, 'eboot_sha256': sha(eboot),
                'eboot_identical_to_released_boot01': True, 'icon_identical_to_boot01': True,
                'recompiled_input_elf_identical': True, 'recompiled_elf_sha256': sha(elf),
                'payload_self_copied_from_verified_published_boot01': True,
                'added_payloads': STUB_HASHES, 'framegen_bridge_packaged': False,
                'frame_generation_enabled': False, 'physical_boot_confirmed': False,
                'root_cause_confirmed': False, 'expected_notification_label': 'FrameGen BOOT01: MAIN alcanzado.',
                'inputs': {str(p): sha(p) for p in inputs}}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('BOOT05 built: identical BOOT01 executable + two documented OpenOrbis stubs.')
    print('PKG integrity, extraction, identity and differential checks passed. Physical boot remains untested.')


if __name__ == '__main__':
    main()
