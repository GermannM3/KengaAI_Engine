/**
 * @file RenderPass.h
 * @brief Vulkan render pass, framebuffers, depth buffer
 *
 * PROJECT_RULES.md. Фаза 3.5: Depth buffer.
 */

#pragma once

#include "rendering/Swapchain.h"
#include "rendering/VulkanContext.h"

#include <vulkan/vulkan.hpp>

#include <vector>

namespace kenga {

class RenderPass {
public:
    RenderPass(VulkanContext& context, Swapchain& swapchain);
    ~RenderPass();

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    void recreate();

    vk::RenderPass get() const { return m_render_pass; }
    vk::Framebuffer framebuffer(size_t index) const { return m_framebuffers[index]; }
    size_t framebuffer_count() const { return m_framebuffers.size(); }

    /// @brief Depth format used by the render pass
    static constexpr vk::Format depth_format = vk::Format::eD32Sfloat;

private:
    void create_render_pass();
    void create_depth_resources();
    void create_framebuffers();
    void cleanup();
    void cleanup_depth_resources();

    VulkanContext& m_context;
    Swapchain& m_swapchain;
    vk::RenderPass m_render_pass = VK_NULL_HANDLE;
    std::vector<vk::Framebuffer> m_framebuffers;

    vk::Image m_depth_image = VK_NULL_HANDLE;
    vk::DeviceMemory m_depth_image_memory = VK_NULL_HANDLE;
    vk::ImageView m_depth_image_view = VK_NULL_HANDLE;
};

} // namespace kenga
