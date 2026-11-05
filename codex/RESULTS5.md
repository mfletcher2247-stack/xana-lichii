# Round 5b: docked haze, CPU floor and corpus

STATUS: PARTIAL. Docked lowering replay passed. Haze cure accepted from Michael's
A/B. Exact consumer and performance ranking remain open pending runtime evidence.
Branch: xana/oot-perf2, based on xana/oot-perf f807437ab9. Local only, no push.

## A: haze mechanism, bounded to what the capture proves

Michael's docked A/B overrides H5's negative verdict: V3 restores contrast and
shadows with zero rejects; build-75 is hazy. This is a reported gameplay result,
not a new gameplay test here. It implicates the changed shader behavior as a set.
It does not isolate which of the three formerly rejected programs is causal.
Rejecting 39110c9db5ac83a4 on build-75 without removing haze also defeats H6's
claim that this one bloom pass alone explains the defect. Rejecting a producer
can leave an old target behind; rejection is not equivalent to clearing bloom.

The supplied SASS does not justify calling both compute outputs bloom masks:

- 6f9367, 0x340: RGB gathers feed half-precision luminance weights at 0x510-0x5f0,
  shared-memory filtering and coefficient loads. Near 0x4590-0x4ff0 the stores
  convert half components to floating-point R/G/B and set the fourth component
  to 1.0. The 16 SUST.P.2D stores use surface slot 0x34. Their predicates include
  comparisons against tile/output bounds. This is a filtered color producer;
  neither predication nor SUST proves an eligibility mask or bright-pass gate.
- 9387db, 0x2290: near 0x4f40-0x50b0, weights are scaled by 255, converted to
  integers, packed into byte lanes and ORed into surface slot 0x84. This proves
  a packed surface/coverage-like output, not a Vulkan stencil attachment.
- d5e90d: depth-compare gathers can change shadows directly. Therefore deeper
  shadows in V3 do not by themselves prove a missing bloom-mask dependency.

H5/H6 establish the visible path: scene -> bloom pyramid -> composite, with the
light-shaft volume built after bloom. H6 measures L0 as roughly 0.098 times the
whole scene and the final pyramid as about 0.218 mean luma. That describes the
hazy build's values. It does not prove a threshold is required: threshold-free
HDR bloom is also possible. H5's missing-producer test only covers executed,
dumped image resources. It cannot exonerate rejected programs, storage buffers,
constant-buffer controls, depth/stencil state or preserved targets.

Answer to the named consumer question: UNRESOLVED. No captured binding ties
surface slots 0x34/0x84 to a bright-pass, bloom gate, light-shaft gate or stencil
consumer. The capture was made with those dispatches rejected, so their output
records are absent. The final composite's three texture inputs have producers,
but that does not identify the skipped programs' downstream use. Saying their
absence makes bloom sample every pixel would invent the missing dependency.

The closing test is a same-camera V3 sampled frame plus bound surface descriptors
and depth/stencil/constant-buffer state, followed by rejecting each formerly
failed key separately with targets initialized identically. Match the writer's
resolved GPU address and subresource to its first reader and compare it to
build-75. V3 itself is already the working fix; leave its image behavior alone.

## B: performance evidence and code

The objective remains locked 60 at docked 1x, then 2x headroom. This round has
not demonstrated either. The top three actual CPU costs cannot be named from
the supplied logs: no GPU_PROFILE zone report or nx2_samples file was found.
P explicitly says it could not distinguish guest JIT from GPU-thread cost.
J's warmed 60 fps segment is superseded by the corrected B3 docked runs.
As requested, no speculative CPU optimization is being written before ranking.
The code change below is measurement infrastructure only.

### Important correction inside B3

Raw eden_b3h.txt at 150.131 s says 34,686 draws/s, 4.95 us preparation per draw,
and only 4,058 verifier checks/s. Verifier checks are a subset, not the draw
count. The raw count implies about 653 draws/frame at 53.26 fps and about
3.22 ms/frame of preparation, not 76 draws and 0.4 ms. This does NOT overturn
the 11-run measured result: the switch union still has no reliable speed gain.
Do not pursue fast-draw on that arithmetic alone.

