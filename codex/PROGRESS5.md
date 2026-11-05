WAITING FOR MEASURED PROFILE: C complete; A consumer unresolved; B instrumentation ready, speed fix open.

Round 5b resumed in the moved local checkout. Live hostname: ember. No remote
machine contacted, no game started, no push. Branch xana/oot-perf2 was created
from xana/oot-perf f807437ab9 fetched from the lane-p clone.

Commits:
- 17f6ca1a1c: merge Lichii origin/nx2 724aede4ea, PR #89's depth/stencil-copy fix.
- 3149d7e19d: opt-in GPU zone phase after stack sampling, with gate tests.
- Round report/corpus/receipt commit records this status.

A: Michael's V3 haze cure accepted as reported in-game evidence. The exact mask
consumer cannot be identified from the build-75 capture. 6f9367 writes filtered
RGB plus alpha 1; SUST predication does not prove a bloom mask. 9387db writes
packed surface bytes, not proven Vulkan stencil. Section A of RESULTS5.md is
one page with evidence, limits and the specific capture that would close it.
No change to V3 image behavior.

C: complete fresh clang replay of all 935 programs (874 distinct contents).
Pristine: 932 accepted, 3 rejected at the known offsets. V3: 935 accepted,
ZERO rejects. The 932 joint accepts retain identical frame hashes. There are
235 new program files (227 distinct contents) versus captures/raw's original
686-file handheld corpus. All new content lowers in both. Per-file results
and source/driver hashes: codex/docked-corpus5.json.

B: three recorded cherry-pick conflict sites reviewed and compiled. Recompiler
and shader_environment.cpp still match V3. Raw B3h logs contradict its draw
count explanation: roughly 34,700 draws/s, not the roughly 4,000/s verifier
subset. Preparation is about 3.22 ms/frame, not 0.4 ms. This does not change the
11-run measured absence of a reliable fast-draw gain; those paths were left
alone. Warmed present fence wait is about 3.8 ms/frame, composite 0.035 ms.
These overlap producer work and cannot be added as separate frame costs.
No local SAMPLE_PROFILE file or GPU_PROFILE zone output was found, so guest
JIT vs HLE vs command processing cannot be ranked. No CPU optimization before
that ranking. Locked 60 at 1x and headroom for 2x remain UNPROVEN.

The new EDEN_NX2_GPU_PROFILE_AFTER_SAMPLE=1 opt-in supports one launch: clean
stack samples first, then zone timing. Exact env vars, trigger, selected CPU
and host-service thread names, durations and required artifacts: PROFILE5.md.
Default rendering behavior and the existing speed switches are unchanged.

Checks: 581222 assertions / 140 lowering cases; 7 sample-report tests; 8 phase
gate cases. Clang compiled 7 production translation units and 4 fast-path test
sources. Production Windows sampler cross-compiled with MinGW. No full link,
full CI suite, Vulkan validation run or game benchmark is claimed. Receipts in
codex/round5-receipts and build-gather/round5. Original round-5 files preserved;
backups under build-gather/round5/backups. Reports copied to research codex/.

RESUME HERE: supervisor builds xana/oot-perf2 and runs PROFILE5.md on Atlas,
then supplies eden_log.txt, complete nx2_samples_PID.bin and matching exe/PDB
at a local path. Rank the actual top three CPU costs, select a measured change,
run targeted tests and a warmed profile-off A/B. Do not repeat a fast-draw sweep
or call the diagnostic branch a 60 fps fix. Task B is still open.
