/**
 * @file ShadowMap.cpp
 * @brief Shadow map implementation
 */

#include "rendering/ShadowMap.h"
#include <stdexcept>
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

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
    KNG_CRITICAL("ShadowMap: failed to find suitable memory type at {}", __FUNCTION__);
    throw std::runtime_error("Fatal engine error");
}

} // namespace

ShadowMap::ShadowMap(VulkanContext& context, uint32_t width, uint32_t height)
    : m_context(context)
    , m_width(width)
    , m_height(height)
{
    create_depth_image();
    create_render_pass();
    create_framebuffer();
    create_sampler();
    KNG_INFO("Shadow map initialized ({}x{})", width, height);
}

ShadowMap::~ShadowMap()
{
    cleanup();
}

void ShadowMap::create_depth_image()
{
    const vk::Device dev = m_context.device();
    const vk::PhysicalDevice phys = m_context.physical_device();

    vk::ImageCreateInfo img_info;
    img_info.imageType = vk::ImageType::e2D;
    img_info.format = vk::Format::eD32Sfloat;
    img_info.extent = vk::Extent3D{m_width, m_height, 1};
    img_info.mipLevels = 1;
    img_info.arrayLayers = 1;
    img_info.samples = vk::SampleCountFlagBits::e1;
    img_info.tiling = vk::ImageTiling::eOptimal;
    img_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
                     vk::ImageUsageFlagBits::eSampled;
    img_info.sharingMode = vk::SharingMode::eExclusive;
    img_info.initialLayout = vk::ImageLayout::eUndefined;

    vk::Result result = dev.createImage(&img_info, nullptr, &m_image);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("ShadowMap createImage: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    vk::MemoryRequirements mem_req;
    dev.getImageMemoryRequirements(m_image, &mem_req);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex =
        find_memory_type(phys, mem_req.memoryTypeBits,
                         vk::MemoryPropertyFlagBits::eDeviceLocal);

    result = dev.allocateMemory(&alloc_info, nullptr, &m_image_memory);
    if (result != vk::Result::eSuccess) {
        dev.destroyImage(m_image);
        KNG_CRITICAL("ShadowMap allocateMemory: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    dev.bindImageMemory(m_image, m_image_memory, 0);

    vk::ImageViewCreateInfo view_info;
    view_info.image = m_image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = vk::Format::eD32Sfloat;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    result = dev.createImageView(&view_info, nullptr, &m_image_view);
    if (result != vk::Result::eSuccess) {
        dev.freeMemory(m_image_memory);
        dev.destroyImage(m_image);
        KNG_CRITICAL("ShadowMap createImageView: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
}

void ShadowMap::create_render_pass()
{
    vk::AttachmentDescription depth_attachment;
    depth_attachment.format = vk::Format::eD32Sfloat;
    depth_attachment.samples = vk::SampleCountFlagBits::e1;
    depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    depth_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
    depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference depth_ref;
    depth_ref.attachment = 0;
    depth_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depth_ref;

    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eTopOfPipe;
    dependency.srcAccessMask = vk::AccessFlags{};
    dependency.dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    vk::RenderPassCreateInfo create_info;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &depth_attachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    vk::Result result =
        m_context.device().createRenderPass(&create_info, nullptr, &m_render_pass);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("ShadowMap createRenderPass: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
}

void ShadowMap::create_framebuffer()
{
    vk::FramebufferCreateInfo create_info;
    create_info.renderPass = m_render_pass;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &m_image_view;
    create_info.width = m_width;
    create_info.height = m_height;
    create_info.layers = 1;

    vk::Result result =
        m_context.device().createFramebuffer(&create_info, nullptr, &m_framebuffer);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("ShadowMap createFramebuffer: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
}

void ShadowMap::create_sampler()
{
    vk::SamplerCreateInfo info;
    info.magFilter = vk::Filter::eLinear;
    info.minFilter = vk::Filter::eLinear;
    info.mipmapMode = vk::SamplerMipmapMode::eNearest;
    info.addressModeU = vk::SamplerAddressMode::eClampToBorder;
    info.addressModeV = vk::SamplerAddressMode::eClampToBorder;
    info.addressModeW = vk::SamplerAddressMode::eClampToBorder;
    info.mipLodBias = 0.0f;
    info.compareEnable = VK_TRUE;
    info.compareOp = vk::CompareOp::eLessOrEqual;
    info.minLod = 0.0f;
    info.maxLod = 1.0f;
    info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    info.anisotropyEnable = VK_FALSE;

    vk::Result result = m_context.device().createSampler(&info, nullptr, &m_sampler);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("ShadowMap createSampler: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
}

void ShadowMap::cleanup()
{
    const vk::Device dev = m_context.device();
    if (!dev) {
        return;
    }
    if (m_sampler) {
        dev.destroySampler(m_sampler);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_framebuffer) {
        dev.destroyFramebuffer(m_framebuffer);
        m_framebuffer = VK_NULL_HANDLE;
    }
    if (m_render_pass) {
        dev.destroyRenderPass(m_render_pass);
        m_render_pass = VK_NULL_HANDLE;
    }
    if (m_image_view) {
        dev.destroyImageView(m_image_view);
        m_image_view = VK_NULL_HANDLE;
    }
    if (m_image) {
        dev.destroyImage(m_image);
        dev.freeMemory(m_image_memory);
        m_image = VK_NULL_HANDLE;
        m_image_memory = VK_NULL_HANDLE;
    }
}

} // namespace kenga
