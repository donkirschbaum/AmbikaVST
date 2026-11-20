#!/usr/bin/env python3
"""Builds src/plugin/FactoryBank.inc from Ambika program files.

Reads Ambika .PRO files (RIFF "MBKS" containers written by the Ambika SD card
code) and Ambika SysEx dumps (.syx), extracts the patch (112 bytes) and part
data (84 bytes), removes duplicates and writes a C++ table.

Usage: import_presets.py OUTPUT.inc BANKNAME=DIR [BANKNAME=DIR ...]
"""

import hashlib
import os
import struct
import sys

PATCH_SIZE = 112
PART_SIZE = 84

# Ambika init part data (controller/part.cc), used when a file has no part.
INIT_PART = bytes([
    120, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 10, 16, 16, 16, 1,
    0xff, 0xff, 0x80, 0x80, 0xcc, 0xcc, 0x20, 0x20,
    0x00, 0x20, 0x40, 0x60, 0x80, 0xa0, 0xc0, 0xff,
    0x00, 0x10, 0x20, 0x40, 0x80, 0xff, 0x80, 0x40,
    0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x80, 0x40,
    60 | 0x80, 100, 60, 100, 48 | 0x80, 100, 48, 100,
    60 | 0x80, 100, 60, 100, 48 | 0x80, 100, 48, 100,
    60 | 0x80, 100, 60, 100, 48 | 0x80, 100, 48 | 0x80, 100 | 0x80,
    60 | 0x80, 100, 60, 100, 48 | 0x80, 100, 48, 100,
    0, 0, 0, 0])
assert len(INIT_PART) == PART_SIZE


def parse_pro(data):
    if data[:4] != b'RIFF' or data[8:12] != b'MBKS':
        return None
    name, patch, part = None, None, None
    pos = 12
    while pos + 8 <= len(data):
        tag = data[pos:pos + 4]
        size = struct.unpack('<I', data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if tag == b'name':
            name = body.split(b'\0')[0].decode('latin-1').strip()
        elif tag == b'obj ' and len(body) >= 4:
            kind = body[0]  # StorageObject + 1
            payload = body[4:]
            if kind == 1 and len(payload) >= PATCH_SIZE:
                patch = payload[:PATCH_SIZE]
            elif kind == 5 and len(payload) >= PART_SIZE:
                part = payload[:PART_SIZE]
        pos += 8 + size + (size & 1)
    return name, patch, part


def parse_mul(data):
    """A multi holds 6 parts; returns a list of (name, patch, part)."""
    if data[:4] != b'RIFF' or data[8:12] != b'MBKS':
        return []
    name, patches, parts = None, {}, {}
    pos = 12
    while pos + 8 <= len(data):
        tag = data[pos:pos + 4]
        size = struct.unpack('<I', data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if tag == b'name':
            name = body.split(b'\0')[0].decode('latin-1').strip()
        elif tag == b'obj ' and len(body) >= 4:
            kind, alias, payload = body[0], body[1], body[4:]
            if kind == 1 and len(payload) >= PATCH_SIZE:
                patches[alias] = payload[:PATCH_SIZE]
            elif kind == 5 and len(payload) >= PART_SIZE:
                parts[alias] = payload[:PART_SIZE]
        pos += 8 + size + (size & 1)
    return [('%s %d' % (name or 'Multi', a + 1), patches[a], parts.get(a))
            for a in sorted(patches)]


def parse_syx(data):
    patch, part = None, None
    for msg in data.split(b'\xf0')[1:]:
        msg = msg.split(b'\xf7')[0]
        if msg[:5] != b'\x00\x21\x02\x00\x04' or len(msg) < 7:
            continue
        command = msg[5]
        nibbles = msg[7:-2]
        payload = bytes((nibbles[i] << 4) | nibbles[i + 1] for i in range(0, len(nibbles) - 1, 2))
        if command == 1 and len(payload) >= PATCH_SIZE:
            patch = payload[:PATCH_SIZE]
        elif command == 5 and len(payload) >= PART_SIZE:
            part = payload[:PART_SIZE]
        elif command == 3 and len(payload) >= PATCH_SIZE + PART_SIZE:
            patch, part = payload[:PATCH_SIZE], payload[PATCH_SIZE:PATCH_SIZE + PART_SIZE]
    return None, patch, part


def clean_name(name, path):
    if not name:
        name = os.path.splitext(os.path.basename(path))[0]
        # "b126_BoC418" -> "BoC418"
        if '_' in name and name.split('_')[0][1:].isdigit():
            name = name.split('_', 1)[1]
    name = ''.join(c if 32 <= ord(c) < 127 else '?' for c in name).strip()
    return name or '(unnamed)'


def main():
    out_path = sys.argv[1]
    entries, seen = [], set()
    stats = []
    for spec in sys.argv[2:]:
        bank, directory = spec.split('=', 1)
        added = dupes = bad = 0
        files = []
        for root, _, names in os.walk(directory):
            for n in names:
                if n.lower().endswith(('.pro', '.syx', '.mul')):
                    files.append(os.path.join(root, n))
        for path in sorted(files):
            data = open(path, 'rb').read()
            lower = path.lower()
            if lower.endswith('.mul'):
                results = parse_mul(data)
            else:
                results = [parse_pro(data) if lower.endswith('.pro') else parse_syx(data)]
            for result in results:
                if not result or result[1] is None:
                    bad += 1
                    continue
                name, patch, part = result
                part = part or INIT_PART
                key = hashlib.sha1(patch + part).hexdigest()
                if key in seen:
                    dupes += 1
                    continue
                seen.add(key)
                entries.append((bank, clean_name(name, path), patch + part))
                added += 1
        stats.append((bank, added, dupes, bad))

    with open(out_path, 'w') as f:
        f.write('// Generated by tools/import_presets.py - do not edit.\n')
        f.write('// Ambika programs: 112 bytes patch + 84 bytes part data each.\n')
        f.write('// Sources and credits: see PRESETS.md.\n')
        for bank, name, blob in entries:
            esc = name.replace('\\', '\\\\').replace('"', '\\"')
            f.write('{"%s", "%s", "%s"},\n' % (bank, esc, blob.hex()))
    for bank, added, dupes, bad in stats:
        print('%-28s %4d added  %4d duplicates  %3d unreadable' % (bank, added, dupes, bad))
    print('total', len(entries))


if __name__ == '__main__':
    main()
