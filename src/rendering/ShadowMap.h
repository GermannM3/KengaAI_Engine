/**
 * @file ShadowMap.h
 * @brief Shadow map texture, render pass, framebuffer
 *
 * PROJECT_RULES.md. Фаза 3.4: Shadow mapping.
 */

#pragma once

#include "rendering/VulkanContext.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>

namespace kenga {

class ShadowMap {
public:
    ShadowMap(VulkanContext& context, uint32_t width, uint32_t height);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    vk::RenderPass render_pass() const { return m_render_pass; }
    vk::Framebuffer framebuffer() const { return m_framebuffer; }
    vk::Image image() const { return m_image; }
    vk::ImageView image_view() const { return m_image_view; }
    vk::Sampler sampler() const { return m_sampler; }
    vk::ImageLayout image_layout() const { return m_image_layout; }

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

private:
    void create_depth_image();
    void create_render_pass();
    void create_framebuffer();
    void create_sampler();
    void cleanup();

    VulkanContext& m_context;
    uint32_t m_width;
    uint32_t m_height;

    vk::Image m_image = VK_NULL_HANDLE;
    vk::DeviceMemory m_image_memory = VK_NULL_HANDLE;
    vk::ImageView m_image_view = VK_NULL_HANDLE;
    vk::RenderPass m_render_pass = VK_NULL_HANDLE;
    vk::Framebuffer m_framebuffer = VK_NULL_HANDLE;
    vk::Sampler m_sampler = VK_NULL_HANDLE;
    vk::ImageLayout m_image_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
};

} // namespace kenga
