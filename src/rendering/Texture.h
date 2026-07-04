/**
 * @file Texture.h
 * @brief Vulkan texture: image, view, sampler, loaded from file via stb_image
 *
 * PROJECT_RULES.md. Фаза 3.5: Textures.
 */

#pragma once

#include "rendering/VulkanContext.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <string>

namespace kenga {

/**
 * @brief GPU texture with image, view, sampler.
 *
 * Loads RGBA8 from file (stb_image) or creates a 1x1 fallback.
 */
class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    /// Load texture from file. Returns false on failure (fallback not created).
    bool load_from_file(VulkanContext& context, vk::CommandPool cmd_pool,
                        const std::string& path);

    /// Create a 1x1 white texture (fallback when no texture available)
    void create_white_fallback(VulkanContext& context, vk::CommandPool cmd_pool);

    vk::ImageView image_view() const { return m_image_view; }
    vk::Sampler sampler() const { return m_sampler; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    void destroy();

private:
    void create_from_pixels(VulkanContext& context, vk::CommandPool cmd_pool,
                            const uint8_t* pixels, uint32_t w, uint32_t h);
    void create_sampler(vk::Device dev);

    vk::Device m_device = VK_NULL_HANDLE;
    vk::Image m_image = VK_NULL_HANDLE;
    vk::DeviceMemory m_memory = VK_NULL_HANDLE;
    vk::ImageView m_image_view = VK_NULL_HANDLE;
    vk::Sampler m_sampler = VK_NULL_HANDLE;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace kenga