At 151.549 s the same log says 18,777 us/frame, 3,756 us waiting for the GPU
fence, 35 us composite and 0 us vblank pacing. These are present-thread wall
times. The fence can overlap GPU-thread draw preparation; adding them as
independent frame costs would double-count work. About 15 ms lies outside the
present call, but this log cannot split it into JIT, HLE, command processing
and other waits. A fence wait is not proof that the physical GPU is saturated.

Three measured quantities, NOT the requested global top-three ranking:

| Quantity | B3h warmed sample | Interpretation |
|---|---:|---|
| Present fence wait | 3.756 ms/frame | Waiting for submitted frame progress |
| Full draw preparation | about 3.22 ms/frame | Host wall time, may overlap fence wait |
| Present composite | 0.035 ms/frame | Too small to explain the floor |

### Branch and review

Local commits: 17f6ca1a1c merges the Lichii correction; 3149d7e19d adds the
measurement phase. This report and the replay have a separate docs commit.

Fetched xana/oot-perf from the lane-p clone. Checked the three reported conflict
resolutions: sampled-frame collection remains before delta-view bookkeeping;
delta eligibility uses DeltaBlocker::None; the rasterizer keeps the current
frame_dump(device, memory_allocator, scheduler) constructor and initializes
dynamic_state_flags. Recompiler sources and shader_environment.cpp are identical
to V3. FAST_DRAW=2 remains excluded from the recipe. This is a source review,
not a proof that every fast path is correct in all games.

