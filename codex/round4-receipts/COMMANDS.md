# Receipt commands

All paths below are relative to the repository root unless absolute.

- broad.log: cmake --build build-gather/tests --parallel 3 && build-gather/tests/gather-tests
- frontend.log: cmake --build build-gather/round2/frontend-build --parallel 3 && build-gather/round2/frontend-build/ampere-extra-tests
- python.log: python3 -m unittest discover -s tools/nx2 -p 'test_*.py'
- clang-configure.log: cmake -S build-gather -B build-gather/round4/clang-tests -G Ninja -DCMAKE_CXX_COMPILER=/usr/bin/clang++-18 -DCMAKE_BUILD_TYPE=Release
- clang-build.log: cmake --build build-gather/round4/clang-tests --parallel 3
- clang-tests.log: build-gather/round4/clang-tests/gather-tests
- numeric-clang.log and hardware.log: the exact build and run commands in ../round4/README.md
- numeric.log: the same numeric build/run using c++ instead of clang++-18
- census.log and census-summary.json: the census command in ../round4/README.md

The census summary keeps the complete compute inventory and global counts.
The complete 686-file JSON replay and SASS are local in build-gather/round4/census.
The lower-program driver uses tensor-enabled, non-geometry lowering as in v3.
There are no captured graphics stage headers in this old capture set.

These are standalone suite receipts, not a full emulator or project-wide build.
