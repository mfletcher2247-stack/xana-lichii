#include <fstream>
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
#include "shader_recompiler/frontend/ampere/lower_to_maxwell.h"
int main(int argc, char** argv) {
    using Shader::Ampere::Instruction;
    if (argc != 3) return 2;
    std::ifstream in(argv[1], std::ios::binary | std::ios::ate);
    const auto size = in.tellg();
    if (!in || size <= 0 || size % 16) return 2;
    std::vector<Instruction> code(static_cast<size_t>(size) / 16);
    in.seekg(0); in.read(reinterpret_cast<char*>(code.data()), size);
    // Match shader-sweep's default native non-geometry lowering, including tensor emulation.
    const auto result = Shader::Ampere::LowerToMaxwell(code, false, true);
    nlohmann::json out{{"source_bytes", static_cast<size_t>(size)}, {"status", result ? "ok" : "reject"},
        {"offset", result.error_offset}, {"reason", result.error},
        {"lowered_words", result.instructions.size()}};
    if (result) {
        std::ofstream file(argv[2], std::ios::binary);
        file.write(reinterpret_cast<const char*>(result.instructions.data()), result.instructions.size()*8);
        out["source_map"] = result.source_instruction_offsets;
    }
    std::cout << out.dump() << '\n';
    return result ? 0 : 1;
}
