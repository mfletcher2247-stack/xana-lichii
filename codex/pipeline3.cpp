// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <nlohmann/json.hpp>


#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ampere/lower_to_maxwell.h"
#include "shader_recompiler/frontend/ampere/program_address_map.h"
#include "shader_recompiler/frontend/ampere/program_capture.h"
#include "shader_recompiler/frontend/ampere/trap_guards.h"
#include "shader_recompiler/frontend/maxwell/structured_control_flow.h"
#include "shader_recompiler/frontend/maxwell/translate_program.h"
#include "shader_recompiler/host_translate_info.h"
#include "shader_recompiler/frontend/maxwell/translate/translate.h"

namespace {
using Json = nlohmann::json;
using Shader::Stage;
using Shader::Ampere::Instruction;

struct MissingContext : std::runtime_error {
    using std::runtime_error::runtime_error;
};

constexpr std::array StageNames{"vertex",  "tess-control", "tess-eval", "geometry", "fragment",
                                "compute", "vertex-a",     "task",      "mesh"};

Stage ParseStage(std::string_view name) {
    for (size_t i = 0; i < StageNames.size(); ++i) {
        if (name == StageNames[i])
            return static_cast<Stage>(i);
    }
    throw std::runtime_error("Unknown stage: " + std::string{name});
}

std::vector<u8> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Cannot open " + path.string());
    const auto size = file.tellg();
    if (size < 0 || size > 64 * 1024 * 1024) {
        throw std::runtime_error("Input exceeds the 64 MiB limit");
    }
    std::vector<u8> bytes(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        throw std::runtime_error("Cannot read " + path.string());
    }
    return bytes;
}

class Reader {
public:
    explicit Reader(std::span<const u8> bytes_) : bytes{bytes_} {}

    template <typename T>
    T Read() {
        T value;
        Copy(&value, sizeof(value));
        return value;
    }

    void Copy(void* destination, size_t size) {
        if (size > bytes.size())
            throw std::runtime_error("Truncated environment record");
        std::memcpy(destination, bytes.data(), size);
        bytes = bytes.subspan(size);
    }

    size_t Remaining() const {
        return bytes.size();
    }

private:
    std::span<const u8> bytes;
};

template <typename K, typename V>
V Lookup(const std::map<K, V>& values, K key, std::string_view name) {
    const auto it = values.find(key);
    if (it == values.end()) {
        throw MissingContext(fmt::format("Uncaptured {} at {:#x}", name, key));
    }
    return it->second;
}

u64 CbufKey(u32 bank, u32 offset) {
    return (u64{bank} << 32) | offset;
}

class OfflineEnvironment final : public Shader::Environment {
public:
    void Load(std::span<const u8> bytes, u32 version) {
        Reader reader{bytes};
        const u64 size = reader.Read<u64>();
        const u64 type_count = reader.Read<u64>();
        const u64 format_count = reader.Read<u64>();
        const u64 cbuf_count = reader.Read<u64>();
        const u64 pointer_count = version >= 47 ? reader.Read<u64>() : 0;
        const u64 replacement_count = reader.Read<u64>();
        if (size % 8 || !size || size > reader.Remaining()) {
            throw std::runtime_error("Invalid environment code size");
        }
        for (const u64 count :
             {type_count, format_count, cbuf_count, pointer_count, replacement_count}) {
            if (count > bytes.size() / 8)
                throw std::runtime_error("Invalid environment entry count");
        }
        local_memory = reader.Read<u32>();
        texture_bound = reader.Read<u32>();
        start_address = reader.Read<u32>();
        low = reader.Read<u32>();
        high = reader.Read<u32>();
        viewport = reader.Read<u32>();
        const u32 stage_value = reader.Read<u32>();
        if (stage_value >= StageNames.size())
            throw std::runtime_error("Invalid cached stage");
        stage = static_cast<Stage>(stage_value);
        code.resize(size / 8);
        reader.Copy(code.data(), size);
        if (low % 8 || high < low || u64{high} - low + 8 != size) {
            throw std::runtime_error("Invalid cached instruction bounds");
        }
        for (u64 i = 0; i < type_count; ++i) {
            const u32 key = reader.Read<u32>();
            const u32 type = reader.Read<u32>();
            if (type >= Shader::NUM_TEXTURE_TYPES)
                throw std::runtime_error("Invalid texture type");
            texture_types.emplace(key, static_cast<Shader::TextureType>(type));
        }
        for (u64 i = 0; i < format_count; ++i) {
            const u32 key = reader.Read<u32>();
            const u32 value = reader.Read<u32>();
            if (value > std::numeric_limits<u8>::max()) {
                throw std::runtime_error("Invalid texture pixel format");
            }
            texture_formats.emplace(key, static_cast<Shader::TexturePixelFormat>(value));
        }
        for (u64 i = 0; i < cbuf_count; ++i) {
            const u64 key = reader.Read<u64>();
            cbuf_values.emplace(key, reader.Read<u32>());
        }
        for (u64 i = 0; i < pointer_count; ++i) {
            const u64 key = reader.Read<u64>();
            const u8 value = reader.Read<u8>();
            if (value > 1)
                throw std::runtime_error("Invalid storage pointer classification");
            pointers.emplace(key, value);
        }
        for (u64 i = 0; i < replacement_count; ++i) {
            const u64 key = reader.Read<u64>();
            const u32 value = reader.Read<u32>();
            if (value > 2)
                throw std::runtime_error("Invalid constant replacement");
            replacements.emplace(key, static_cast<Shader::ReplaceConstant>(value));
        }
        if (stage == Stage::Compute) {
            workgroup = reader.Read<std::array<u32, 3>>();
            shared_memory = reader.Read<u32>();
            if (!workgroup[0] || !workgroup[1] || !workgroup[2]) {
                throw std::runtime_error("Invalid cached workgroup dimensions");
            }
        } else {
            sph = reader.Read<Shader::ProgramHeader>();
            if (stage == Stage::Geometry)
                gp_passthrough_mask = reader.Read<std::array<u32, 8>>();
        }
        if (reader.Remaining())
            throw std::runtime_error("Trailing bytes after environment record");
        is_proprietary_driver = texture_bound == 2 || IsNativeProgram();
        captured = true;
    }

