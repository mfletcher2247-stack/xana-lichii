READY-TO-PUSH

Branch xana/oot-gather-v3 starts at b16b23fbe7. V2 is unchanged.
A: captured 0x4b50 = 01017211 80000000 078ff0ff 000fca00.
LEA.HI R1, R1, -R0, RZ, 30; no carry input/output, no uniform operand.
Guard rejects bit 63, the B operand negation. Local Mesa NAK sm70_encode.rs
and ir.rs distinguish integer negation from bitwise complement under .X.
Plan: apply non-X B negation at the final Maxwell IADD, test captured words,
all shifts and operand forms, then run the whole program and existing suites.
B and C pending. No remote or other-machine access; no push.

A is implemented and green. Local regression receipts:
cmake --build build-gather/tests --parallel 3 -> linked gather-tests.
build-gather/tests/gather-tests -> All tests passed (581202 assertions in 137 test cases).
build-gather/tests/gather-tests '[gather]' -> All tests passed (367 assertions in 13 test cases).
cmake --build build-gather/round2/frontend-build --parallel 3 -> linked ampere-extra-tests.
build-gather/round2/frontend-build/ampere-extra-tests -> All tests passed (6840742 assertions in 55 test cases).
Frontend tests evaluate real Maxwell SHR, SHF.L and IADD IR over register,
immediate and uniform B forms, zero/high pairs, shifts 0/1/15/30/31 and wraparound.
Captured cbuf modifier and .X fail-closed cases are checked structurally.

Whole compute program now passes 0x4b50 and rejects at 0x5040:
native surface reduction mode, words 0400159f 60008400 0310e000 0001e200.
B pending: operation field is 6 (OR), whereas existing SURED only accepts ADD.
C pending: calls at 0x6330/0x6350 target 0x6f00/0x7120 with link R2.
First setup is at 0x6290; later unrelated capture tail disables the global proof.

A commit: 8f0d580fc20cdfa97f01e0f7f51cd4e593fbb628. Ready intermediate.
B now lowers the full 0x2290 compute capture to 1792 Maxwell words.
SURED operation 6 is OR in local Mesa and the existing Maxwell frontend.
Only ADD and OR pass the U32 direct-descriptor buffer/2D reduction guard.
Captured words at 0x5040/0x5050/0x50b0 have register/predicate/descriptor tests.
The frontend test confirms BoundImageAtomicOr32, not ADD, at descriptor 0x210.

B validation:
build-gather/tests/gather-tests -> All tests passed (581214 assertions in 138 test cases).
build-gather/round2/frontend-build/ampere-extra-tests -> All tests passed (6840750 assertions in 56 test cases).
Whole compute CFG constructed and all CFG blocks translated to raw IR, including
three BoundImageAtomicOr32 operations. Optimized translation correctly stops on
missing storage-pointer context at 0x1c0. No SPIR-V or rendered result claimed.
C continues. No push or remote access.

B commit: 3e39da8c56. C trial: entire graphics capture lowers to 3072 words.
The broad prefix analysis changed old conservative regression expectations.
Narrowed the new gate to require a closed entry-rooted control walk AND the
canonical unpredicated BRA-to-self immediately after its maximum reachable PC.
The real capture has that marker at 0x7340. Existing tests without it keep all
whole-capture checks. Rebuilding and re-running the unchanged existing suites.
Trial corpus counts: baseline 4, v1 3, G 3, v2 2, v3 0 rejected of 686.
These trial counts will be rerun after final C validation.

C is green. Both native CALL.REL.NOINC sites use link R2 and the existing
Maxwell CAL relocation plus RET lowering. Native targets are 0x6f00 and 0x7120;
return-PC setup is at 0x6290 (for 0x6340) and 0x6340 (for 0x6360).
No call or return encoding changed. A conservative reachable-prefix analysis
prevents unrelated post-boundary code from disabling the existing linkage proof.
All earlier conservative regression tests remain unchanged and pass.
New captured-word tests keep original PCs and verify CAL displacement, RET,
link clobber, bypassing setup and reachable indirect-control rejection.

Final C validation:
build-gather/tests/gather-tests -> All tests passed (581222 assertions in 140 test cases).
build-gather/round2/frontend-build/ampere-extra-tests -> All tests passed (6840750 assertions in 56 test cases).
build-gather/tests/gather-tests '[gather]' -> All tests passed (367 assertions in 13 test cases).
python3 codex/corpus3.py -> baseline 682/4, v1 683/3, G 683/3, v2 684/2, v3 686/0 (lower/reject).
Both newly accepted outputs reference-decode: 1792 compute words, 3072 graphics.
The reference disassembler appends a synthetic zero word; exclude that final row.
Graphics CFG has 3 functions; all blocks translate to raw IR with a synthetic
mesh stage. Optimization stops on missing live mesh-output topology metadata.
ISBE attribute/map writes and shuffle/add helpers suggest mesh geometry work;
a lighting or fog classification is not established.
D documentation and receipt packaging in progress. No push.
