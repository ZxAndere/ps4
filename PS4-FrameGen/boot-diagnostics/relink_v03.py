#!/usr/bin/env python3
"""Relink preserved v0.3 objects into a NEW directory, without building a PKG."""
import argparse
import json
import os
from pathlib import Path
import subprocess

from inspect_orbis import inspect

ROOT = Path(__file__).resolve().parents[1]
HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects-build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    old = args.objects_build.resolve()
    objects = sorted(p for p in (old / 'obj').glob('*.o') if p.name != 'bridge.o')
    if not objects or not any(p.name == 'ps4-app_main.cpp.o' for p in objects):
        parser.error('Expected the preserved native application object files')
    sdk = Path(os.environ['OO_PS4_TOOLCHAIN'])
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    commands = [
        [os.environ['FRAMEGEN_PS4_LD'], '-m', 'elf_x86_64', '-pie',
         '--script', str(HERE / 'link-boot.x'), '--eh-frame-hdr', '-e', '_start',
         '--gc-sections', '--build-id=none', '-z', 'max-page-size=0x4000',
         '-z', 'common-page-size=0x4000', '-L' + str(sdk / 'lib'),
         *map(str, objects), str(sdk / 'lib/crt1.o'), '--start-group',
         '-lc++', '-lc', '-lkernel', '-lSceVideoOut', '-lScePad',
         '-lSceUserService', '--end-group', '-o', str(out / 'framegen.elf')],
        [str(sdk / 'bin/linux/create-fself'), '-in=' + str(out / 'framegen.elf'),
         '-out=' + str(out / 'framegen.oelf'), '--eboot', str(out / 'eboot.bin'),
         '--paid', '0x3800000000000011'],
    ]
    with (out / 'build.txt').open('w') as log:
        for command in commands:
            log.write('COMMAND ' + repr(command) + '\n')
            log.flush()
            subprocess.run(command, cwd=ROOT, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
    audit = inspect(out / 'framegen.elf', out / 'framegen.oelf', out / 'eboot.bin')
    (out / 'executable-audit.json').write_text(json.dumps(audit, indent=2) + '\n')
    if not audit['static_valid'] or audit['warnings']:
        raise RuntimeError('Relinked executable did not pass the static audit')
    print('Original objects relinked and statically checked. No PKG; physical boot untested.')


if __name__ == '__main__':
    main()
