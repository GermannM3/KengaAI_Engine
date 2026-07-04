/**
 * @file Texture.cpp
 * @brief Vulkan texture loading via stb_image
 *
 * PROJECT_RULES.md. Фаза 3.5: Textures.
 */

#include "rendering/Texture.h"
#include <stdexcept>
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

#include <stb_image.h>

#include <cstdlib>
#include <cstring>

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
    KNG_CRITICAL("Failed to find suitable memory type for texture");
    throw std::runtime_error("Fatal engine error");
}

} // namespace

Texture::~Texture()
{
    destroy();
}

Texture::Texture(Texture&& other) noexcept
    : m_device(other.m_device)
    , m_image(other.m_image)
    , m_memory(other.m_memory)
    , m_image_view(other.m_image_view)
    , m_sampler(other.m_sampler)
    , m_width(other.m_width)
    , m_height(other.m_height)
{
    other.m_device = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_image_view = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_device = other.m_device;
        m_image = other.m_image;
        m_memory = other.m_memory;
        m_image_view = other.m_image_view;
        m_sampler = other.m_sampler;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_device = VK_NULL_HANDLE;
        other.m_image = VK_NULL_HANDLE;
        other.m_memory = VK_NULL_HANDLE;
        other.m_image_view = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
    }
    return *this;
}

bool Texture::load_from_file(VulkanContext& context, vk::CommandPool cmd_pool,
                             const std::string& path)
{
    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        KNG_WARN("Failed to load texture: {}", path);
        return false;
    }

    create_from_pixels(context, cmd_pool, pixels, static_cast<uint32_t>(w),
                       static_cast<uint32_t>(h));
    stbi_image_free(pixels);
    KNG_INFO("Loaded texture: {} ({}x{})", path, m_width, m_height);
    return true;
}

void Texture::create_white_fallback(VulkanContext& context, vk::CommandPool cmd_pool)
{
    const uint8_t white[4] = {255, 255, 255, 255};
    create_from_pixels(context, cmd_pool, white, 1, 1);
    KNG_DEBUG("Created 1x1 white fallback texture");
}

void Texture::create_from_pixels(VulkanContext& context, vk::CommandPool cmd_pool,
                                 const uint8_t* pixels, uint32_t w, uint32_t h)
{
    m_device = context.device();
    m_width = w;
    m_height = h;
    const vk::DeviceSize image_size = static_cast<vk::DeviceSize>(w) * h * 4;

    // Staging buffer
    vk::BufferCreateInfo buf_info;
    buf_info.size = image_size;
    buf_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
    buf_info.sharingMode = vk::SharingMode::eExclusive;

    vk::Buffer staging_buf;
    vk::Result r = m_device.createBuffer(&buf_info, nullptr, &staging_buf);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("Texture staging buffer: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }

    vk::MemoryRequirements mem_req;
    m_device.getBufferMemoryRequirements(staging_buf, &mem_req);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_memory_type(
        context.physical_device(), mem_req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    vk::DeviceMemory staging_mem;
    r = m_device.allocateMemory(&alloc_info, nullptr, &staging_mem);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("Texture staging memory: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }
    m_device.bindBufferMemory(staging_buf, staging_mem, 0);

    void* data = nullptr;
    m_device.mapMemory(staging_mem, 0, image_size, {}, &data);
    std::memcpy(data, pixels, static_cast<size_t>(image_size));
    m_device.unmapMemory(staging_mem);

    // Image
    vk::ImageCreateInfo img_info;
    img_info.imageType = vk::ImageType::e2D;
    img_info.format = vk::Format::eR8G8B8A8Srgb;
    img_info.extent = vk::Extent3D{w, h, 1};
    img_info.mipLevels = 1;
    img_info.arrayLayers = 1;
    img_info.samples = vk::SampleCountFlagBits::e1;
    img_info.tiling = vk::ImageTiling::eOptimal;
    img_info.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    img_info.sharingMode = vk::SharingMode::eExclusive;
    img_info.initialLayout = vk::ImageLayout::eUndefined;

    r = m_device.createImage(&img_info, nullptr, &m_image);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("Texture createImage: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }

    m_device.getImageMemoryRequirements(m_image, &mem_req);
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_memory_type(
        context.physical_device(), mem_req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    r = m_device.allocateMemory(&alloc_info, nullptr, &m_memory);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("Texture image memory: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }
    m_device.bindImageMemory(m_image, m_memory, 0);

    // One-shot command buffer: transition + copy + transition
    vk::CommandBufferAllocateInfo cmd_alloc;
    cmd_alloc.commandPool = cmd_pool;
    cmd_alloc.level = vk::CommandBufferLevel::ePrimary;
    cmd_alloc.commandBufferCount = 1;

    vk::CommandBuffer cmd;
    m_device.allocateCommandBuffers(&cmd_alloc, &cmd);

    vk::CommandBufferBeginInfo begin_info;
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(&begin_info);

    // Undefined -> TransferDst
    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = vk::AccessFlags{};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::DependencyFlags{}, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy buffer to image
    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{w, h, 1};

    cmd.copyBufferToImage(staging_buf, m_image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

    // TransferDst -> ShaderReadOnlyOptimal
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::DependencyFlags{}, 0, nullptr, 0, nullptr, 1, &barrier);

    cmd.end();

    vk::SubmitInfo submit;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    context.graphics_queue().submit(1, &submit, VK_NULL_HANDLE);
    context.graphics_queue().waitIdle();

    m_device.freeCommandBuffers(cmd_pool, 1, &cmd);
    m_device.destroyBuffer(staging_buf);
    m_device.freeMemory(staging_mem);

    // Image view
    vk::ImageViewCreateInfo view_info;
    view_info.image = m_image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = vk::Format::eR8G8B8A8Srgb;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    r = m_device.createImageView(&view_info, nullptr, &m_image_view);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("Texture createImageView: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }

    create_sampler(m_device);
}

void Texture::create_sampler(vk::Device dev)
{
    vk::SamplerCreateInfo sampler_info;
    sampler_info.magFilter = vk::Filter::eLinear;
    sampler_info.minFilter = vk::Filter::eLinear;
    sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
    sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
    sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = vk::BorderColor::eIntOpaqueBlack;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    vk::Result r = dev.createSampler(&sampler_info, nullptr, &m_sampler);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("Texture createSampler: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }
}

void Texture::destroy()
{
    if (!m_device) {
        return;
    }
    if (m_sampler) {
        m_device.destroySampler(m_sampler);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_image_view) {
        m_device.destroyImageView(m_image_view);
        m_image_view = VK_NULL_HANDLE;
    }
    if (m_image) {
        m_device.destroyImage(m_image);
        m_image = VK_NULL_HANDLE;
    }
    if (m_memory) {
        m_device.freeMemory(m_memory);
        m_memory = VK_NULL_HANDLE;
    }
}

} // namespace kenga
