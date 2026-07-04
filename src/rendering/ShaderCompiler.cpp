/**
 * @file ShaderCompiler.cpp
 * @brief Shared GLSL→SPIR-V compiler with on-disk caching
 */

#include "rendering/ShaderCompiler.h"
#include "core/LoggerMacros.h"

#include <shaderc/shaderc.hpp>
#include <fstream>
#include <filesystem>
#include <chrono>

namespace kenga {

std::vector<uint32_t> ShaderCompiler::compile(const std::string& source,
                                               int shader_kind,
                                               const std::string& name,
                                               const std::string& source_path,
                                               const std::string& cache_dir)
{
    namespace fs = std::filesystem;

    // Build cache path
    const std::string spirv_path = cache_dir + "/" + name + ".spv";

    // Try to load cached SPIR-V if source path is known
    if (!source_path.empty()) {
        const auto source_time = fs::last_write_time(source_path);
        if (fs::exists(spirv_path)) {
            const auto spirv_time = fs::last_write_time(spirv_path);
            if (spirv_time >= source_time) {
                auto cached = load_spirv(spirv_path);
                if (!cached.empty()) {
                    KNG_DEBUG("Loaded cached SPIR-V: {}", spirv_path);
                    return cached;
                }
            }
        }
    }

    // Compile GLSL → SPIR-V
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

    const auto kind = static_cast<shaderc_shader_kind>(shader_kind);
    const shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
        source, kind, name.c_str(), options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        KNG_CRITICAL("Shader compilation failed for '{}': {}", name, result.GetErrorMessage());
        return {};
    }

    std::vector<uint32_t> spirv(result.cbegin(), result.cend());
    KNG_DEBUG("Compiled shader: {} ({} words)", name, spirv.size());

    // Cache to disk
    fs::create_directories(cache_dir);
    save_spirv(spirv_path, spirv);

    return spirv;
}

std::vector<uint32_t> ShaderCompiler::load_spirv(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }

    const auto size = file.tellg();
    if (size <= 0 || size % 4 != 0) {
        return {};
    }

    file.seekg(0);
    std::vector<uint32_t> spirv(size / 4);
    file.read(reinterpret_cast<char*>(spirv.data()), size);

    return spirv;
}

bool ShaderCompiler::save_spirv(const std::string& path, const std::vector<uint32_t>& spirv)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        KNG_WARN("Failed to save SPIR-V to '{}'", path);
        return false;
    }

    file.write(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(uint32_t));
    return true;
}

} // namespace kenga
