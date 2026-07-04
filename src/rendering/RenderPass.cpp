/**
 * @file RenderPass.cpp
 * @brief Реализация RenderPass с depth buffer
 *
 * PROJECT_RULES.md. Фаза 3.5: Depth buffer.
 */

#include "rendering/RenderPass.h"
#include <stdexcept>
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

#include <array>
#include <cstdlib>

namespace kenga {

namespace {

uint32_t find_memory_type(vk::PhysicalDevice phys_dev, uint32_t type_filter,
                          vk::MemoryPropertyFlags properties)
{
    const auto mem_props = phys_dev.getMemoryProperties();
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    KNG_CRITICAL("Failed to find suitable memory type for depth buffer");
    throw std::runtime_error("Fatal engine error");
}

} // namespace

RenderPass::RenderPass(VulkanContext& context, Swapchain& swapchain)
    : m_context(context)
    , m_swapchain(swapchain)
{
    create_render_pass();
    create_depth_resources();
    create_framebuffers();
}

RenderPass::~RenderPass()
{
    cleanup();
}

void RenderPass::recreate()
{
    cleanup();
    create_render_pass();
    create_depth_resources();
    create_framebuffers();
}

void RenderPass::create_render_pass()
{
    std::array<vk::AttachmentDescription, 2> attachments = {};

    // Color attachment (index 0)
    attachments[0].format = m_swapchain.format();
    attachments[0].samples = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    // Depth attachment (index 1)
    attachments[1].format = depth_format;
    attachments[1].samples = vk::SampleCountFlagBits::e1;
    attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
    attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[1].initialLayout = vk::ImageLayout::eUndefined;
    attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference color_ref;
    color_ref.attachment = 0;
    color_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference depth_ref;
    depth_ref.attachment = 1;
    depth_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                              vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                              vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.srcAccessMask = vk::AccessFlags{};
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                               vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    vk::RenderPassCreateInfo create_info;
    create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    create_info.pAttachments = attachments.data();
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    vk::Result result =
        m_context.device().createRenderPass(&create_info, nullptr, &m_render_pass);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("createRenderPass: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
    KNG_INFO("Render pass created with color + depth attachments");
}

void RenderPass::create_depth_resources()
{
    const vk::Extent2D extent = m_swapchain.extent();

    vk::ImageCreateInfo image_info;
    image_info.imageType = vk::ImageType::e2D;
    image_info.format = depth_format;
    image_info.extent = vk::Extent3D{extent.width, extent.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.tiling = vk::ImageTiling::eOptimal;
    image_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    image_info.sharingMode = vk::SharingMode::eExclusive;
    image_info.initialLayout = vk::ImageLayout::eUndefined;

    const vk::Device dev = m_context.device();

    vk::Result result = dev.createImage(&image_info, nullptr, &m_depth_image);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("createImage (depth): {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    vk::MemoryRequirements mem_req;
    dev.getImageMemoryRequirements(m_depth_image, &mem_req);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_memory_type(
        m_context.physical_device(), mem_req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    result = dev.allocateMemory(&alloc_info, nullptr, &m_depth_image_memory);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("allocateMemory (depth): {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
    dev.bindImageMemory(m_depth_image, m_depth_image_memory, 0);

    vk::ImageViewCreateInfo view_info;
    view_info.image = m_depth_image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = depth_format;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    result = dev.createImageView(&view_info, nullptr, &m_depth_image_view);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("createImageView (depth): {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
    KNG_DEBUG("Depth image created: {}x{}", extent.width, extent.height);
}

void RenderPass::create_framebuffers()
{
    m_framebuffers.resize(m_swapchain.image_count());
    for (size_t i = 0; i < m_framebuffers.size(); ++i) {
        const std::array<vk::ImageView, 2> attachments = {
            m_swapchain.image_views()[i],
            m_depth_image_view,
        };

        vk::FramebufferCreateInfo create_info;
        create_info.renderPass = m_render_pass;
        create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        create_info.pAttachments = attachments.data();
        create_info.width = m_swapchain.extent().width;
        create_info.height = m_swapchain.extent().height;
        create_info.layers = 1;

        vk::Result result =
            m_context.device().createFramebuffer(&create_info, nullptr, &m_framebuffers[i]);
        if (result != vk::Result::eSuccess) {
            KNG_CRITICAL("createFramebuffer: {} at {}", vk::to_string(result), __FUNCTION__);
            throw std::runtime_error("Fatal engine error");
        }
    }
}

void RenderPass::cleanup_depth_resources()
{
    const vk::Device dev = m_context.device();
    if (m_depth_image_view) {
        dev.destroyImageView(m_depth_image_view);
        m_depth_image_view = VK_NULL_HANDLE;
    }
    if (m_depth_image) {
        dev.destroyImage(m_depth_image);
        m_depth_image = VK_NULL_HANDLE;
    }
    if (m_depth_image_memory) {
        dev.freeMemory(m_depth_image_memory);
        m_depth_image_memory = VK_NULL_HANDLE;
    }
}

void RenderPass::cleanup()
{
    for (auto fb : m_framebuffers) {
        m_context.device().destroyFramebuffer(fb);
    }
    m_framebuffers.clear();
    cleanup_depth_resources();
    if (m_render_pass) {
        m_context.device().destroyRenderPass(m_render_pass);
        m_render_pass = VK_NULL_HANDLE;
    }
}

} // namespace kenga
