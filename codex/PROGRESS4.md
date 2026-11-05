READY-TO-PUSH

Branch xana/oot-bloom-fix from b94eb6513c. No push. V3 unchanged.
No production fix: offline checks have not reproduced the docked bloom cause.
RESULTS4.md contains ranked candidates and the one-run docked capture plan.

Completed:
- Inspected screenshot, PR #87 body/reviews and relevant upstream branches/diffs.
- Verified all 686 filename hashes and replayed all captures: 686 lower.
- Disassembled all 100 compute captures. Identified four actual 3D volume writers.
- No H0_NH1 occurrences anywhere in the 686 captures.
- Local SM86 hardware H0_NH1 test matches current lowering: (-10,12).
- Numeric probes cover SAT, EX2 sign, LG2 abs, F2F rounding, mixed-sign HMUL2/HFMA2,
  HADD2 lane broadcast, F2FP packing, FMNMX NaN rules, 64-bit F32 RED and float formats.
- Existing GCC suites pass: 581222/140 and 6840750/56 assertions/cases.
- Fresh clang-18/Ninja broad suite passes: 581222 assertions in 140 cases.
- Python tools/nx2 utility tests: 24 pass.

Fallback handoff complete: diagnostics only, no proven bloom fix.
Commit with per-command identity is on this branch; see git log.
Gap: no docked frame/resources, full emulator build, SPIR-V execution or live game test.
No other machine accessed. No settings or production source modified.
