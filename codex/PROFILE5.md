# One-launch docked CPU profile for the supervisor

Use xana/oot-perf2, not shelf 18: the opt-in second phase is new. Keep the
matching lichii.exe and lichii.pdb together. This recipe has not been run on
Atlas by Codex. It writes no settings and makes no changes on another machine.

Keep Michael's existing docked 1x per-game settings, async GPU ON, the warmed
pipeline cache, the same save and camera in Link's treehouse. Keep async shader,
reactive flushing, CPU accuracy and driver settings fixed. Do not clear caches,
change resolution or tour other areas during measurement. A cold load is not a
baseline. Keep FAST_DRAW at 1. Do not use FAST_DRAW=2 or verifier timings.

Set these environment variables in the PowerShell process that launches the
new build. Avoid inherited dump/tracing experiments; use a fresh shell with
only this recipe. The sample rate is a number in Hz, not a boolean.

```powershell
$env:EDEN_NX2_FAST_DRAW = '1'
$env:EDEN_NX2_BUFFER_SKIP = '1'
$env:EDEN_NX2_INLINE_INVALIDATE = '1'
$env:EDEN_NX2_DYNAMIC_SKIP = '1'
$env:EDEN_NX2_TRANSCODE_THREADS = '2'
$env:EDEN_NX2_DRAW_RUNS = '1'
$env:EDEN_NX2_GPU_PROFILE = '1'
$env:EDEN_NX2_GPU_PROFILE_AFTER_SAMPLE = '1'
$env:EDEN_NX2_SAMPLE_PROFILE = '250'
$env:EDEN_NX2_SAMPLE_DELAY = 'file'
$env:EDEN_NX2_SAMPLE_SECONDS = '30'
$env:EDEN_NX2_SAMPLE_THREADS = 'GPU,GPUBack,VulkanWorker,CPUCore_*,CPUThread,nvservices,vi,FS,HostTiming'
$env:EDEN_DKB_FRAME_TRACE = '1'
```

Log filter: `*:Info`. This retains HW.GPU, Render.Vulkan, Service and Common
Info. The separate gpu_log_level setting is not needed for these LOG_INFO
lines; leave settings fixed. This is a diagnostic launch with overhead, not a
final FPS benchmark. The five existing speed switches are held constant.

1. Launch the new build from this shell using the normal game path. Load the
   same save manually. Let it settle, idle, for at least 120 s after loading,
   with no remaining runtime pipeline-build bursts. Do not automate pad edits.
2. Check the log for `Sample profile: starts once ...sample.now exists`. Once
   settled, create the trigger at exactly that logged path, normally:

   ```powershell
   New-Item -ItemType File -Force -Path (Join-Path $env:APPDATA 'eden\log\sample.now') | Out-Null
   ```

   A portable user folder can use a different log path. Use the logged one.
3. Stay on the same camera for the 30 s sample, then at least 45 s longer. The
   log must show `Sample profile: ... samples of ... threads at 250 Hz written
   to ...`, followed by at least three ten-second `GPU thread per second` AND
   `GPU back thread per second` reports. Sampling disables zones until its file
   is closed; the first Report on each thread starts a fresh zone window.
4. Close normally. Preserve the full eden_log.txt, nx2_samples_<pid>.bin, exe,
   matching PDB, actual branch/build label and effective per-game settings.
   Verify that all six CPUCore threads, GPU, GPUBack, VulkanWorker, nvservices
   and vi occur in the parsed sample file. No sample or zone report means an
   incomplete run, regardless of the FPS display. The file must have its end
   record. Do not mix addresses with a PDB from a different build.

Analyze on Anvil after the supervisor copies artifacts locally:

```bash
python3 -I tools/nx2/sample_report.py /path/nx2_samples_PID.bin \
  --exe /path/lichii.exe --folded /path/stacks.txt --svg /path/stacks.svg
```

With llvm-symbolizer on PATH, the PDB beside the exe, and the executable's
actual basename, that resolves module-relative host PCs. `CPUCore_*` samples
in `[jit]` are guest execution candidates, not named guest functions. HLE may
run on those same cores, so distinguish its symbolized stacks from JIT code.
The host vi/nvservices/FS threads are explicitly selected, not guessed from
`Service*`. Other guest-backed services execute on CPUCore threads. The fixed
32-thread sampler capacity is enough for this explicit list, not for `*`.
Wall-clock samples include sleeping threads: a wait stack is not CPU busy time.

Rank only after reading the artifacts:

- Guest execution and HLE: per-core host CPU ms/frame from `DKB cpu` plus stacks
  on those cores; report each core separately before any total. Six cores'
  CPU time is not six additive pieces of frame latency.
- GPU front/back command processing: exclusive zone ms/s divided by the same
  window's presents/s, with host_cpu_busy_ms_per_s and sampled hot paths. Wait
  zones and active zones must be separate. Report macro/method/compute/setup
  costs, command-list writer/drain sites, and physical-GPU waits distinctly.
- Present: fence, composite and pacing from the same warmed segment. Fence
  latency overlaps producer work. Do not add it to that producer's CPU cost.

Name the top three costs with their thread, scope, timing basis and plausible
critical-path role, then select the first change. This run cannot establish a
small speed gain: a candidate needs a later controlled baseline/candidate A/B
with profiling disabled, warmed caches and the same route. Test 2x only after
1x reliably meets 16.67 ms; neither target is claimed by this branch.
