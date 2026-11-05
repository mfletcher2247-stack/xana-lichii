READY-TO-PUSH

Round 3 complete on xana/oot-gather-v3, based on b16b23fbe7.
No push. No remote or other-machine access. No Git config edits.
V2 remains b16b23fbe73a8b98c9d8af0256c02c7eb54c54b2.

A committed and green: 8f0d580fc20cdfa97f01e0f7f51cd4e593fbb628.
Captured LEA.HI is R1=(R1>>2)-R0. B-negation now survives the final IADD;
immediate, uniform and .X modifier semantics remain distinct.
B committed and green: 3e39da8c567a4f9f9b0066f7ee2ed04684de654e.
Next blocker was SURED.OR at 0x5040. All 0x50d0 compute bytes now lower to
1792 Maxwell words; no further lowering blocker remains.
C committed and green: 6cfa9157938727f4af64425d9e61af421b9472a6.
Reuse existing CAL/RET relocation after proving a closed reachable prefix and
closing self-branch. Graphics consumes through native RET 0x7330, lowers to
3072 words, and builds a 3-function CFG. All original safety tests still pass.
D completed: five-tree, 686-file rerun; table and detailed RESULTS3.md written.

Rejected files: pristine 4, v1 3, Lane G 3, v2 2, v3 0.
Distinct contents: 653; all v3 lower. All 686 filename FNV hashes match.
Tests: 581222 assertions/140 broad cases, 367/13 gather,
6840750/56 frontend-backed. All pass. Final builds: ninja: no work to do.
Reference decoding covers both formerly rejected programs' real output words.
Both programs translate all CFG blocks to raw IR. Optimization stops on missing
compute storage-pointer classification and graphics mesh-topology metadata.
No SPIR-V, full emulator build, or live rendering result is claimed.

Reports: RESULTS3.md, corpus-table-v3.md, corpus-table-v3.json.
Reproduction: corpus3.py, lower-program.cpp, pipeline3.cpp.
Research copy: /home/xana/xana/30-Research/oot-60fps-20261104/codex/.
Detailed logs, maps and binaries: its round3-receipts/ and local build-gather/round3/.
Riskiest assumption: complete classification of native control transfers in
the prefix proof. Known indirect/unknown forms fail closed. CI and live
mesh/resource context plus an in-game A/B remain the supervisor's validation.
