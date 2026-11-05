# Round 4: docked bloom investigation

READY-TO-PUSH: diagnostics and ranked candidates only. Bloom is not fixed.

Branch: xana/oot-bloom-fix, created at b94eb6513c. V3 remains at that revision.
No push. No other machine was accessed. No production source or settings changed.
Investigation performed locally on 2026-11-04, starting at 21:02 MST.

## Result

No wrong arithmetic result explaining the docked haze was reproduced. The fallback
handoff is a ranked list of actual captured programs, numeric probes, and one
instrumented docked capture plan. A speculative fog multiplier would hide the
failure without identifying it, so none was added.

Michael's screenshot shows a lit scene and severe bright-surface washout. PR #87
reports excessive volumetric lighting, but that report is not a proof that this
screenshot has the same underlying cause. The 686 supplied programs were captured
in handheld mode on build 09 with a black world. Docked variants can be absent.

## What was tested, in the requested order

### 1. Precision, half arithmetic, saturation and exponent operations

REPRODUCED checks that pass:

- The entire 686-file corpus contains zero H0_NH1 instructions. The captured half
  add/multiply/FMA instructions also have DNZ, SAT and FTZ clear. This substantially
  lowers those particular modifier hypotheses for these captures, not for unseen
  docked programs.
- The existing texture FP16 component-order correction is already in v3. Its
  F2F/ISCADD sequence packs the first component into the low half. Existing gather
  tests exercise that sequence. No duplicate upstream port was made.
- A synthetic F2F.F16.F32 probe reaches the real Maxwell translator and preserves
  nearest-even ties, signed zero, the largest finite half, a smallest half
  subnormal, and a cleared upper half.
- A captured F2FP.PACK_AB at 2f5791cea2a85397:0x1b20 retains low/high
  component order and a nearest-even tie while its destination aliases an input.
- Captured HADD2 at 6f9367bd41dc0b58:0x2f40 broadcasts the selected low half
  without reading the deliberately poisoned high half.
- Real luminance-weighted HMUL2 at 6f9367bd41dc0b58:0x510 and HFMA2 at 0x570
  preserve independent mixed-sign lanes for the tested inputs. The coefficients
  are 0.0721435546875 and 0.2125244140625; green's coefficient is 0.71484375.
- Synthetic FMNMX probes preserve default numeric-operand behavior when one
  input is NaN and propagation with .NAN, for both minimum and maximum.
- Real volume FMUL.FTZ.SAT at ec68e40eb783ff32:0x480 clamps negative and greater
  than one inputs. There is an FPSaturate32 in production IR; SAT was not dropped.
- Real volume MUFU.LG2 at 0x4a0 gives log2(abs(-0.25)) = -2.
- Real volume MUFU.EX2 at 0x590 gives exp2(-x), rather than exp2(x), for x from
  zero through 16. Its sign modifier survives lowering.

The captured words for those volume checks are:

    0x480: 18187220 00000013 00412000 080fe200
    0x4a0: 00187308 40000018 00000c00 000e7000
    0x590: 000f7308 8000000a 00000800 000e2200

The H0_NH1 claim received an additional hardware test, not just a source comparison.
A one-thread CUDA kernel on this local SM86 GPU executes one HFMA2. A copy of its
cubin changes only instruction bit 86. Inputs are A=(2,3), B=(4,5), C=(0,0).
Actual output:

    plain 0x4b804800 (8.0, 15.0)
    H0_NH1 0x4a00c900 (-10.0, 12.0)

The production Maxwell translation of the synthetic H0_NH1 word also produces
0x4a00c900. PR #87's final change is correct for this case. Reversing it would
introduce an arithmetic bug.

References: Mesa NAK sm70_encode.rs OpHAdd2/OpHMul2/OpHFma2, OpF2F/OpF2FP;
NAK from_nir.rs reverses the two F2FP inputs when packing a low/high NIR pair;
and the repository's Maxwell half helpers, F2F and MUFU implementations.
The local reference copies are /tmp/mesa-nak and /tmp/mesa-sparse.

