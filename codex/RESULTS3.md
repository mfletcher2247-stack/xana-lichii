# Round 3 handoff

READY-TO-PUSH on xana/oot-gather-v3, branched from b16b23fbe7.
V2 still points to b16b23fbe73a8b98c9d8af0256c02c7eb54c54b2.
No push, remote access, other-machine access, or Git configuration edits.

| Tree | Revision | Lowered | Rejected |
|---|---|---:|---:|
| Pristine | c9ebb5c3af | 682 | 4 |
| V1 | 8ca0e4ba1d | 683 | 3 |
| Lane G | 6f08f31dff | 683 | 3 |
| V2 | b16b23fbe7 | 684 | 2 |
| V3 | 6cfa915793 | 686 | 0 |

These are real LowerToMaxwell results on all 686 captured files, using the same
non-geometry, tensor-enabled options as round 2. There are 653 distinct byte
contents. Distinct lower/reject counts: pristine 649/4, v1 650/3, G 650/3,
v2 651/2, v3 653/0. All 686 filename FNV hashes match their bytes.
Per-file results: corpus-table-v3.md and corpus-table-v3.json.

## A: LEA.HI

Commit 8f0d580fc20cdfa97f01e0f7f51cd4e593fbb628.
The captured words at 0x4b50 are:

    01017211 80000000 078ff0ff 000fca00

Opcode 0x211; guard PT; Rd=R1; Ra=R1; Rb=R0; Rc=RZ;
bit 63 negates B; shift bits 75..79 are 30; .HI=1; .X=0; carry-out=PT.
The result is R1 = (R1 >> 2) - R0, modulo 2^32.
The high-address guard previously rejected both B modifier bits 62 and 63.
It now permits B negation, while retaining the unsupported bit-62 rejection.

The lowering keeps the existing high-word SHR/SHF calculation and sets Maxwell
IADD's B-negation bit on the final add. The captured case emits:

    38280000002701fa  SHR R250, R1, 2
    5c1100000007fa01  IADD R1, R250, -R0

Maxwell has LEA.HI, but its offset negation is not native B negation, and its
frontend rejects LEA.HI.X. Keeping the existing expanded lowering avoids either
mismatch. Native .X adds the carry predicate at bits 87..90. Under .X, a modified
B is one's complement, not integer negation; that new combination remains
rejected. Existing unmodified-B .X tests remain green. Immediate bit 63 stays
part of the immediate. Cbuf B is materialized before the subtract. Uniform B
uses uniform_register(), including its existing mirror-allocation retry.

Captured-word tests check the real instruction, cbuf/uniform/register forms,
immediate sign bit and unsupported .X/modifier combinations. Production Maxwell
SHR, SHF.L and IADD translation is evaluated for shifts 0, 1, 15, 30, 31,
zero/nonzero high halves, wraparound, and register/immediate/uniform B.
Encoding and arithmetic were cross-checked against the pre-existing local Mesa
NAK sm70_encode.rs OpLea/OpLeaX and ir.rs fold implementations. No network lookup.

## B: next compute blocker

Commit 3e39da8c567a4f9f9b0066f7ee2ed04684de654e.
After A, the actual next reject was native surface reduction mode at 0x5040:

    0400159f 60008400 0310e000 0001e200

Operation bits 87..89 are 6, OR. The existing case accepted only operation 0,
ADD. Operation 6 is also OR in the existing Maxwell surface-atomic frontend.
The lowerer now accepts ADD or OR under the same U32 direct-descriptor,
buffer/2D, scope, ordering and clamp checks, and carries the operation into
Maxwell SURED bits 24..26. No other reduction mode was enabled.

Tests include all three real OR instructions at 0x5040, 0x5050 and 0x50b0,
their guards, descriptor bank/offset and a rejected modifier. The frontend test
confirms BoundImageAtomicOr32 and descriptor byte offset 0x210, rather than ADD.
The complete compute capture now lowers to 1792 Maxwell words and consumes all
0x50d0 native bytes. Every CFG block translates to raw IR, including three OR
atomics. Optimized translation stops honestly on uncaptured storage-pointer
classification at 0x1c0. No further lowering blocker remains in this program.

## C: call linkage

Commit 6cfa9157938727f4af64425d9e61af421b9472a6.
The real calls are CALL.REL.NOINC with explicit link R2:

