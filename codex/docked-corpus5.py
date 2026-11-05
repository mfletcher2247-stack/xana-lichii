#!/usr/bin/env python3
"""Compare the docked corpus with handheld and replay two local lowering drivers."""
import argparse
import collections
import concurrent.futures
import ctypes
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('docked', type=Path)
    parser.add_argument('handheld', type=Path)
    parser.add_argument('work', type=Path)
    parser.add_argument('report', type=Path)
    args = parser.parse_args()
    files = sorted(p for p in args.docked.glob('*.bin')
                   if not p.name.endswith('.header.bin'))
    handheld = {hashlib.sha256(p.read_bytes()).hexdigest()
                for p in args.handheld.glob('*.bin')}
    library = ctypes.CDLL(str((args.work / 'cityhash.so').resolve()))
    library.hash_bytes.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    library.hash_bytes.restype = ctypes.c_uint64
    for tree in ['pristine', 'v3', 'new-docked']:
        (args.work / tree).mkdir(exist_ok=True)

    def lower(source):
        data = source.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        key = 0xcbf29ce484222325
        for byte in data:
            key = ((key ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        assert source.name.split('-')[0] == f'{key:016x}', source.name
        row = {'sha256': digest, 'native_key': f'{key:016x}',
               'new_content': digest not in handheld, 'bytes': len(data)}
        if row['new_content']:
            shutil.copy2(source, args.work / 'new-docked' / source.name)
            header = source.with_suffix('.header.bin')
            if header.exists():
                shutil.copy2(header, args.work / 'new-docked' / header.name)
        for tree in ['pristine', 'v3']:
            output = args.work / tree / (source.stem + '.maxwell.bin')
            proc = subprocess.run([str((args.work / ('lower-' + tree)).resolve()),
                                   str(source), str(output)],
                                  capture_output=True, text=True, timeout=60)
            result = json.loads(proc.stdout)
            assert proc.returncode == (0 if result['status'] == 'ok' else 1), proc.stderr
            (args.work / tree / (source.stem + '.json')).write_text(proc.stdout)
            if result['status'] == 'ok':
                header = source.with_suffix('.header.bin')
                # GenericEnvironment copies the 80-byte SPH, then scheduling words,
                # trailing zero and native address map. Compute has a zero SPH.
                words = list(struct.unpack('<10Q', header.read_bytes()[:80])) if header.exists() else [0] * 10
                lowered = output.read_bytes()
                for (word,) in struct.iter_unpack('<Q', lowered):
                    if len(words) % 4 == 0:
                        words.append(0)
                    words.append(word)
                words.append(0)
                offsets = result['source_map']
                if offsets:
                    words.extend(0x50 + i * 8 + ((i + 1) // 3) * 8 for i in offsets)
                    words.extend([len(offsets), 0x50414d4e4544454e])
                code = struct.pack('<' + str(len(words)) + 'Q', *words)
                result['frame_hash'] = f'{library.hash_bytes(code, len(code)) ^ 0x4f554e4345000002:016x}'
            result.pop('source_map', None)
            row[tree] = result
        return source.name, row

    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        rows = dict(pool.map(lower, files))
    summary = {'program_files': len(files), 'header_files': len(list(args.docked.glob('*.header.bin'))),
               'compute_files': sum('-cs-' in p.name for p in files),
               'distinct_contents': len({r['sha256'] for r in rows.values()}),
               'new_content_files': sum(r['new_content'] for r in rows.values()),
               'new_distinct_contents': len({r['sha256'] for r in rows.values() if r['new_content']}),
               'trees': {}}
    for tree in ['pristine', 'v3']:
        summary['trees'][tree] = {
            'all': dict(collections.Counter(r[tree]['status'] for r in rows.values())),
            'new': dict(collections.Counter(r[tree]['status'] for r in rows.values() if r['new_content'])),
            'rejected': {n: r[tree] for n, r in rows.items() if r[tree]['status'] != 'ok'},
            'driver_sha256': hashlib.sha256((args.work / ('lower-' + tree)).read_bytes()).hexdigest()}
    args.report.write_text(json.dumps({'summary': summary, 'programs': rows}, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
