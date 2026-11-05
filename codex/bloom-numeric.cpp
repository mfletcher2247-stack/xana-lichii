// SPDX-License-Identifier: GPL-3.0-or-later
// Offline numeric probes for the captured OoT lighting instructions. No game execution.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ampere/lower_to_maxwell.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"
#include "shader_recompiler/object_pool.h"
#include "video_core/texture_cache/format_lookup_table.h"

namespace IR = Shader::IR;
using Shader::Ampere::Instruction;
struct Environment : Shader::Environment {
    u64 ReadInstruction(u32) override { throw std::runtime_error("instruction read"); }
    u32 ReadCbufValue(u32, u32) override { throw std::runtime_error("cbuf read"); }
    Shader::TextureType ReadTextureType(u32) override { return Shader::TextureType::Color2D; }
    Shader::TexturePixelFormat ReadTexturePixelFormat(u32) override {
        return Shader::TexturePixelFormat::A8B8G8R8_UNORM;
    }
    bool IsTexturePixelFormatInteger(u32) override { return false; }
    u32 ReadViewportTransformState() override { return 1; }
    u32 TextureBoundBuffer() const override { return 0; }
    u32 LocalMemorySize() const override { return 0; }
    u32 SharedMemorySize() const override { return 0; }
    std::array<u32, 3> WorkgroupSize() const override { return {1, 1, 1}; }
    bool HasHLEMacroState() const override { return false; }
    bool HasNativeInstructions() const override { return true; }
    std::optional<Shader::ReplaceConstant> GetReplaceConstBuffer(u32, u32) override {
        return std::nullopt;
    }
    void Dump(u64, u64) override {}
};

