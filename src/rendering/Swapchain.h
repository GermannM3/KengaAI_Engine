/**
 * @file Swapchain.h
 * @brief Vulkan swapchain, images, image views
 *
 * PROJECT_RULES.md. Фаза 3.1: Rendering.
 */

#pragma once

#include "rendering/VulkanContext.h"

#include <vulkan/vulkan.hpp>

#include <vector>

namespace kenga {

class Swapchain {
public:
    Swapchain(VulkanContext& context, uint32_t width, uint32_t height);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void recreate(uint32_t width, uint32_t height);

    vk::SwapchainKHR get() const { return m_swapchain; }
    vk::Format format() const { return m_format; }
    vk::Extent2D extent() const { return m_extent; }
    const std::vector<vk::ImageView>& image_views() const { return m_image_views; }
    uint32_t image_count() const { return static_cast<uint32_t>(m_image_views.size()); }

    uint32_t acquire_next_image(vk::Semaphore semaphore);
    vk::Result present(vk::Queue queue, uint32_t image_index, vk::Semaphore wait_semaphore);

private:
    void create_swapchain(uint32_t width, uint32_t height);
    void create_image_views();
    void cleanup();

    VulkanContext& m_context;
    vk::SwapchainKHR m_swapchain = VK_NULL_HANDLE;
    vk::Format m_format = vk::Format::eB8G8R8A8Srgb;
    vk::Extent2D m_extent;
    std::vector<vk::Image> m_images;
    std::vector<vk::ImageView> m_image_views;
};

} // namespace kenga
