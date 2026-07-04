/**
 * @file ShaderCompiler.h
 * @brief Shared GLSL→SPIR-V compiler with on-disk caching
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace kenga {

class ShaderCompiler {
public:
    /// Compile GLSL source to SPIR-V. Caches result to <cache_dir>/<name>.spv.
    /// Returns cached SPIR-V if .spv is newer than the source file.
    static std::vector<uint32_t> compile(const std::string& source,
                                         int shader_kind,
                                         const std::string& name,
                                         const std::string& source_path = "",
                                         const std::string& cache_dir = "shaders");

    /// Load pre-compiled SPIR-V from file. Returns empty vector on failure.
    static std::vector<uint32_t> load_spirv(const std::string& path);

    /// Save SPIR-V to file.
    static bool save_spirv(const std::string& path, const std::vector<uint32_t>& spirv);
};

} // namespace kenga
