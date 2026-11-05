#!/usr/bin/env python3
"""Inventory actual compute instructions and optionally replay all native captures."""
import argparse
import collections
import concurrent.futures
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--lower', type=Path, help='lower-program executable, tensor enabled')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    files = sorted(args.captures.glob('*.bin'))
    rows = []
    totals = collections.Counter()
    mixed_sign = []
    for source in files:
        data = source.read_bytes()
        key = 0xcbf29ce484222325
        for byte in data:
            key = ((key ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        if source.name.split('-')[0] != f'{key:016x}':
            raise ValueError(f'filename hash mismatch: {source.name}')
        for pc in range(0, len(data), 16):
            word = int.from_bytes(data[pc:pc + 16], 'little')
            if word & 4095 in (0x231, 0x431, 0x631, 0x831, 0xa31) and word >> 86 & 1:
                mixed_sign.append([source.name, pc])
        row = {'file': source.name, 'bytes': len(data), 'hash_valid': True}
        if '-cs-' in source.name:
            result = subprocess.run(['nvdisasm', '-b', 'SM86', '-ndf', str(source)],
                                    capture_output=True, text=True, timeout=30, check=True)
            if result.stderr.strip():
                raise RuntimeError(result.stderr)
            (args.output / (source.stem + '.sass')).write_text(result.stdout)
            operations = collections.Counter(re.findall(
                r'/\*[0-9a-f]+\*/\s+(?:@!?\w+\s+)?([A-Z][A-Z0-9_.]+)', result.stdout))
            row['operations'] = dict(operations)
            totals.update(operations)
        rows.append(row)
    if args.lower:
        def lower(source):
            process = subprocess.run([str(args.lower.resolve()), str(source),
                                      str(args.output / (source.stem + '.maxwell.bin'))],
                                     capture_output=True, text=True, timeout=60)
            result = json.loads(process.stdout)
            if process.returncode != (0 if result['status'] == 'ok' else 1):
                raise RuntimeError(process.stderr)
            result.pop('source_map', None)
            return result
        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
            for row, result in zip(rows, pool.map(lower, files)):
                row['lowering'] = result
    report = {'files': rows, 'compute_operations': dict(totals), 'H0_NH1': mixed_sign}
    (args.output / 'census.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Filename hashes verified:', len(files))
    print('Compute captures disassembled:', sum('operations' in row for row in rows))
    print('H0_NH1 occurrences:', len(mixed_sign))
    if args.lower:
        print('Lowering:', dict(collections.Counter(row['lowering']['status'] for row in rows)))
    for row in rows:
        ops = row.get('operations', {})
        if any(op.startswith('SUST') and '3D' in op for op in ops):
            print('3D volume writer:', row['file'],
                  {k: v for k, v in ops.items() if k.startswith(('SUST', 'MUFU', 'HMUL', 'HFMA', 'HADD'))})


if __name__ == '__main__':
    main()
