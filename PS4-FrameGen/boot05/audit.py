"""Strict BOOT05 differential checks. Static evidence does not certify PS4 boot."""
import hashlib
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'boot-diagnostics'))
from inspect_orbis import ELF, bounded, cstring

BASELINE_PKG_SHA256 = '7393781b7b26ef3818ca823084d885b7e0c3ac18efe77ef6201bc5c7d21f3d47'
BASELINE_SELF_SHA256 = 'ad6cdb3be767681159c99d52c3cfdac6195f1c6323d158886253f539c2a64263'
BASELINE_ELF_SHA256 = '72b66a17839972e7b1150b2645d74082a2e14af13efbc8079df9303bb5fb6a27'
STUB_HASHES = {
    'sce_module/libc.prx': '39ad53672bb0b14895f8465eb1619478ab935f5afe5dbf69c15f02cfc0a75ce6',
    'sce_module/libSceFios2.prx': '3f8236c5996cf8e9917b8d27706d9743f6603d89de3b9859eacbb9f466b19c01',
}
TITLE_ID = 'FGBT00005'
CONTENT_ID = 'IV0000-FGBT00005_00-BOOTDIAG00000005'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def audit_self(path):
    """Decode the SELF container and its SCE tables, without relying on section names."""
    raw = Path(path).read_bytes()
    require(len(raw) >= 32 and raw[:4] == bytes.fromhex('4f153d1d'), 'Bad SELF magic')
    fields = struct.unpack_from('<I4BIHHQHH', raw)
    require(fields[1:6] == (0, 1, 1, 0x12, 0x101) and fields[10] == 0x22,
            'Unexpected SELF version/mode/endian/attributes/key/flags')
    count = fields[9]
    require(fields[8] == len(raw), 'SELF file length mismatch')
    elf_offset = 32 + count * 32
    header = bounded(raw, elf_offset, 64)
    phnum = struct.unpack_from('<H', header, 56)[0]
    headers = bounded(raw, elf_offset, 64 + phnum * 56)
    elf = ELF(headers, sections=False)
    require(elf.machine == 62 and elf.osabi == 9 and elf.kind in (0xfe10, 0xfe18),
            'Wrong Orbis architecture/ABI/type')
    image = bytearray(max(p['offset'] + p['file_size'] for p in elf.ph))
    image[:len(headers)] = headers
    segments = []
    for i in range(count):
        prop, offset, size, memsize = struct.unpack('<4Q', bounded(raw, 32 + i * 32, 32))
        payload = bounded(raw, offset, size)
        if not prop & (1 << 11):
            continue
        index = (prop >> 20) & 0xffff
        require(index < len(elf.ph), 'SELF segment index out of range')
        p = elf.ph[index]
        require(size == p['file_size'] and memsize == size, 'SELF segment size mismatch')
        image[p['offset']:p['offset'] + size] = payload
        segments.append(index)
    expected = {i for i, p in enumerate(elf.ph) if p['type'] in (1, 0x61000010, 0x61000000)}
    require(set(segments) == expected and len(segments) == len(expected), 'SELF segment coverage mismatch')
    extended = (elf_offset + len(headers) + 15) & ~15
    paid, kind, app_version, fw_version = struct.unpack('<4Q', bounded(raw, extended, 32))
    require(paid == 0x3800000000000011 and kind == 1, 'Unexpected fake SELF authentication type')
    signature_offset = fields[6] + fields[7] - 0x100
    require(signature_offset >= fields[6], 'Invalid SELF metadata extent')
    signature = bounded(raw, signature_offset, 0x100)
    e = ELF(bytes(image), sections=False)
    tags = e.tags()
    t = dict(tags)
    dyn = next(p for p in e.ph if p['type'] == 0x61000000)
    strings = bounded(image, dyn['offset'] + t[0x61000035], t[0x61000037])
    modules = [cstring(strings, value) for tag, value in tags if tag == 1]
    imported = []
    for i in range(t[0x6100003f] // 24):
        name, info, other, section, value, size = struct.unpack(
            '<IBBHQQ', bounded(image, dyn['offset'] + t[0x61000039] + i * 24, 24))
        if name and not section:
            imported.append(cstring(strings, name))
    self_header = dict(zip(['magic', 'version', 'mode', 'endian', 'attributes', 'key_type',
                            'header_size', 'metadata_size', 'file_size', 'entry_count', 'flags'], fields))
    return {'sha256': sha(path), 'bytes': len(raw), 'self_header': self_header, 'orbis_type': hex(e.kind),
            'entry': e.entry, 'program_headers': e.ph, 'segments_verified': segments,
            'paid': hex(paid), 'self_type': kind, 'app_version_field': app_version,
            'firmware_field': fw_version, 'authinfo_present': any(signature),
            'needed_modules': modules, 'imported_nids': imported}


def verify_package_tree(root, baseline_eboot, baseline_icon):
    root = Path(root)
    expected = {'eboot.bin', 'sce_sys/param.sfo', 'sce_sys/icon0.png', 'project.gp4', *STUB_HASHES}
    actual = {str(p.relative_to(root)) for p in root.rglob('*') if p.is_file()}
    require(actual == expected, 'Unexpected or missing package files: ' + str(actual ^ expected))
    require(sha(root / 'eboot.bin') == BASELINE_SELF_SHA256, 'BOOT01 executable changed')
    require((root / 'eboot.bin').read_bytes() == Path(baseline_eboot).read_bytes(), 'Executable differs from published BOOT01')
    require((root / 'sce_sys/icon0.png').read_bytes() == Path(baseline_icon).read_bytes(), 'Control icon changed')
    for name, digest in STUB_HASHES.items():
        require(sha(root / name) == digest, 'Wrong SDK compatibility stub: ' + name)


def verify_sfo(source, packaged, baseline):
    changes = {'TITLE_ID': TITLE_ID, 'CONTENT_ID': CONTENT_ID, 'TITLE': 'FrameGen BOOT05 MODULE CONTROL'}
    for name, value in source.items():
        require(packaged.get(name) == value, 'Packaged SFO differs: ' + name)
    for name, value in baseline.items():
        if name in ('PUBTOOLINFO', 'PUBTOOLVER'):
            continue
        require(source.get(name) == changes.get(name, value), 'Unexpected SFO change: ' + name)
    for name, value in changes.items():
        require(source.get(name) == value, 'Wrong BOOT05 identity: ' + name)