Both upstreams were checked. Current Lichii origin/nx2 is 724aede4ea, one commit
ahead of the shared build-75 base. Its [PR #89](https://github.com/zortos293/Lichii/pull/89) depth/stencil-copy correction was
read and merged without conflict. It fixes invalid aspect/layout use; it is
not claimed to improve FPS or to explain this haze. Eglantine upstream/nx2 is
a different development line with 140 nonancestor commits and conflicts across
shader_environment, compute, scheduler and buffer cache. An automatic merge
would discard the controlled V3 baseline. Its [PR #609](https://github.com/Djihads80/Eglantine/pull/609) changes native storage
sizing and MKW pacing, not a demonstrated OoT CPU bottleneck. [PR #614](https://github.com/Djihads80/Eglantine/pull/614) is shader
compatibility work, not a measured fix for this floor. Neither was blindly ported.

### One-launch measurement change

Previously SAMPLE_PROFILE disabled GPU_PROFILE zone timers for the entire
process, even after sampling ended. Setting SAMPLE_PROFILE=1 means 1 Hz, not a
boolean enable, and default sampled names omit all guest cores and HLE services.

New opt-in EDEN_NX2_GPU_PROFILE_AFTER_SAMPLE=1 lets zone timers begin only after
the sample file closes. Set GPU_PROFILE as well. The latch only enables once;
no active timed scope is disabled or reset across phases. AddDrain ignores a
zero start timestamp if a wait began before timers became enabled. Failed or
unfinished sampling leaves the zone phase off. Default rendering and all
fast-draw choices stay as inherited. See codex/PROFILE5.md for the exact recipe.

## C: complete docked lowering replay

Rebuilt both lowering drivers with clang++-18 -std=c++20 -O2 from the round-3
lower-program.cpp. Verified every frozen pristine header against c9ebb5c3
(build-75): no differences. V3 recompiler sources match b94eb6513c exactly.
The generic non-geometry lowering probe enables tensor emulation, as in prior
rounds. It does not execute the programs or establish full SPIR-V compilation.

- 935 native program files: 830 graphics with header sidecars, 105 compute;
  874 distinct byte contents. Sidecars are metadata, not counted as programs.
- Pristine: 932 accepted, 3 rejected. 6f9367 rejects at 0x340; 9387db at 0x2290;
  d5e90d at 0x1650. Their reasons match the captured log.
- V3: 935 accepted, ZERO rejects, including all three known failures.
- The 932 joint accepts have identical reconstructed frame hashes in both
  trees. Only the three previously rejected programs gain executable output.
- Against the original 686-file handheld corpus (captures/raw), docked adds
  235 program files with 227 distinct contents. All lower in pristine and V3.
  An exploratory comparison against raw_b10 used the wrong handheld subset;
  its new-docked folder is quarantined under build-gather/round5 and excluded.

Per-program input SHA256, native key, status, error offset, lowered word count,
frame hash, driver SHA256 and source provenance are in codex/docked-corpus5.json.
The replay script remains codex/docked-corpus5.py. Fresh output is retained in
build-gather/round5/{pristine,v3,new-docked}.

## Validation and remaining gates

Local checks, with actual output saved under codex/round5-receipts and
build-gather/round5:

    build-gather/tests-resume/gather-tests
    All tests passed (581222 assertions in 140 test cases)

    python3 -m unittest discover -s tools/nx2 -p test_sample_report.py -v
    Ran 7 tests in 0.006s
    OK

    python3 codex/test-profile5.py build-gather/round5/profile-phase-test
    8/8 phase-gate cases passed

The standalone phase test uses the actual zone gate with a controllable sample
completion signal; it checks off/default, sampler zero, sampler-only suppression,
opt-in handoff and zero-start drain behavior. It does not execute Windows thread
suspension. The production Windows sampler cross-compiles with MinGW, and clang
compiles the changed Linux sampler plus GPU split/thread, scheduler, both conflict
sites, upstream texture-cache correction and all four inherited fast-path test
sources. Test-source compilation is not test execution. This is not a full
emulator link, full CI test run, Vulkan validation run or gameplay benchmark.

Devil's-advocate flagged: the exact haze consumer and global CPU ranking still
lack runtime evidence. Scope is explicit; the replay and diagnostic tests are
local proofs, not a 60 fps result. All operations used this checkout, whose live hostname is ember. No remote
machine was contacted and no game was started.
The supervisor must supply measured stacks and GPU zones before a CPU win can
be selected or any 60 fps / 2x performance claim can be made.


### Reproduction and handoff

The phase harness is built with:

    clang++-18 -std=c++20 -O2 -pthread -Isrc \
      -I/home/xana/.cache/uv/archive-v0/BXcRHo_rqOMlqymp/torch/include \
      codex/profile-phase5.cpp -o build-gather/round5/profile-phase-test

Fresh replay drivers:

    clang++-18 -std=c++20 -O2 -Isrc -Iexternals/nlohmann/json/include \
      codex/lower-program.cpp -o build-gather/round5/lower-v3
    clang++-18 -std=c++20 -O2 -Ibuild-gather/round5/pristine-source/src \
      -Isrc -Iexternals/nlohmann/json/include codex/lower-program.cpp \
      -o build-gather/round5/lower-pristine
    python3 codex/docked-corpus5.py \
      /home/xana/xana/30-Research/oot-60fps-20261104/captures-docked/20261104-212843/shaders \
      /home/xana/xana/30-Research/oot-60fps-20261104/captures/raw \
      build-gather/round5 codex/docked-corpus5.json

The frozen pristine-source headers are the c9ebb5c3 versions of the Ampere
headers used by lower-program.cpp. cityhash.so is the small existing wrapper
in build-gather/round5/cityhash-wrapper.cpp linked with src/common/cityhash.cpp.
The script's last argument is a generated report; the recorded report also has
source provenance added after replay.

Next step: supervisor builds perf2 and runs PROFILE5.md once, then supplies the
local log, complete sample file and matching exe/PDB. Resume at the measured
ranking, not another fast-draw sweep. Optimize the largest demonstrated CPU
cost, run its targeted regression tests, then compare warmed profile-off runs
at 1x. Until those artifacts exist, Task B's actual performance fix is open.
