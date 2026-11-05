#!/usr/bin/env python3
"""Exercise the standalone profile-phase5.cpp binary under fresh process environments."""
import os
from pathlib import Path
import subprocess
import sys

names = ['EDEN_NX2_GPU_PROFILE', 'EDEN_NX2_SAMPLE_PROFILE',
         'EDEN_NX2_GPU_PROFILE_AFTER_SAMPLE']
cases = [({}, (0, 0)), ({names[0]: '1'}, (1, 1)),
         ({names[0]: '1', names[1]: '250'}, (0, 0)),
         ({names[0]: '1', names[1]: '250', names[2]: '1'}, (0, 1)),
         ({names[0]: '1', names[1]: '0'}, (1, 1)),
         ({names[0]: '1', names[1]: '250', names[2]: '0'}, (0, 0)),
         ({names[1]: '250', names[2]: '1'}, (0, 0)),
         ({names[0]: '1', names[2]: '1'}, (1, 1))]
for values, expected in cases:
    env = {k: v for k, v in os.environ.items() if k not in names}
    env.update(values)
    result = subprocess.run([str(Path(sys.argv[1]).resolve()), *map(str, expected)],
                            env=env, capture_output=True, text=True, check=True)
    print(f'{values}: {result.stdout.strip()}')
print('8/8 phase-gate cases passed')
