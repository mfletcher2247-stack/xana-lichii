// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <vector>
#include <catch2/catch_test_macros.hpp>

#include "shader_recompiler/frontend/ampere/lower_to_maxwell.h"

namespace {
using Shader::Ampere::Instruction;
using Shader::Ampere::LowerToMaxwell;

std::vector<Instruction> CapturedOcarinaCall() {
    const Instruction nop{{0x00007918, 0, 0, 0}};
    std::vector<Instruction> code(0x7150 / 16, nop);
    // Keep the original setup, call and helper addresses so these are captured words.
    code[0] = Instruction{{0x00007947, 0x6280, 0x03800000, 0}};
    constexpr std::array<Instruction, 11> window{{
        {{0x00027802, 0x00006340, 0x00000f00, 0x000fe200}},
        {{0x16037223, 0x00000016, 0x00010003, 0x000fe200}},
        {{0x0000781c, 0x00000000, 0x00703070, 0x000fca00}},
        {{0x00037308, 0x00000003, 0x00001400, 0x000e6400}},
        {{0x12127220, 0x00000003, 0x00410000, 0x082fe200}},
        {{0x14147220, 0x00000003, 0x00410000, 0x080fe200}},
        {{0x16167220, 0x00000003, 0x00410000, 0x000fe200}},
        {{0x0500720c, 0x000000ff, 0x00705070, 0x001fc800}},
        {{0xff1c7807, 0x00000003, 0x04000000, 0x000fe400}},
        {{0xff057807, 0x00000001, 0x04000000, 0x000fe400}},
        {{0x00007944, 0x00000bc0, 0x03c00000, 0x000fea00}},
    }};
    constexpr std::array<Instruction, 34> helper{{
        {{0x000f7919, 0x00000000, 0x00003900, 0x000e2200}},
        {{0x00047886, 0x00000000, 0x038e0000, 0x000fc400}},
        {{0xff037c12, 0x00000004, 0x0f82880f, 0x001fe400}},
        {{0x0f0f7c12, 0x00000004, 0x0f8ec0ff, 0x000fc800}},
        {{0x00037300, 0x00000003, 0x000e0000, 0x000e3000}},
        {{0x000f7300, 0x0000000f, 0x000e0000, 0x000e6200}},
        {{0x1c0e7589, 0x00001f03, 0x000e0000, 0x001e2400}},
        {{0x0e0e7207, 0x000000ff, 0x00800000, 0x001fc400}},
        {{0x0f1a7589, 0x00001e0f, 0x00020000, 0x002e2800}},
        {{0x0e107589, 0x00001f0f, 0x000e0000, 0x000e6200}},
        {{0x1a1a7207, 0x0000000f, 0x00800000, 0x001fe400}},
        {{0x10177207, 0x000000ff, 0x00800000, 0x002fc600}},
        {{0x1a197589, 0x00001e1a, 0x00020000, 0x000e2200}},
        {{0x0e177210, 0x00000017, 0x07ffe0ff, 0x000fca00}},
        {{0x17107589, 0x00001f1a, 0x000e0000, 0x000e6200}},
        {{0x19197207, 0x0000001a, 0x00800000, 0x001fe400}},
        {{0x10107207, 0x000000ff, 0x00800000, 0x002fc600}},
        {{0x191c7589, 0x00001e19, 0x00020000, 0x000e2200}},
        {{0x17107210, 0x00000010, 0x07ffe0ff, 0x000fca00}},
        {{0x10037589, 0x00001f19, 0x000e0000, 0x000e6200}},
        {{0x1c1c7207, 0x00000019, 0x00800000, 0x001fe400}},
        {{0x03037207, 0x000000ff, 0x00800000, 0x002fc600}},
        {{0x1c1b7589, 0x00001e1c, 0x00020000, 0x000e2200}},
        {{0x100f7210, 0x00000003, 0x07ffe0ff, 0x000fca00}},
        {{0x0f037589, 0x00001f1c, 0x000e0000, 0x000e6200}},
        {{0x1b1a7207, 0x0000001c, 0x00800000, 0x001fe400}},
        {{0x030e7207, 0x000000ff, 0x00800000, 0x002fc600}},
        {{0x1aff7589, 0x00001e1a, 0x00020000, 0x000fe200}},
        {{0x0f177210, 0x0000000e, 0x07ffe0ff, 0x000fca00}},
        {{0x17037589, 0x00001f1a, 0x000e0000, 0x000e2400}},
        {{0x030e7207, 0x000000ff, 0x00800000, 0x001fe400}},
        {{0x00037802, 0x00000000, 0x00000f00, 0x000fe400}},
        {{0x170e7210, 0x0000000e, 0x07ffe0ff, 0x000fe400}},
        {{0x02007950, 0xffff8ee0, 0x03c3ffff, 0x000fea00}},
    }};
    std::copy(window.begin(), window.end(), code.begin() + 0x629);
    code[0x634] = Instruction{{0x0000794d, 0, 0x03800000, 0}};
    code[0x6ef] = Instruction{{0x0000794d, 0, 0x03800000, 0}};
    std::copy(helper.begin(), helper.end(), code.begin() + 0x6f0);
    // Unreachable captured tails can contain indirect control and unrelated calls.
    code[0x712] = Instruction{{0x00007947, 0xfffffff0, 0x0383ffff, 0}};
    code[0x713] = Instruction{{0x00007949, 0, 0, 0}};
    code[0x714] = Instruction{{0x00000944, 0x10, 0x03c00000, 0}};
    return code;
}

TEST_CASE("Ocarina nonadjacent call link ignores an unreachable capture tail",
          "[video_core][ampere][oot]") {
    const auto code = CapturedOcarinaCall();
    const auto lowered = LowerToMaxwell(code);
    INFO(lowered.error);
    REQUIRE(lowered);
    CHECK(lowered.source_size == 0x7120);
    const u32 call = lowered.source_instruction_offsets[0x633];
    const u32 target = lowered.source_instruction_offsets[0x6f0];
    const auto address = [](u32 index) { return 0x50 + index * 8 + ((index + 1) / 3) * 8; };
    const s64 relative = s64{address(target)} - address(call) - 8;
    CHECK(lowered.instructions[call] == (0xe26000000007000fULL |
          ((static_cast<u64>(relative) & 0xffffff) << 20)));
    CHECK(lowered.instructions.back() == 0xe32000000007000fULL);
}

TEST_CASE("Ocarina nonadjacent call still requires a dominating preserved link",
          "[video_core][ampere][oot]") {
    for (const u32 mutation : {0U, 1U, 2U, 3U}) {
        auto code = CapturedOcarinaCall();
        if (mutation == 0) // R2 overwritten between setup and call.
            code[0x632].words[0] = (code[0x632].words[0] & ~0xff0000U) | (2U << 16);
        if (mutation == 1) // Entry bypasses the setup.
            code[0].words[1] += 16;
        if (mutation == 2) // Reachable indirect control cannot bound the capture.
            code[0x629] = Instruction{{0x00007949, 0, 0, 0}};
        if (mutation == 3) { // A conditional return can reach indirect control.
            code[0x711].words[0] &= ~0xf000U;
            code[0x712] = code[0x713];
        }
        const auto lowered = LowerToMaxwell(code);
        CAPTURE(mutation);
        REQUIRE_FALSE(lowered);
    }
}

TEST_CASE("Ampere returns do not write the link register", "[video_core][ampere]") {
    for (const u32 link : {0U, 55U}) {
        const std::array<Instruction, 4> code{{
            {{0x00007802U | (link << 16), 0x20, 0x00000f00, 0}},
            {{0x00007944, 0x10, 0x03c00000, 0}},
            {{0x0000794d, 0, 0x03800000, 0}},
            {{0x00007950U | (link << 24), 0xffffffc0, 0x03c3ffff, 0}},
        }};
        const auto lowered = LowerToMaxwell(code);
        REQUIRE(static_cast<bool>(lowered));
        REQUIRE(lowered.source_size == sizeof(code));
        REQUIRE(lowered.instructions.back() == 0xe32000000007000fULL);
    }
}

TEST_CASE("Ampere returns can read a copy of the R0 link", "[video_core][ampere]") {
    const std::array<Instruction, 5> code{{
        {{0x00007802, 0x20, 0x00000f00, 0}},
        {{0x00007944, 0x10, 0x03c00000, 0}},
        {{0x0000794d, 0, 0x03800000, 0}},
        {{0x00017202, 0, 0x00000f00, 0}},
        {{0x01007950, 0xffffffb0, 0x03c3ffff, 0}},
    }};
    REQUIRE(static_cast<bool>(LowerToMaxwell(code)));
}

TEST_CASE("Ampere callees still reject writes to the link", "[video_core][ampere]") {
    const std::array<Instruction, 5> code{{
        {{0x00007802, 0x20, 0x00000f00, 0}},
        {{0x00007944, 0x10, 0x03c00000, 0}},
        {{0x0000794d, 0, 0x03800000, 0}},
        {{0x00007802, 0, 0x00000f00, 0}},
        {{0x00007950, 0xffffffb0, 0x03c3ffff, 0}},
    }};
    const auto lowered = LowerToMaxwell(code);
    REQUIRE(!static_cast<bool>(lowered));
    REQUIRE(lowered.error == "native callee overwrites link register");
    REQUIRE(lowered.error_offset == 0x30);
}

TEST_CASE("Ampere callees can compare into predicates with an R0 link", "[video_core][ampere]") {
    // ISETP's zero destination field is not a write to R0.
    const std::array<Instruction, 5> code{{
        {{0x00007802, 0x20, 0x00000f00, 0}},
        {{0x00007944, 0x10, 0x03c00000, 0}},
        {{0x0000794d, 0, 0x03800000, 0}},
        {{0x0100720c, 0x00000002, 0x03f06070, 0}},
        {{0x00007950, 0xffffffb0, 0x03c3ffff, 0}},
    }};
    REQUIRE(static_cast<bool>(LowerToMaxwell(code)));
}

TEST_CASE("Ampere callees reject a wide multiply over the link", "[video_core][ampere]") {
    // IMAD.WIDE.U32 R54:R55 writes the R55 link.
    const std::array<Instruction, 5> code{{
        {{0x00377802, 0x20, 0x00000f00, 0}},
        {{0x00007944, 0x10, 0x03c00000, 0}},
        {{0x0000794d, 0, 0x03800000, 0}},
        {{0x01367225, 0x00000002, 0x078e00ff, 0}},
        {{0x37007950, 0xffffffb0, 0x03c3ffff, 0}},
    }};
    const auto lowered = LowerToMaxwell(code);
    REQUIRE(!static_cast<bool>(lowered));
    REQUIRE(lowered.error == "native callee overwrites link register");
    REQUIRE(lowered.error_offset == 0x30);
}
} // namespace