| Call PC | Words | Target | Return PC setup |
|---|---|---|---|
| 0x6330 | 00007944 00000bc0 03c00000 000fea00 | 0x6f00 | 0x6290: R2=0x6340 |
| 0x6350 | 00007944 00000dc0 03c00000 000fea00 | 0x7120 | 0x6340: R2=0x6360 |

The helpers return through RET.REL.NODEC R2 at 0x7110 and 0x7330; each relative
base resolves to zero, so R2 holds the original native return PC.
The first setup is nonadjacent. Existing lowering already emits Maxwell CAL,
relocates its target through the expanded instruction map, verifies callee link
preservation and matching RET, then emits Maxwell RET. The Maxwell structured
frontend recursively inlines calls with each call's continuation as the return
label. No CAL/RET encoding or relocation arithmetic needed changing.

The old bounded proof scans all captured bytes. This file has unrelated code
after the first program's closing self-branch, including indirect branches and
a predicated call, which disabled that proof before 0x6330 could use it.
The new gate walks control conservatively from entry, including call targets
and continuations, both conditional successors and reconvergence destinations.
Unknown control or invalid destinations fall back to the entire-capture checks.
It restricts analysis only when the closed reachable region is immediately
followed by canonical unpredicated BRA-to-self. Other captures retain the old
conservative analysis. The actual lower/return/link checks are unchanged.

For this capture, every instruction through 0x7330 is reachable and 0x7340 is
the self-branch boundary. The lowerer consumes 0x7340 of the file's 0x10b50
bytes and produces 3072 Maxwell words. The remaining allocation bytes are not
reachable from this shader's entry. No instruction after the boundary was
reinterpreted as part of the first program.

New tests preserve original PCs and captured setup/call/helper words. They check
the exact relocated CAL displacement, final RET, and rejection of a clobbered
link, setup-bypassing entry, and reachable indirect control. Existing whole-
capture and dead-call safety tests are unchanged and still pass.
The real Maxwell CFG contains three functions, the main program plus both
helpers. All blocks translate to raw IR with a synthetic mesh stage.
Optimized translation stops on missing mesh-output topology metadata. A fragment
stage instead fails on ISBE stage-buffer access, as it should.

Shader role: ISBE reads and per-vertex/primitive attribute and map writes suggest
mesh geometry processing. The two helpers use lane masks, ballots, shuffles and
integer additions, consistent with subgroup aggregation. A precise game-pass
name, including lighting or fog, cannot be established without stage metadata
and frame/resource context. The synthetic mesh stage is an explicit probe
assumption, not captured metadata.

## D: final validation and receipts

Commands and actual output, on the final source revision:

    cmake --build build-gather/tests --parallel 3
    ninja: no work to do.
    cmake --build build-gather/round2/frontend-build --parallel 3
    ninja: no work to do.
    build-gather/tests/gather-tests
    All tests passed (581222 assertions in 140 test cases).
    build-gather/tests/gather-tests '[gather]'
    All tests passed (367 assertions in 13 test cases).
    build-gather/round2/frontend-build/ampere-extra-tests
    All tests passed (6840750 assertions in 56 test cases).
    python3 codex/corpus3.py
    baseline {'ok': 682, 'reject': 4}
    v1 {'ok': 683, 'reject': 3}
    g {'ok': 683, 'reject': 3}
    v2 {'ok': 684, 'reject': 2}
    v3 {'ok': 686}

Both newly accepted outputs also reference-decode all 1792 and 3072 real words.
The reference disassembler appends a synthetic zero; that last row is excluded.
Git diff --check is empty. New lines and all round-3 reports are ASCII.
The final driver can be rebuilt locally with:

    c++ -std=c++20 -O2 -Isrc -Iexternals/nlohmann/json/include codex/lower-program.cpp -o build-gather/round3/lower-v3-c

Receipts are in build-gather/round3 locally and the research workspace's
codex/round3-receipts. corpus3.py and lower-program.cpp preserve the driver;
pipeline3.cpp preserves the raw-IR probe. Existing locally built frontend
libraries and dependency caches from round 2 were reused. These checks do not
constitute a full emulator build, SPIR-V compilation or a game-rendering test.

Devil's-advocate passed: the broader prefix trial was narrowed to preserve all
existing safety tests, captured PCs are retained in linkage tests, and every
claim above is separated from missing runtime context. The riskiest assumption
is that the entry-rooted control classification fully captures every native
transfer relevant to the bounded proof. Known indirect/unknown control fails
closed; no hardware/rendered comparison was possible under the local-only task.
The supervisor still needs CI and an in-game A/B with real mesh/resource state.