Limits: bloom-numeric.cpp evaluates production raw IR on the CPU. It does not
execute generated SPIR-V, reproduce texture sampling or control flow, implement
per-operation FTZ, validate every rounding/NaN case, or prove Vulkan driver SFU
accuracy. Its SFU values are ordinary finite values, not precision/range torture
coverage. The separate H0_NH1 hardware probe executes native SASS, not SPIR-V.

### 2. Exposure and reductions

REPRODUCED: captured RED.E.ADD.F32.FTZ.RN at
70cc863d5e1a6627:0xba0 lowers through the real RED translator to a float atomic
add, retaining both halves of the pointer. At address 0x1212340000 it computes
1.5 + 0.25 = 1.75 in the raw-IR probe. It does not become an integer add or a
32-bit-addressed operation.

The census finds 23 F32 global reductions, five REDUX.SUM instructions and no
ATOMS shared-atomic instructions in the 100 compute captures. None of these
counts proves which shader controls exposure. For example, 8475d671e6967428's
REDUX.SUM counts POPC results and reserves output slots with ATOMG, which looks
like compaction, not mean scene luminance. Calling it exposure would be guessing.

The F32 backend uses a CAS loop, not a 64-bit integer atomic. Runtime buffer
visibility, allocation size and binding continuity remain untested. PR #87's
review raises a still-relevant native writable-range continuity concern in
StorageBufferBinding: IsFullyMappedRange does not prove device continuity. That
is a resource-binding candidate, not a reproduced bloom cause.

### 3. Formats and gamma

REPRODUCED: the production texture-format lookup maps FLOAT RGBA16, RG16 and
R11G11B10 descriptors to R16G16B16A16_FLOAT, R16G16_FLOAT and B10G11R11_FLOAT.
Source inspection follows those to Vulkan SFLOAT/UFLOAT formats. The render
format mapping also keeps the corresponding float types. No general widening
to UNORM or SRGB was found along these standard paths.

UNPROVEN: the actual docked resource descriptors, aliasing and view formats.
Instruction-only captures do not contain them. A valid generic mapping does not
prove the game received the right resource view. Float frame-dump PNGs are
preview-encoded; judge the raw data and manifest statistics, not PNG whiteness.
An HDR value above one is normal and alone is not evidence of overexposure.

The setting named fix_bloom_effects is not a proven solution. Its 0.98 sample
multiplier is Android-only. On desktop its remaining path affects certain
D32-to-color conversions, not general fog intensity.

### 4. HLE and presentation

SOURCE INSPECTION ONLY: vi2 replies to QueryDisplayHdrStatus with {1,0,400} and
QueryDisplayHdrPaperWhite with {203}. Neither reply depends on docked mode.
GetBufferMaxResolution and the handheld crop do depend on docked mode. There
is no proven HLE gamma multiplier here. The HDR comments contain conflicting
older wording, so the game SDK's interpretation is not established by comments.
Lane I's claim that this tree has no HDR query answer is contradicted by the
current nx2_gsv.h. HDR output support is a separate question.

There is one cheap presentation control: the supplied screenshot says FX.
MainWindow::UpdatePostShaderText shows that when post_shader_enabled is true
and no named preset is active. A custom unnamed chain can still exist; the
screenshot does not prove that it does. An empty chain is a no-op. For the next
capture toggle it to NO FX and observe the image in the same run. This can
confirm or exclude a host-effect contribution without changing shaders.

## Captured programs worth testing

These are HYPOTHESES. Names describe structure, not confirmed game pass names.
Rank is relative to the unexplained fog after the tested arithmetic survived.

