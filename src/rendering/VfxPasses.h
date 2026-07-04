/**
 * @file VfxPasses.h
 * @brief Advanced VFX post-process passes: god rays, volumetric fog, SSR, decals
 *
 * PROJECT_RULES.md. Phase 9: Advanced VFX + shaders.
 */

#pragma once

#include "rendering/VulkanContext.h"

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace kenga {

/// @brief VFX settings exposed to ImGui
struct VfxSettings {
    // God rays
    bool god_rays_enabled = true;
    glm::vec2 light_screen_pos = {0.5f, 0.3f}; ///< updated from light direction + camera
    float god_rays_density = 1.0f;
    float god_rays_weight = 0.04f;
    float god_rays_decay = 0.97f;
    float god_rays_exposure = 0.3f;
    int god_rays_samples = 60;

    // Volumetric fog
    bool fog_enabled = true;
    glm::vec3 fog_color = {0.6f, 0.65f, 0.75f};
    float fog_density = 0.03f;
    float fog_start = 5.0f;
    float fog_end = 100.0f;
    float fog_height = 0.0f;
    float fog_height_falloff = 0.1f;

    // SSR
    bool ssr_enabled = false;  ///< off by default (expensive)
    float ssr_step_size = 0.5f;
    float ssr_max_distance = 50.0f;
    float ssr_thickness = 0.5f;
    float ssr_intensity = 0.3f;
    int ssr_max_steps = 32;
};

/**
 * @brief Manages advanced VFX post-process passes.
 *
 * These passes run between the main HDR scene render and the tonemap pass.
 * They read from the HDR scene texture and depth buffer, and write to
 * intermediate targets that feed into the final composite.
 */
class VfxPasses {
public:
    VfxPasses(VulkanContext& context, uint32_t width, uint32_t height,
              vk::RenderPass post_render_pass,
              vk::ImageView hdr_view, vk::Sampler hdr_sampler,
              vk::ImageView depth_view);
    ~VfxPasses();

    VfxPasses(const VfxPasses&) = delete;
    VfxPasses& operator=(const VfxPasses&) = delete;

    void recreate(uint32_t width, uint32_t height,
                  vk::ImageView hdr_view, vk::ImageView depth_view);

    /// Record VFX passes. Returns the image view to use as input for tonemap
    /// (either the original HDR view or the VFX-processed intermediate).
    /// Call after scene render, before tonemap.
    void record_commands(vk::CommandBuffer cmd, const VfxSettings& settings,
                         float near_plane, float far_plane, float camera_y,
                         const glm::mat4& inv_view_proj, const glm::mat4& view_proj);

    /// Get the output image view (after VFX processing)
    vk::ImageView output_view() const { return m_vfx_views[m_output_index]; }

    VfxSettings& settings() { return m_settings; }
    const VfxSettings& settings() const { return m_settings; }

private:
    void create_resources();
    void create_pipelines();
    void create_descriptors();
    void cleanup();

    VulkanContext& m_context;
    uint32_t m_width;
    uint32_t m_height;
    vk::RenderPass m_post_render_pass;
    vk::Sampler m_hdr_sampler;
    vk::ImageView m_hdr_view;
    vk::ImageView m_depth_view;

    VfxSettings m_settings;

    // Ping-pong intermediate targets (full resolution)
    vk::Image m_vfx_images[2] = {};
    vk::DeviceMemory m_vfx_memory[2] = {};
    vk::ImageView m_vfx_views[2] = {};
    vk::Framebuffer m_vfx_framebuffers[2] = {};
    vk::Sampler m_vfx_sampler = VK_NULL_HANDLE;
    int m_output_index = 0;

    // Depth sampler (for fog and SSR)
    vk::Sampler m_depth_sampler = VK_NULL_HANDLE;

    // God rays
    vk::PipelineLayout m_god_rays_layout = VK_NULL_HANDLE;
    vk::Pipeline m_god_rays_pipeline = VK_NULL_HANDLE;

    // Fog
    vk::PipelineLayout m_fog_layout = VK_NULL_HANDLE;
    vk::Pipeline m_fog_pipeline = VK_NULL_HANDLE;

    // SSR
    vk::PipelineLayout m_ssr_layout = VK_NULL_HANDLE;
    vk::Pipeline m_ssr_pipeline = VK_NULL_HANDLE;

    // Descriptors
    vk::DescriptorPool m_desc_pool = VK_NULL_HANDLE;
    vk::DescriptorSetLayout m_single_tex_layout = VK_NULL_HANDLE;
    vk::DescriptorSetLayout m_two_tex_layout = VK_NULL_HANDLE;

    // Descriptor sets: [0] reads HDR, [1] reads vfx[0], [2] reads vfx[1]
    // For two-tex: [0] reads HDR+depth, [1] reads vfx[0]+depth, [2] reads vfx[1]+depth
    vk::DescriptorSet m_single_sets[3] = {};
    vk::DescriptorSet m_two_tex_sets[3] = {};
};

} // namespace kenga