struct Probe {
    Shader::ObjectPool<IR::Inst> pool{256};
    IR::Block block{pool};
    Environment env;
    std::array<u32, 256> regs{};
    std::unordered_map<u64, u32> cbuf, memory;
    std::unordered_map<const IR::Inst*, u64> values;
    explicit Probe(Instruction instruction) {
        if (instruction.Bits(12, 4) != 7) throw std::runtime_error("probe requires @PT");
        constexpr Instruction exit{{0x794d, 0, 0x03800000, 0}};
        const auto lowered = Shader::Ampere::LowerToMaxwell(std::array{instruction, exit});
        if (!lowered) throw std::runtime_error(std::string(lowered.error));
        Shader::Maxwell::TranslatorVisitor visitor{env, block};
        for (u64 word : lowered.instructions) {
            const u32 op = word >> 48;
            if (op == 0x5c98) visitor.MOV_reg(word);
            else if (op == 0x4c98) visitor.MOV_cbuf(word);
            else if (op == 0x5ca8) visitor.F2F_reg(word);
            else if (op == 0x5c18) visitor.ISCADD_reg(word);
            else if (op == 0x36f8) visitor.SHF_l_imm(word);
            else if (op == 0x5c68 || op == 0x5c6c) visitor.FMUL_reg(word);
            else if (op == 0x5c58 || op == 0x5c5c) visitor.FADD_reg(word);
            else if ((op & 0xff80) == 0x5980) visitor.FFMA_reg(word);
            else if (op == 0x5c60) visitor.FMNMX_reg(word);
            else if ((op & 0xfffc) == 0x5080) visitor.MUFU(word);
            else if ((op & 0xfff8) == 0x5d00) visitor.HFMA2_reg(word);
            else if ((op & 0xfff8) == 0x5d08) visitor.HMUL2_reg(word);
            else if ((op & 0xfff8) == 0x5d10) visitor.HADD2_reg(word);
            else if ((op & 0xfff8) == 0xebf8) visitor.RED(word);
            else if ((op & 0xff80) == 0x5800) visitor.FSET_reg(word);
            else if ((op & 0xfff8) == 0x5c40) visitor.LOP_reg(word);
            else if (word >> 58 == 1) visitor.LOP32I(word);
            else if (word >> 56 == 1) visitor.MOV32I(word);
            else if (word != 0xe30000000007000fULL)
                throw std::runtime_error("unhandled lowered word " + std::to_string(word));
        }
    }
    u64 Value(IR::Value v) {
        if (!v.IsImmediate()) return values.at(v.Inst());
        switch (v.Type()) {
        case IR::Type::U1: return v.U1();
        case IR::Type::U32: return v.U32();
        case IR::Type::U64: return v.U64();
        case IR::Type::F32: return std::bit_cast<u32>(v.F32());
        default: throw std::runtime_error("unhandled immediate");
        }
    }
    double Float(IR::Value v) {
        return v.Type() == IR::Type::F16 ? double(std::bit_cast<_Float16>(u16(Value(v))))
                                       : double(std::bit_cast<float>(u32(Value(v))));
    }
    void Run() {
        values.clear();
        for (const auto& i : block.Instructions()) {
            auto a = [&](unsigned n) { return Value(i.Arg(n)); };
            auto f = [&](unsigned n) { return Float(i.Arg(n)); };
            auto fp = [&](double v) -> u64 {
                return i.Type() == IR::Type::F16 ? std::bit_cast<u16>(_Float16(v))
                                               : std::bit_cast<u32>(float(v));
            };
            u64 result{};
            switch (i.GetOpcode()) {
            case IR::Opcode::Void: break;
            case IR::Opcode::SetRegister: regs.at(size_t(i.Arg(0).Reg())) = a(1); break;
            case IR::Opcode::GetRegister: result = regs.at(size_t(i.Arg(0).Reg())); break;
            case IR::Opcode::GetCbufU32: result = cbuf.at((a(0) << 32) | a(1)); break;
            case IR::Opcode::Identity:
            case IR::Opcode::BitCastF32U32:
            case IR::Opcode::BitCastU32F32: result = a(0); break;
            case IR::Opcode::ConvertF16F32: result = fp(f(0)); break;
            case IR::Opcode::ConvertF32F16: result = fp(f(0)); break;
            case IR::Opcode::UnpackFloat2x16: result = (a(0) & 65535) | (a(0) >> 16) << 32; break;
            case IR::Opcode::CompositeExtractF16x2: result = u32(a(0) >> (a(1) * 32)); break;
            case IR::Opcode::CompositeConstructF16x2: result = a(0) | a(1) << 32; break;
            case IR::Opcode::PackFloat2x16: result = (a(0) & 65535) | (a(0) >> 32) << 16; break;
            case IR::Opcode::CompositeConstructU32x2: result = u32(a(0)) | a(1) << 32; break;
            case IR::Opcode::PackUint2x32: result = a(0); break;
            case IR::Opcode::ShiftLeftLogical32: result = u32(a(0)) << (a(1) & 31); break;
            case IR::Opcode::ShiftRightLogical32: result = u32(a(0)) >> (a(1) & 31); break;
            case IR::Opcode::BitwiseAnd32: result = a(0) & a(1); break;
            case IR::Opcode::BitwiseOr32: result = a(0) | a(1); break;
            case IR::Opcode::BitwiseXor32: result = a(0) ^ a(1); break;
            case IR::Opcode::UMin32: result = std::min(u32(a(0)), u32(a(1))); break;
            case IR::Opcode::IEqual: result = a(0) == a(1); break;
            case IR::Opcode::ShiftLeftLogical64: result = a(0) << (a(1) & 63); break;
            case IR::Opcode::ShiftRightLogical64: result = a(0) >> (a(1) & 63); break;
            case IR::Opcode::UnpackUint2x32: result = a(0); break;
            case IR::Opcode::CompositeExtractU32x2: result = u32(a(0) >> (a(1) * 32)); break;
            case IR::Opcode::IAdd32: result = u32(a(0) + a(1)); break;
            case IR::Opcode::IAdd64: result = a(0) + a(1); break;
            case IR::Opcode::FPIsNan32: result = std::isnan(f(0)); break;
            case IR::Opcode::LogicalAnd: result = a(0) && a(1); break;
            case IR::Opcode::LogicalOr: result = a(0) || a(1); break;
            case IR::Opcode::LogicalNot: result = !a(0); break;
            case IR::Opcode::SelectF32:
            case IR::Opcode::SelectU32: result = a(0) ? a(1) : a(2); break;
            case IR::Opcode::FPNeg16:
            case IR::Opcode::FPNeg32: result = fp(-f(0)); break;
            case IR::Opcode::FPAbs16:
            case IR::Opcode::FPAbs32: result = fp(std::abs(f(0))); break;
            case IR::Opcode::FPAdd16:
            case IR::Opcode::FPAdd32: result = fp(f(0) + f(1)); break;
            case IR::Opcode::FPMul16:
            case IR::Opcode::FPMul32: result = fp(f(0) * f(1)); break;
            case IR::Opcode::FPFma16:
            case IR::Opcode::FPFma32: result = fp(std::fma(f(0), f(1), f(2))); break;
            case IR::Opcode::FPSaturate16:
            case IR::Opcode::FPSaturate32: result = fp(std::fmin(std::fmax(f(0), 0.0), 1.0)); break;
            case IR::Opcode::FPClamp16:
            case IR::Opcode::FPClamp32: result = fp(std::fmin(std::fmax(f(0), f(1)), f(2))); break;
            case IR::Opcode::FPMin32: result = fp(std::fmin(f(0), f(1))); break;
            case IR::Opcode::FPMax32: result = fp(std::fmax(f(0), f(1))); break;
            case IR::Opcode::FPExp2: result = fp(std::exp2(f(0))); break;
            case IR::Opcode::FPLog2: result = fp(std::log2(f(0))); break;
            case IR::Opcode::FPRecip32: result = fp(1.0 / f(0)); break;
            case IR::Opcode::FPRecipSqrt32: result = fp(1.0 / std::sqrt(f(0))); break;
            case IR::Opcode::FPSqrt: result = fp(std::sqrt(f(0))); break;
            case IR::Opcode::GlobalAtomicAddF32: {
                auto& cell = memory.at(a(0));
                result = cell;
                cell = std::bit_cast<u32>(std::bit_cast<float>(cell) + float(f(1)));
                break;
            }
            default: throw std::runtime_error(std::string(IR::NameOf(i.GetOpcode())));
            }
            values.emplace(&i, result);
        }
    }
};