| Rank | Native program key | Evidence and discriminating check |
|---|---|---|
| 1 | d9b1657f3a0b4bc2 and e883414e8c9be33b | Large FP32 3D-volume writers, 10 and 6 SUST stores; many EX2/LG2 calls, clamps and lighting-like constants. Compare their inputs and outputs before the integration pass. Inspect constants/resources or an untested FP32 instruction if these outputs first become unreasonable. |
| 2 | ec68e40eb783ff32 | Clear slice integration: 3D TEX, accumulated optical depth, exp2 of negative depth, three 3D outputs and a z loop. If inputs are reasonable but integrated light is excessive, investigate coordinate mapping, sampling, loop/control or constants here. |
| 3 | 6f9367bd41dc0b58; also F32 accumulator group in census | RGB luminance weights, FP16 gathers, shared-memory stencil and barrier in 6f9367. It was rejected on the original build; whether it affects exposure is unknown. Identify its actual consumer and readbacks before calling it auto-exposure. |
| 4 | Actual sampled/storage view formats, shared by the above | Generic float mappings pass, but actual descriptors are absent. Check manifest format/address continuity across producer and consumer, and distinguish legitimate normalized occupancy masks from HDR lighting volumes. |
| 5 | vi2/host presentation | Lower confidence because status/paper-white replies are mode-independent. Toggle FX off; compare raw present data with the visible window. A bright raw present excludes a purely host-effect explanation. |

The fourth 3D writer, 0c546119f1c74fbd, has integer masks and neighbor fetches,
with two stores. It looks like occupancy/mask dilation rather than light
integration. Include it when tracing the large writers' sampled inputs.
The small integration kernel uses constants c[3][0x180], [0x184], [0x18c].
The large writers use the corresponding parameter region in c[5].

## One instrumented docked run

Use build-75 or a build from v3. There is no new production fix to build in this
branch. Preserve the save and use the same room and camera as the screenshot.
Keep docked and 1x. In a temporary test configuration use GPU accuracy High,
reactive flushing on, asynchronous GPU emulation off, asynchronous shaders off,
frame generation off, disk shader cache off, and host effects off (NO FX).
Do not delete existing caches. Disabling disk cache makes the diagnostic
lowering/reject switches take effect rather than loading an older pipeline.

Launch the emulator from PowerShell with these variables, choosing fresh output
directories if C:/oot-r4 already holds a previous capture:

    $env:EDEN_NX2_SHADER_DUMP = 'C:/oot-r4/shaders'
    $env:EDEN_NX2_FRAME_DUMP = 'C:/oot-r4/frame'
    $env:EDEN_NX2_FRAME_DUMP_AT = 'file'
    $env:EDEN_NX2_FRAME_DUMP_SAMPLED = '1'
    $env:EDEN_NX2_IPC_TRACE = '1'
    New-Item -ItemType Directory -Force C:/oot-r4/frame,C:/oot-r4/shaders

Start the chosen lichii.exe from that same console. Load the save normally.
Once the haze is visible, make the trigger file:

    New-Item -ItemType File -Force C:/oot-r4/frame/capture.now

Wait for "Frame dump done", let one more frame present, and exit normally.
Keep eden_log.txt, manifest.jsonl, the .raw.zst files and shader/header dumps.
Check that the frame summary says truncated=false. Generate an index with:

    python3 -I tools/nx2/frame_dump_index.py <captured-frame-directory>

In that one capture:

1. Check whether NO FX changes the visible haze. If the raw present is already
   washed out, trace backward through the game's passes rather than host FX.
2. Locate the 3D writer/integrator shaders by the dumped native binaries and
   their structure. Use the manifest's dispatch hashes to match their sampled
   inputs and storage outputs by GPU address and dimensions. The old filename
   keys may be absent in docked mode; that is useful evidence, not a failed run.
3. Find the first producer whose output is suspect relative to its inputs and
   the scattering/integration equations. Preserve raw values and NaN/Inf counts.
   Sampled inputs are deduplicated by shader/address/mip, so a repeatedly reused
   image may lack a per-dispatch before-image. The dump is not a full replay.
4. Trace luminance/exposure consumers and check for a missing dispatch or a stale
   value, then check the actual view formats. IPC identifies the display queries;
   it does not by itself dump their reply payloads.

That run can localize the broken stage and collect the missing docked programs.
It cannot guarantee proof of an instruction bug: scalar constants, SSBO contents,
raw TIC descriptors and a known-correct console comparison are not all captured
by these switches. If those are required, state the missing evidence instead of
calling a dispatch's bright output a confirmed miscompile.