    void SetStage(Stage value) {
        stage = value;
    }

    void SetHeader(std::span<const u8> bytes) {
        std::memcpy(&sph, bytes.data(), sizeof(sph));
        const u64 size = sph.LocalMemorySize() + sph.common3.shader_local_memory_crs_size;
        if (size > std::numeric_limits<u32>::max())
            throw std::runtime_error("Local memory overflow");
        local_memory = static_cast<u32>(size);
    }

    void SetCode(const Shader::Ampere::LoweredProgram& lowered) {
        code.resize(sizeof(sph) / sizeof(u64));
        std::memcpy(code.data(), &sph, sizeof(sph));
        for (const u64 instruction : lowered.instructions) {
            if (code.size() % 4 == 0)
                code.push_back(0);
            code.push_back(instruction);
        }
        code.push_back(0);
        Shader::Ampere::AppendNativeAddressMap(code, lowered.source_instruction_offsets);
        low = 0;
        high = static_cast<u32>(code.size() * 8 - 8);
        start_address = stage == Stage::Compute ? sizeof(sph) : 0;
        if (lowered.required_texture_buffer != ~0U)
            texture_bound = lowered.required_texture_buffer;
        is_proprietary_driver = true;
    }

    u32 EntryPoint() const {
        return start_address + (stage == Stage::Compute ? 0 : sizeof(sph));
    }

