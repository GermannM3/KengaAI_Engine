/**
 * @file PostProcess.h
 * @brief Post-processing pipeline: HDR offscreen, bloom, tone mapping
 *
 * PROJECT_RULES.md. Фаза 3.5: Post-processing.
 */

#pragma once

#include "rendering/VulkanContext.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <vector>

namespace kenga {

/**
 * @brief Manages offscreen HDR framebuffer, bloom passes, and tone mapping.
 *
 * Flow: scene renders to HDR offscreen -> bloom threshold -> blur H -> blur V
 *       -> tonemap composite (HDR + bloom) -> swapchain.
 */
class PostProcess {
public:
    PostProcess(VulkanContext& context, uint32_t width, uint32_t height,
                vk::RenderPass tonemap_render_pass);
    ~PostProcess();

    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    void recreate(uint32_t width, uint32_t height);

    /// HDR render pass (scene renders into this)
    vk::RenderPass hdr_render_pass() const { return m_hdr_render_pass; }
    vk::Framebuffer hdr_framebuffer() const { return m_hdr_framebuffer; }

    /// Post-process render pass (fullscreen passes)
    vk::RenderPass post_render_pass() const { return m_post_render_pass; }

    /// Record post-processing commands into the given command buffer.
    /// Call after scene render pass ends.
    /// final_framebuffer = swapchain framebuffer to present to.
    void record_commands(vk::CommandBuffer cmd, vk::Framebuffer final_framebuffer,
                         vk::RenderPass final_render_pass, vk::Extent2D extent,
                         float exposure, float bloom_strength, float bloom_threshold);

    /// Access HDR image view and depth view (for VFX passes)
    vk::ImageView hdr_image_view() const { return m_hdr_image_view; }
    vk::ImageView hdr_depth_view() const { return m_hdr_depth_view; }
    vk::Sampler hdr_sampler() const { return m_hdr_sampler; }

    /// Depth format for HDR render pass
    static constexpr vk::Format depth_format = vk::Format::eD32Sfloat;
    static constexpr vk::Format hdr_format = vk::Format::eR16G16B16A16Sfloat;

private:
    void create_hdr_resources();
    void create_bloom_resources();
    void create_render_passes();
    void create_pipelines();
    void create_descriptor_resources();
    void cleanup();

    VulkanContext& m_context;
    uint32_t m_width;
    uint32_t m_height;
    vk::RenderPass m_tonemap_ext_render_pass; ///< external render pass for tonemap (swapchain)

    // HDR offscreen target
    vk::Image m_hdr_image = VK_NULL_HANDLE;
    vk::DeviceMemory m_hdr_memory = VK_NULL_HANDLE;
    vk::ImageView m_hdr_image_view = VK_NULL_HANDLE;
    vk::Sampler m_hdr_sampler = VK_NULL_HANDLE;

    // HDR depth
    vk::Image m_hdr_depth_image = VK_NULL_HANDLE;
    vk::DeviceMemory m_hdr_depth_memory = VK_NULL_HANDLE;
    vk::ImageView m_hdr_depth_view = VK_NULL_HANDLE;

    vk::RenderPass m_hdr_render_pass = VK_NULL_HANDLE;
    vk::Framebuffer m_hdr_framebuffer = VK_NULL_HANDLE;

    // Bloom intermediate textures (half resolution)
    vk::Image m_bloom_images[2] = {};
    vk::DeviceMemory m_bloom_memory[2] = {};
    vk::ImageView m_bloom_views[2] = {};
    vk::Sampler m_bloom_sampler = VK_NULL_HANDLE;
    vk::Framebuffer m_bloom_framebuffers[2] = {};

    // Post-process render pass (single color attachment, no depth)
    vk::RenderPass m_post_render_pass = VK_NULL_HANDLE;

    // Pipelines
    vk::PipelineLayout m_threshold_layout = VK_NULL_HANDLE;
    vk::Pipeline m_threshold_pipeline = VK_NULL_HANDLE;
    vk::PipelineLayout m_blur_layout = VK_NULL_HANDLE;
    vk::Pipeline m_blur_pipeline = VK_NULL_HANDLE;
    vk::PipelineLayout m_tonemap_layout = VK_NULL_HANDLE;
    vk::Pipeline m_tonemap_pipeline = VK_NULL_HANDLE;

    // Descriptor sets
    vk::DescriptorPool m_descriptor_pool = VK_NULL_HANDLE;
    vk::DescriptorSetLayout m_single_tex_layout = VK_NULL_HANDLE;
    vk::DescriptorSetLayout m_two_tex_layout = VK_NULL_HANDLE;
    vk::DescriptorSet m_threshold_set = VK_NULL_HANDLE;   // reads hdr
    vk::DescriptorSet m_blur_h_set = VK_NULL_HANDLE;      // reads bloom[0]
    vk::DescriptorSet m_blur_v_set = VK_NULL_HANDLE;      // reads bloom[1]
    vk::DescriptorSet m_tonemap_set = VK_NULL_HANDLE;     // reads hdr + bloom[0]
};

} // namespace kenga
