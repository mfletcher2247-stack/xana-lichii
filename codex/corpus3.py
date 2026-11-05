#!/usr/bin/env python3
"""Re-run every captured file with the local per-tree lowering drivers."""
import collections
import concurrent.futures
import hashlib
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
CAPTURES = pathlib.Path('/home/xana/xana/30-Research/oot-60fps-20261104/captures/raw')
WORK = ROOT / 'build-gather/round3'
TREES = ['baseline', 'v1', 'g', 'v2', 'v3']
FILES = sorted(CAPTURES.glob('*.bin'))
assert len(FILES) == 686


def lower(item):
    tree, source = item
    binary = (ROOT / 'build-gather/round2' / ('lower-' + tree)
              if tree != 'v3' else WORK / 'lower-v3-c')
    output = WORK / tree
    output.mkdir(exist_ok=True)
    process = subprocess.run([str(binary), str(source),
                              str(output / (source.stem + '.maxwell.bin'))],
                             capture_output=True, text=True, timeout=60)
    result = json.loads(process.stdout)
    assert process.returncode == (0 if result['status'] == 'ok' else 1)
    (output / (source.stem + '.json')).write_text(process.stdout)
    result.pop('source_map', None)
    return source.name, tree, result


results = {source.name: {} for source in FILES}
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    for name, tree, result in pool.map(lower, [(t, p) for t in TREES for p in FILES]):
        results[name][tree] = result
out = ROOT / 'codex'
(out / 'corpus-table-v3.json').write_text(json.dumps(results, indent=2) + '\n')
lines = ['# Whole captured program lowering, round 3', '',
         '686 files, tensor emulation enabled. Graphics has no captured stage header.', '',
         '| Tree | Lowered | Rejected |', '|---|---:|---:|']
for tree in TREES:
    counts = collections.Counter(row[tree]['status'] for row in results.values())
    lines.append(f"| {tree} | {counts['ok']} | {counts['reject']} |")
    print(tree, dict(counts), flush=True)
lines += ['', '| Program | ' + ' | '.join(TREES) + ' |', '|---|' + '---|' * len(TREES)]
for name, row in results.items():
    cells = [f"OK ({row[t]['lowered_words']} words)" if row[t]['status'] == 'ok'
             else f"{row[t]['reason']} @ 0x{row[t]['offset']:x}" for t in TREES]
    lines.append('| ' + name + ' | ' + ' | '.join(cells) + ' |')
(out / 'corpus-table-v3.md').write_text('\n'.join(lines) + '\n')
unique = {hashlib.sha256(p.read_bytes()).hexdigest(): p.name for p in FILES}
print('Distinct contents:', len(unique))
for tree in TREES:
    print('Distinct', tree, dict(collections.Counter(results[n][tree]['status']
                                                   for n in unique.values())))
