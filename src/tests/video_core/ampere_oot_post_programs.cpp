// SPDX-License-Identifier: GPL-3.0-or-later
//
// The four reject classes a native recomp run of Ocarina of Time hits (lane K8). Every instruction below is a captured word
// from that run's DKB_SHADER_DUMP; the surrounding program is the smallest shape that reaches the same guard.

#include <array>
#include <vector>
#include <catch2/catch_test_macros.hpp>

#include "shader_recompiler/frontend/ampere/lower_to_maxwell.h"

namespace {
using Shader::Ampere::Instruction;
using Shader::Ampere::LowerToMaxwell;

constexpr Instruction Exit{{0x794d, 0, 0x03800000, 0}};
// TLD4.PTP, 2D, RGBA, per-tap offsets in R8/R9 (the same gather the gather suite uses).
constexpr Instruction PerTapGather{{0x040a7b63, 0x33800408, 0x041e2f08, 0x100fc200}};

// Offset definitions for the per-tap gather, then the gather, then the end.
std::vector<Instruction> PerTapProgram() {
    return {{
        {{0x00087802, 0xff00ff00, 0x00000f00, 0x000fe400}},   // MOV32I R8, 0xff00ff00
        {{0x00097802, 0x01000001, 0x00000f00, 0x000fe400}},   // MOV32I R9, 0x01000001
        PerTapGather,
        Exit,
    }};
}
} // namespace

TEST_CASE("Ocarina post-process tail: a call to an abort stub needs no link", "[video_core][ampere][gather][oot]") {
    // The tail of every failing program (e.g. a584b3ac9aafc26c at 0x1368): @Px CALL stub; BRA back; stub: BRA next; EXIT.
    // The CALL is predicated (0x00006944 / 0x00000944), its callee is a branch through to the EXIT, and no MOV32I sets a link
    // because the callee never returns.
    for (const u32 call : {0x00006944U, 0x00000944U, 0x00007944U}) {
        auto code = PerTapProgram();
        code.push_back({{0x00007918, 0, 0, 0}});                    // NOP
        code.push_back({{call, 0x10, 0x03c00000, 0x000fe200}});     // CALL the stub two instructions on
        code.push_back({{0x00007947, 0xffffffc0, 0x0383ffff, 0}});  // BRA back into the program
        code.push_back({{0x00007941, 0, 0x03800000, 0}});           // stub: BRA next
        code.push_back(Exit);                                      // stub: EXIT
        const auto lowered = LowerToMaxwell(code);
        INFO(lowered.error);
        CAPTURE(call);
        REQUIRE(lowered);
    }
}

TEST_CASE("Ocarina post-process tail: a callee that returns through a link is still proved", "[video_core][ampere][oot]") {
    // The same program without the stub: an unpredicated call whose callee returns must keep its return-linkage proof.
    auto code = PerTapProgram();
    code.push_back({{0x00007802, 0x20, 0x00000f00, 0}});             // MOV32I R0, 0x20 (the return address)
    code.push_back({{0x00007944, 0x10, 0x03c00000, 0}});             // CALL +0x10
    code.push_back({{0x00007947, 0xffffffc0, 0x0383ffff, 0}});       // BRA back
    code.push_back({{0x0000794d, 0, 0x03800000, 0}});               // EXIT
    code.push_back({{0x00007950, 0xffffffb0, 0x03c3ffff, 0}});       // RET to R0
    const auto lowered = LowerToMaxwell(code);
    const bool not_the_gather_proof = !lowered || lowered.error != "native per-tap gather control-flow proof";
    REQUIRE(not_the_gather_proof);
}

TEST_CASE("Ocarina offset proof: LEA between the offset definitions and the gather", "[video_core][ampere][gather][oot]") {
    // 0x34067211 at +0x140 of 7a2509bdb3ecb85e and +0x1e0 of 01b841e4ea92ce24: LEA writes one general register, and the scan
    // between the definitions and the gather has to know that.
    auto code = PerTapProgram();
    code.insert(code.begin() + 2, Instruction{{0x34067211, 0x00000009, 0x078e08ff, 0x000fe200}});
    const auto lowered = LowerToMaxwell(code);
    INFO(lowered.error);
    REQUIRE(lowered);
}

TEST_CASE("Ocarina offset proof: MOV cbuf/uniform and uniform IADD3 write one register", "[video_core][ampere][gather][oot]") {
    // 0x000a7c02 (MOV from a uniform register) and 0x0f1baa10 (IADD3 with a uniform source) from 0b60fb059b760c01 and
    // a7533cc90250f994. Both sit in the same scan.
    for (const Instruction definition : {Instruction{{0x000a7c02, 0x00000008, 0x08000f00, 0x000fc400}},
                                         Instruction{{0x0f1baa10, 0x00000900, 0x07ffe0ff, 0x000fe400}}}) {
        auto code = PerTapProgram();
        code.insert(code.begin() + 2, definition);
        const auto lowered = LowerToMaxwell(code);
        INFO(lowered.error);
        const bool not_the_offset_proof = !lowered || lowered.error != "native per-tap gather offset proof";
        REQUIRE(not_the_offset_proof);
    }
}

TEST_CASE("Ocarina FSEL 0xc08 lowers", "[video_core][ampere][oot]") {
    // ff347c08 0000000a 0e000000 000fe400 from 9b15ebf7d86d59a9 (+0x4210): FSEL with a uniform-bank operand, which had no
    // case. It shares the 0x208/0x808/0xa08 case body.
    const std::array<Instruction, 3> code{{
        {{0xff347c08, 0x0000000a, 0x0e000000, 0x000fe400}},
        {{0x00097802, 0x01000001, 0x00000f00, 0x000fe400}},
        Exit,
    }};
    const auto lowered = LowerToMaxwell(code);
    INFO(lowered.error);
    REQUIRE(lowered);
    REQUIRE(lowered.error != "unsupported native opcode");
}