### Optional one-run ablations, one candidate per launch

Keep the same settings and use a different dump directory for each. Select only
one of the following EDEN_NX2_REJECT_HASH values before launch:

    d9b1657f3a0b4bc2,e883414e8c9be33b
    ec68e40eb783ff32
    6f9367bd41dc0b58

The first removes both captured lighting-volume variants, the second the captured
integrator, the third the captured luminance/stencil kernel. A valid ablation
must log "rejected by EDEN_NX2_REJECT_HASH". If it does not, no matching program
was compiled: the docked path may use another key, or a cache was reused. Use the
new docked dump's exact keys and repeat only if needed. Do not use the old GPU
addresses, which drift between launches. Do not include "0x", spaces or short keys.

A matching rejection that removes haze only localizes a dependency. It can also
remove legitimate lighting, leave stale images or break the frame. It does not
prove the rejected shader has wrong arithmetic. These are diagnostic runs, not
recommended gameplay settings. Clear the reject variable afterward.

## Upstream survey

Read PR #87 body, comments, both inline review comments, and its final H0_NH1 diff:
https://github.com/zortos293/Lichii/pull/87

Added/fetched Djihads80/Eglantine as upstream; inspected nx2, open/closed PR and
issue searches and related remote branches. upstream/nx2 is e736dfde9e. These
histories diverge (132 commits unique to HEAD, 140 unique to upstream); neither
is an ancestor of the other. This is not a simple behind-upstream fast-forward.
No wholesale merge was made into the requested v3-based investigation branch.
No new production fix was written.

Relevant diffs inspected:

- 77952c8d8c / Lichii 6c1dd4e92: H0_NH1, already present, hardware check passes.
- scratch/oot-shader-lab 29ed33831a: FP16 texture result order, already present
  through Lichii's earlier port. No duplicate change.
- scratch/oot-shader-lab 5c7da2713a: refresh 64 KiB graphics uniform banks. This
  is not a compute-bank fix and does not prove a fog cause. Not ported blindly.
- change-fix-bloom-toggle 8ffa544b75: old image-emission change. The Windows
  branch already lacks the Android sample multiplier. Not a missing fog fix.

## Validation and reproduction

Commands and actual outputs are in round4-receipts. In this checkout:

    cmake --build build-gather/tests --parallel 3
    ninja: no work to do.
    build-gather/tests/gather-tests
    All tests passed (581222 assertions in 140 test cases)

    cmake --build build-gather/round2/frontend-build --parallel 3
    ninja: no work to do.
    build-gather/round2/frontend-build/ampere-extra-tests
    All tests passed (6840750 assertions in 56 test cases)

    python3 -m unittest discover -s tools/nx2 -p 'test_*.py'
    Ran 24 tests
    OK

Fresh corpus replay:

    python3 codex/bloom-census.py <captures/raw> build-gather/round4/census \
        --lower build-gather/round4/lower-v4
    Filename hashes verified: 686
    Compute captures disassembled: 100
    H0_NH1 occurrences: 0
    Lowering: {'ok': 686}

All eleven numeric probe groups pass with both GCC and clang-18. See round4/README.md for exact build/run
commands and round4-receipts/numeric.log for output. A fresh clang-18/Ninja build of the broad standalone suite also passes
581222 assertions in 140 cases (clang-build.log and clang-tests.log).
The optional hardware probe also passes, with its raw output in round4-receipts/hardware.log.
The census script retains complete SASS and per-file lowering results locally;
the committed census-summary.json retains compute operation inventories.

No full emulator build, full project-wide test suite, generated-SPIR-V execution
or game rendering test was performed. Existing standalone lowering/frontend
suites passed. No shader-sweep CLI binary is available in this checkout's local
builds; the Python utility tests are not a replacement for that binary's suite.

Devil's-advocate passed: the strongest tempting culprit, H0_NH1, was checked on
hardware instead of blamed from its commit message. Passing raw-IR probes are
not promoted to Vulkan correctness or a bloom fix. Docked programs/resources
remain the missing evidence. No brightness compensation was committed.