    u64 ReadInstruction(u32 address) override {
        if (address < low || address > high || (address - low) % 8) {
            throw std::runtime_error(
                fmt::format("Instruction address outside capture: {:#x}", address));
        }
        return code.at((address - low) / 8);
    }
    u32 TranslateProgramAddress(u32 address) const override {
        if (!IsNativeProgram())
            return address;
        const auto result = Shader::Ampere::NativeProgramAddress(code, address);
        if (!result)
            throw std::runtime_error("Invalid native indirect branch target");
        return *result;
    }
    bool IsNativeProgram() const override {
        return Shader::Ampere::HasNativeAddressMap(code);
    }
    bool HasNativeInstructions() const override {
        return IsNativeProgram();
    }
    bool IsCbufStorageBufferPointer(u32 bank, u32 offset) override {
        return Lookup(pointers, CbufKey(bank, offset), "storage pointer classification") != 0;
    }
    u32 ReadCbufValue(u32 bank, u32 offset) override {
        return Lookup(cbuf_values, CbufKey(bank, offset), "constant buffer value");
    }
    Shader::TextureType ReadTextureType(u32 handle) override {
        return Lookup(texture_types, handle, "texture type");
    }
    Shader::TexturePixelFormat ReadTexturePixelFormat(u32 handle) override {
        return Lookup(texture_formats, handle, "texture pixel format");
    }
    bool IsTexturePixelFormatInteger(u32 handle) override {
        using F = Shader::TexturePixelFormat;
        switch (ReadTexturePixelFormat(handle)) {
        case F::A8B8G8R8_SINT:
        case F::A8B8G8R8_UINT:
        case F::A2B10G10R10_UINT:
        case F::R8_SINT:
        case F::R8_UINT:
        case F::R16G16B16A16_SINT:
        case F::R16G16B16A16_UINT:
        case F::R32G32B32A32_UINT:
        case F::R32G32B32A32_SINT:
        case F::R32G32_SINT:
        case F::R16_UINT:
        case F::R16_SINT:
        case F::R16G16_UINT:
        case F::R16G16_SINT:
        case F::R8G8_SINT:
        case F::R8G8_UINT:
        case F::R32G32_UINT:
        case F::R32_UINT:
        case F::R32_SINT:
            return true;
        default:
            return false;
        }
    }
    u32 ReadViewportTransformState() override {
        return viewport;
    }
    u32 TextureBoundBuffer() const override {
        if (texture_bound == ~0U)
            throw MissingContext("Uncaptured texture-bound constant buffer");
        return texture_bound;
    }
    u32 LocalMemorySize() const override {
        return local_memory;
    }
    u32 SharedMemorySize() const override {
        return shared_memory;
    }
    std::array<u32, 3> WorkgroupSize() const override {
        return workgroup;
    }
    bool HasHLEMacroState() const override {
        return !replacements.empty();
    }
    std::optional<Shader::ReplaceConstant> GetReplaceConstBuffer(u32 bank, u32 offset) override {
        const auto it = replacements.find(CbufKey(bank, offset));
        return it == replacements.end() ? std::nullopt : std::optional{it->second};
    }
    void Dump(u64, u64) override {}

    bool captured{};
    std::vector<u64> code;

private:
    u32 low{}, high{}, local_memory{}, shared_memory{}, viewport{1}, texture_bound{~0U};
    std::array<u32, 3> workgroup{1, 1, 1};
    std::map<u32, Shader::TextureType> texture_types;
    std::map<u32, Shader::TexturePixelFormat> texture_formats;
    std::map<u64, u32> cbuf_values;
    std::map<u64, u8> pointers;
    std::map<u64, Shader::ReplaceConstant> replacements;
};

} // namespace
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        auto bytes=ReadFile(argv[1]);
        std::vector<Instruction> code(bytes.size()/16);
        std::memcpy(code.data(),bytes.data(),bytes.size());
        const auto lowered=Shader::Ampere::LowerToMaxwell(code,false,true);
        if (!lowered) { std::cerr << lowered.error << " @ " << lowered.error_offset << '\n'; return 1; }
        OfflineEnvironment env;
        env.SetStage(ParseStage(argv[2]));
        env.SetCode(lowered);
        Shader::ObjectPool<Shader::Maxwell::Flow::Block> flow_pool;
        Shader::Maxwell::Flow::CFG cfg{env,flow_pool,Shader::Maxwell::Location{env.EntryPoint()}};
        std::cerr << "CFG constructed from entire lowered program; functions: "
                  << cfg.Functions().size() << '\n';
        Shader::ObjectPool<Shader::IR::Inst> inst_pool;
        Shader::ObjectPool<Shader::IR::Block> block_pool;
        Shader::HostTranslateInfo host{};
        host.support_float64=true; host.support_float16=true; host.support_int64=true;
        host.support_conditional_barrier=true;
        host.support_viewport_index_layer=true;
        host.support_geometry_shader_passthrough=true;
        host.ApplyDescriptorLimitPolicy();
        Shader::IR::Program raw;
        for (const auto& function : cfg.Functions()) {
            for (const auto& block : function.blocks) {
                auto* translated=block_pool.Create(inst_pool);
                Shader::Maxwell::Translate(env,translated,block.begin.Offset(),block.end.Offset());
                raw.blocks.push_back(translated);
            }
        }
        std::cout << Shader::IR::DumpProgram(raw) << std::flush;
        std::cerr << "All CFG blocks translated to raw IR (before resource-dependent optimization)\n";
        auto program=Shader::Maxwell::TranslateProgram(inst_pool,block_pool,env,cfg,host);
        std::cout << Shader::IR::DumpProgram(program);
        std::cerr << "IR translated and optimized\n";
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
