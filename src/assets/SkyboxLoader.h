/**
 * @file SkyboxLoader.h
 * @brief Loads equirectangular HDR, converts to cubemap, precomputes IBL maps
 *
 * PROJECT_RULES.md. Фаза 5: Skybox + IBL.
 */

#pragma once

#include "rendering/VulkanContext.h"

#include <vulkan/vulkan.hpp>

#include <string>
#include <vector>

namespace kenga {

/**
 * @brief Loads an equirectangular HDR image and produces:
 *   - Environment cubemap (512x512 per face)
 *   - Irradiance cubemap (32x32 per face)
 *   - Prefiltered environment cubemap (128x128 base, 5 mip levels)
 *   - BRDF integration LUT (512x512 R16G16)
 *
 * All GPU resources are created on load() and destroyed on destroy().
 */
class SkyboxLoader {
public:
    SkyboxLoader() = default;
    ~SkyboxLoader();

    SkyboxLoader(const SkyboxLoader&) = delete;
    SkyboxLoader& operator=(const SkyboxLoader&) = delete;

    /// Load HDR and precompute all IBL textures. Returns false on failure.
    bool load(VulkanContext& ctx, vk::CommandPool pool, const std::string& hdr_path);

    /// Destroy all GPU resources.
    void destroy();

    vk::ImageView cubemap_view() const { return m_cubemap_view; }
    vk::Sampler cubemap_sampler() const { return m_cubemap_sampler; }

    vk::ImageView irradiance_view() const { return m_irradiance_view; }
    vk::Sampler irradiance_sampler() const { return m_irradiance_sampler; }

    vk::ImageView prefiltered_view() const { return m_prefiltered_view; }
    vk::Sampler prefiltered_sampler() const { return m_prefiltered_sampler; }

    vk::ImageView brdf_lut_view() const { return m_brdf_lut_view; }
    vk::Sampler brdf_lut_sampler() const { return m_brdf_lut_sampler; }

    bool is_loaded() const { return m_loaded; }

private:
    /// Upload equirect HDR as a 2D texture, return image+view+memory+sampler
    void upload_equirect(VulkanContext& ctx, vk::CommandPool pool,
                         const float* pixels, int w, int h);

    /// Create a cubemap image with given size and mip levels
    void create_cubemap_image(vk::Device dev, vk::PhysicalDevice phys,
                              uint32_t size, uint32_t mip_levels,
                              vk::Format format,
                              vk::Image& out_image, vk::DeviceMemory& out_memory,
                              vk::ImageView& out_view);

    /// Render 6 faces of a cubemap using a given fragment shader
    void render_cubemap_faces(VulkanContext& ctx, vk::CommandPool pool,
                              vk::Image dst_image, uint32_t size, uint32_t mip_level,
                              vk::RenderPass render_pass,
                              vk::Pipeline pipeline, vk::PipelineLayout layout,
                              vk::DescriptorSet descriptor_set,
                              const void* push_data, uint32_t push_size);

    /// Create the BRDF LUT
    void create_brdf_lut(VulkanContext& ctx, vk::CommandPool pool);

    /// Compile a GLSL shader from file to SPIR-V
    std::vector<uint32_t> compile_shader(const std::string& path, bool is_vertex);

    vk::Device m_device = VK_NULL_HANDLE;
    bool m_loaded = false;

    // Equirect source (temporary, destroyed after cubemap creation)
    vk::Image m_equirect_image = VK_NULL_HANDLE;
    vk::DeviceMemory m_equirect_memory = VK_NULL_HANDLE;
    vk::ImageView m_equirect_view = VK_NULL_HANDLE;
    vk::Sampler m_equirect_sampler = VK_NULL_HANDLE;

    // Environment cubemap (512x512)
    vk::Image m_cubemap_image = VK_NULL_HANDLE;
    vk::DeviceMemory m_cubemap_memory = VK_NULL_HANDLE;
    vk::ImageView m_cubemap_view = VK_NULL_HANDLE;
    vk::Sampler m_cubemap_sampler = VK_NULL_HANDLE;

    // Irradiance cubemap (32x32)
    vk::Image m_irradiance_image = VK_NULL_HANDLE;
    vk::DeviceMemory m_irradiance_memory = VK_NULL_HANDLE;
    vk::ImageView m_irradiance_view = VK_NULL_HANDLE;
    vk::Sampler m_irradiance_sampler = VK_NULL_HANDLE;

    // Prefiltered env cubemap (128x128, 5 mips)
    vk::Image m_prefiltered_image = VK_NULL_HANDLE;
    vk::DeviceMemory m_prefiltered_memory = VK_NULL_HANDLE;
    vk::ImageView m_prefiltered_view = VK_NULL_HANDLE;
    vk::Sampler m_prefiltered_sampler = VK_NULL_HANDLE;

    // BRDF LUT (512x512 R16G16)
    vk::Image m_brdf_lut_image = VK_NULL_HANDLE;
    vk::DeviceMemory m_brdf_lut_memory = VK_NULL_HANDLE;
    vk::ImageView m_brdf_lut_view = VK_NULL_HANDLE;
    vk::Sampler m_brdf_lut_sampler = VK_NULL_HANDLE;

    static constexpr uint32_t cubemap_size = 512;
    static constexpr uint32_t irradiance_size = 32;
    static constexpr uint32_t prefilter_size = 128;
    static constexpr uint32_t prefilter_mip_levels = 5;
    static constexpr uint32_t brdf_lut_size = 512;
    static constexpr vk::Format hdr_format = vk::Format::eR16G16B16A16Sfloat;
};

} // namespace kenga