Instruction Captured(const std::filesystem::path& root, std::string_view prefix, u32 pc) {
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.path().filename().string().starts_with(prefix)) continue;
        std::ifstream file(entry.path(), std::ios::binary);
        Instruction instruction{};
        file.seekg(pc);
        file.read(reinterpret_cast<char*>(&instruction), sizeof(instruction));
        if (!file) throw std::runtime_error("short capture");
        return instruction;
    }
    throw std::runtime_error("capture not found");
}
void Check(bool good, const char* label) {
    if (!good) throw std::runtime_error(label);
}
u32 Bits(float f) { return std::bit_cast<u32>(f); }
int main(int argc, char** argv) try {
    if (argc != 2) throw std::runtime_error("usage: bloom-numeric <captures/raw>");
    const std::filesystem::path root{argv[1]};
    // ec68: volume integration. SAT prevents out-of-range depth coordinates.
    for (float x : {-2.f, 0.f, 0.25f, 0.5f, 1.f, 4.f}) {
        Probe p{Captured(root, "ec68e40eb783ff32", 0x480)};
        p.regs[24] = Bits(x); p.regs[19] = Bits(2.f); p.Run();
        Check(p.regs[24] == Bits(std::clamp(x * 2.f, 0.f, 1.f)), "captured FMUL.SAT");
    }
    std::cout << "PASS captured volume FMUL.SAT: negative, normal and >1 inputs\n";
    for (float x : {0.f, 0.25f, 1.f, 4.f, 16.f}) {
        Probe p{Captured(root, "ec68e40eb783ff32", 0x590)};
        p.regs[10] = Bits(x); p.Run();
        Check(p.regs[15] == Bits(std::exp2(-x)), "captured negative MUFU.EX2");
    }
    std::cout << "PASS captured volume MUFU.EX2: exp2(-x), not exp2(x)\n";
    {
        Probe p{Captured(root, "ec68e40eb783ff32", 0x4a0)};
        p.regs[24] = Bits(-0.25f); p.Run();
        Check(p.regs[24] == Bits(-2.f), "captured MUFU.LG2 absolute");
    }
    std::cout << "PASS captured volume MUFU.LG2: log2(abs(-0.25)) = -2\n";
    {
        // Same register HFMA2 encoding, bit 86 set, as the hardware probe.
        Probe p{Instruction{{0x02017231, 3, 0x00400004, 0}}};
        p.regs[2] = 0x42004000; p.regs[3] = 0x45004400; p.Run();
        Check(p.regs[1] == 0x4a00c900, "H0_NH1 hardware reference");
    }
    std::cout << "PASS synthetic H0_NH1: (-10,12), matches real SM86 execution\n";
    {
        Probe p{Captured(root, "70cc863d5e1a6627", 0xba0)};
        // UR4 maps to R220. Preserve the high address word and float operation.
        p.regs[220] = 0x12340000; p.regs[221] = 0x12;
        p.memory[0x1212340000ULL] = Bits(1.5f);
        p.regs[0] = Bits(0.25f); p.Run();
        Check(p.memory.at(0x1212340000ULL) == Bits(1.75f), "F32 reduction with .E pointer");
    }
    std::cout << "PASS captured RED.E.ADD.F32: 1.5 + 0.25 = 1.75 at >4 GiB address\n";
    {
        for (float value : {0.f, -0.f, 1.f, 1.00048828125f, 1.00146484375f,
                            -1.00048828125f, 65504.f, 0.000000059604644775390625f}) {
            Probe p{Instruction{{0x00017304, 2, 0x00200800, 0}}};
            p.regs[2] = Bits(value); p.Run();
            Check(p.regs[1] == std::bit_cast<u16>(_Float16(value)), "F2F nearest-even and zero high half");
        }
    }
    std::cout << "PASS synthetic F2F.F16.F32: even ties, signed zero, largest finite, subnormal\n";
    {
        for (auto [low, high] : std::array{std::pair{-8.f, 2.f}, std::pair{0.125f, -0.5f},
                                         std::pair{32.f, 0.f}}) {
            const auto pack = [](float a, float b) {
                return u32(std::bit_cast<u16>(_Float16(a))) |
                       (u32(std::bit_cast<u16>(_Float16(b))) << 16);
            };
            Probe mul{Captured(root, "6f9367bd41dc0b58", 0x510)};
            mul.regs[32] = pack(low, high); mul.Run();
            Check(mul.regs[32] == pack(low * 0.0721435546875f, high * 0.0721435546875f),
                  "captured mixed-sign luminance HMUL2");
            Probe fma{Captured(root, "6f9367bd41dc0b58", 0x570)};
            fma.regs[12] = pack(low, high); fma.regs[32] = pack(0.5f, -0.25f); fma.Run();
            Check(fma.regs[12] == pack(std::fma(low, 0.2125244140625f, 0.5f),
                                      std::fma(high, 0.2125244140625f, -0.25f)),
                  "captured mixed-sign luminance HFMA2");
        }
    }
    std::cout << "PASS captured luminance HMUL2/HFMA2: independent mixed-sign halves\n";
    {
        Probe p{Captured(root, "6f9367bd41dc0b58", 0x2f40)};
        p.regs[25] = 0x4000c400; // (-4, 2)
        p.regs[61] = 0xfc003800; // low = 0.5, high is deliberately poisoned
        p.Run();
        Check(p.regs[25] == 0x4100c300, "captured HADD2 low-half broadcast");
    }
    std::cout << "PASS captured HADD2.H0_H0: (-4,2) + (0.5,0.5) = (-3.5,2.5)\n";
    {
        Probe p{Captured(root, "2f5791cea2a85397", 0x1b20)};
        p.regs[83] = Bits(-0.25f); p.regs[82] = Bits(1.00146484375f); p.Run();
        Check(p.regs[82] == 0xb4003c02, "captured F2FP high/low order and even tie");
    }
    std::cout << "PASS captured F2FP.PACK_AB: high=-0.25, low=nearest-even tie\n";
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        for (bool propagate : {false, true}) for (bool maximum : {false, true}) {
            for (auto [a, b] : std::array{std::pair{2.f, 4.f}, std::pair{nan, 4.f},
                                         std::pair{2.f, nan}, std::pair{nan, nan}}) {
                Probe p{Instruction{{0x02017209, 3,
                    0x03800000U | (u32(maximum) << 26) | (u32(propagate) << 17), 0}}};
                p.regs[2] = Bits(a); p.regs[3] = Bits(b); p.Run();
                const float expected = propagate && (std::isnan(a) || std::isnan(b))
                    ? nan : maximum ? std::fmax(a, b) : std::fmin(a, b);
                const float actual = std::bit_cast<float>(p.regs[1]);
                Check(std::isnan(expected) ? std::isnan(actual) : actual == expected,
                      "FMNMX default and .NAN modes");
            }
        }
    }
    std::cout << "PASS synthetic FMNMX: numeric/default-NaN/propagating-NaN min and max\n";
    using namespace Tegra::Texture;
    using VideoCore::Surface::PixelFormat;
    for (auto [format, expected] : std::array{
             std::pair{TextureFormat::R16G16B16A16, PixelFormat::R16G16B16A16_FLOAT},
             std::pair{TextureFormat::R16G16, PixelFormat::R16G16_FLOAT},
             std::pair{TextureFormat::B10G11R11, PixelFormat::B10G11R11_FLOAT}}) {
        Check(VideoCommon::PixelFormatFromTextureInfo(format, ComponentType::FLOAT,
              ComponentType::FLOAT, ComponentType::FLOAT, ComponentType::FLOAT, false) == expected,
              "HDR float format mapping");
    }
    std::cout << "PASS float texture format maps: RGBA16F, RG16F and R11G11B10F stay float\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "FAIL " << e.what() << '\n'; return 1;
}
