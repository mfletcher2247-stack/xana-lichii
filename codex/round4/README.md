# Reproducing the round 4 probes

Run from the repository root. These commands use the frontend library built in
round 2. They do not launch an emulator or access another machine.

## Numeric checks through production Maxwell translation

    clang++-18 -std=c++20 -O2 -DNDEBUG -DFMT_HEADER_ONLY -DARCHITECTURE_x86_64 \
      -ffunction-sections -fdata-sections -Isrc -Iexternals \
      -I/home/xana/.cache/uv/archive-v0/BXcRHo_rqOMlqymp/torch/include \
      codex/bloom-numeric.cpp src/video_core/texture_cache/format_lookup_table.cpp \
      build-gather/round2/frontend-build/libfrontend.a -Wl,--gc-sections \
      -o build-gather/round4/bloom-numeric-clang
    build-gather/round4/bloom-numeric-clang \
      /home/xana/xana/30-Research/oot-60fps-20261104/captures/raw

The fmt include directory is an existing local dependency cache. On another
checkout substitute its fmt include path and rebuild the frontend library.

This probes raw IR for selected ordinary finite arithmetic inputs. It does not
simulate texture sampling, per-instruction FTZ, execution predicates or Vulkan
float controls. Non-PT native instructions are rejected by the probe.

## Whole corpus and compute disassembly

    c++ -std=c++20 -O2 -Isrc -Iexternals/nlohmann/json/include \
      codex/lower-program.cpp -o build-gather/round4/lower-v4
    python3 codex/bloom-census.py \
      /home/xana/xana/30-Research/oot-60fps-20261104/captures/raw \
      build-gather/round4/census --lower build-gather/round4/lower-v4

Requires nvdisasm for raw SM86 disassembly. The output directory holds SASS,
Maxwell binaries and a JSON census. Filename hashes are checked before replay.

## Optional native H0_NH1 hardware check

Requires the local CUDA compiler, driver and an SM86 GPU. It executes a single
thread, stores one word and releases its CUDA context. It is separate from the
CPU numeric check and does not exercise Vulkan.

    nvcc -cubin -arch=sm_86 codex/round4/half-probe.cu \
      -o build-gather/round4/half-probe.cubin
    python3 codex/round4/half-probe.py build-gather/round4/half-probe.cubin \
      build-gather/round4/hardware
    nvdisasm -c build-gather/round4/hardware/half-mixed.cubin

The Python probe finds the kernel's single HFMA2 in the ELF text section and
sets only bit 86 in a copy. It checks the original and mixed-sign results.
Expected output:

    plain 0x4b804800 (8.0, 15.0)
    H0_NH1 0x4a00c900 (-10.0, 12.0)
