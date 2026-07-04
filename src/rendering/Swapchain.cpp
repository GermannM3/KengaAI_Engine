/**
 * @file Swapchain.cpp
 * @brief Реализация Swapchain
 */

#include "rendering/Swapchain.h"
#include <stdexcept>
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace kenga {

Swapchain::Swapchain(VulkanContext& context, uint32_t width, uint32_t height)
    : m_context(context)
{
    create_swapchain(width, height);
    create_image_views();
}

Swapchain::~Swapchain()
{
    cleanup();
}

void Swapchain::recreate(uint32_t width, uint32_t height)
{
    cleanup();
    create_swapchain(width, height);
    create_image_views();
}

void Swapchain::create_swapchain(uint32_t width, uint32_t height)
{
    const vk::PhysicalDevice phys = m_context.physical_device();
    const auto capabilities = phys.getSurfaceCapabilitiesKHR(m_context.surface());
    const auto formats = phys.getSurfaceFormatsKHR(m_context.surface());
    const auto present_modes = phys.getSurfacePresentModesKHR(m_context.surface());

    vk::SurfaceFormatKHR surface_format = formats[0];
    for (const auto& f : formats) {
        if (f.format == vk::Format::eB8G8R8A8Srgb &&
            f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            surface_format = f;
            break;
        }
    }
    m_format = surface_format.format;

    vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
    for (const auto& m : present_modes) {
        if (m == vk::PresentModeKHR::eMailbox) {
            present_mode = m;
            break;
        }
    }

    m_extent.width =
        std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    m_extent.height =
        std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info;
    create_info.surface = m_context.surface();
    create_info.minImageCount = image_count;
    create_info.imageFormat = m_format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = m_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    const uint32_t queue_family_indices[] = {m_context.graphics_queue_family(),
                                             m_context.present_queue_family()};
    if (m_context.graphics_queue_family() != m_context.present_queue_family()) {
        create_info.imageSharingMode = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = vk::SharingMode::eExclusive;
    }
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;

    vk::Result result = m_context.device().createSwapchainKHR(&create_info, nullptr, &m_swapchain);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("createSwapchainKHR: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    m_images = m_context.device().getSwapchainImagesKHR(m_swapchain);
}

void Swapchain::create_image_views()
{
    m_image_views.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); ++i) {
        vk::ImageViewCreateInfo create_info;
        create_info.image = m_images[i];
        create_info.viewType = vk::ImageViewType::e2D;
        create_info.format = m_format;
        create_info.components.r = vk::ComponentSwizzle::eIdentity;
        create_info.components.g = vk::ComponentSwizzle::eIdentity;
        create_info.components.b = vk::ComponentSwizzle::eIdentity;
        create_info.components.a = vk::ComponentSwizzle::eIdentity;
        create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        vk::Result result =
            m_context.device().createImageView(&create_info, nullptr, &m_image_views[i]);
        if (result != vk::Result::eSuccess) {
            KNG_CRITICAL("createImageView: {} at {}", vk::to_string(result), __FUNCTION__);
            throw std::runtime_error("Fatal engine error");
        }
    }
}

void Swapchain::cleanup()
{
    for (auto view : m_image_views) {
        m_context.device().destroyImageView(view);
    }
    m_image_views.clear();
    m_images.clear();
    if (m_swapchain) {
        m_context.device().destroySwapchainKHR(m_swapchain);
        m_swapchain = VK_NULL_HANDLE;
    }
}

uint32_t Swapchain::acquire_next_image(vk::Semaphore semaphore)
{
    const auto result = m_context.device().acquireNextImageKHR(
        m_swapchain, std::numeric_limits<uint64_t>::max(), semaphore, vk::Fence{});
    if (result.result == vk::Result::eErrorOutOfDateKHR ||
        result.result == vk::Result::eSuboptimalKHR) {
        return static_cast<uint32_t>(-1);
    }
    if (result.result != vk::Result::eSuccess) {
        KNG_ERROR("Failed to acquire swapchain image");
        return static_cast<uint32_t>(-1);
    }
    return result.value;
}

vk::Result Swapchain::present(vk::Queue queue, uint32_t image_index,
                               vk::Semaphore wait_semaphore)
{
    vk::PresentInfoKHR present_info;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &wait_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swapchain;
    present_info.pImageIndices = &image_index;

    return queue.presentKHR(&present_info);
}

} // namespace kenga
