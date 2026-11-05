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
