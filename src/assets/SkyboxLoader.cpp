/**
 * @file SkyboxLoader.cpp
 * @brief Equirect HDR -> cubemap + IBL precomputation
 *
 * PROJECT_RULES.md. Фаза 5: Skybox + IBL.
 */

#include "assets/SkyboxLoader.h"
#include <stdexcept>
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

#include <shaderc/shaderc.hpp>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <sstream>

namespace kenga {

namespace {

uint32_t find_memory_type(vk::PhysicalDevice phys, uint32_t type_filter,
                          vk::MemoryPropertyFlags props)
{
    auto mem = phys.getMemoryProperties();
    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mem.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    KNG_CRITICAL("SkyboxLoader: no suitable memory type");
    throw std::runtime_error("Fatal engine error");
}

vk::CommandBuffer begin_one_shot(vk::Device dev, vk::CommandPool pool)
{
    vk::CommandBufferAllocateInfo ai;
    ai.commandPool = pool;
    ai.level = vk::CommandBufferLevel::ePrimary;
    ai.commandBufferCount = 1;
    vk::CommandBuffer cmd;
    (void)dev.allocateCommandBuffers(&ai, &cmd);
    vk::CommandBufferBeginInfo bi;
    bi.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    (void)cmd.begin(&bi);
    return cmd;
}

void end_one_shot(vk::Device dev, vk::CommandPool pool, vk::Queue queue, vk::CommandBuffer cmd)
{
    (void)cmd.end();
    vk::SubmitInfo si;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    (void)queue.submit(1, &si, VK_NULL_HANDLE);
    (void)queue.waitIdle();
    dev.freeCommandBuffers(pool, 1, &cmd);
}

void transition_image(vk::CommandBuffer cmd, vk::Image image,
                      vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                      vk::AccessFlags src_access, vk::AccessFlags dst_access,
                      vk::PipelineStageFlags src_stage, vk::PipelineStageFlags dst_stage,
                      uint32_t layer_count, uint32_t mip_levels)
{
    vk::ImageMemoryBarrier b;
    b.oldLayout = old_layout;
    b.newLayout = new_layout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = mip_levels;
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount = layer_count;
    b.srcAccessMask = src_access;
    b.dstAccessMask = dst_access;
    cmd.pipelineBarrier(src_stage, dst_stage, {}, 0, nullptr, 0, nullptr, 1, &b);
}

// 6 view matrices for cubemap faces (+X, -X, +Y, -Y, +Z, -Z)
std::array<glm::mat4, 6> cubemap_views()
{
    return {
        glm::lookAt(glm::vec3(0), glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
    };
}

std::string read_file_text(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        KNG_WARN("Cannot open shader: {}", path);
        return {};
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Cube vertices for skybox (position only, 36 verts for 12 triangles)
// Using simple cube with positions in [-1,1]
constexpr float cube_verts[] = {
    // +X
     1,-1,-1,  1,-1, 1,  1, 1, 1,  1, 1, 1,  1, 1,-1,  1,-1,-1,
    // -X
    -1,-1, 1, -1,-1,-1, -1, 1,-1, -1, 1,-1, -1, 1, 1, -1,-1, 1,
    // +Y
    -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1,
    // -Y
    -1,-1, 1,  1,-1, 1,  1,-1,-1,  1,-1,-1, -1,-1,-1, -1,-1, 1,
    // +Z
    -1,-1, 1,  1,-1, 1,  1, 1, 1, -1,-1, 1,  1, 1, 1, -1, 1, 1,
    // -Z  -- note: winding corrected
     1,-1,-1, -1,-1,-1, -1, 1,-1,  1,-1,-1, -1, 1,-1,  1, 1,-1,
};

} // namespace

SkyboxLoader::~SkyboxLoader()
{
    destroy();
}

std::vector<uint32_t> SkyboxLoader::compile_shader(const std::string& path, bool is_vertex)
{
    std::string src = read_file_text(path);
    if (src.empty()) {
        KNG_WARN("Empty shader: {}", path);
        return {};
    }
    shaderc::Compiler compiler;
    shaderc::CompileOptions opts;
    opts.SetOptimizationLevel(shaderc_optimization_level_performance);
    auto kind = is_vertex ? shaderc_vertex_shader : shaderc_fragment_shader;
    auto result = compiler.CompileGlslToSpv(src, kind, path.c_str(), opts);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        KNG_WARN("Shader compile error ({}): {}", path, result.GetErrorMessage());
        return {};
    }
    return {result.cbegin(), result.cend()};
}

void SkyboxLoader::upload_equirect(VulkanContext& ctx, vk::CommandPool pool,
                                   const float* pixels, int w, int h)
{
    m_device = ctx.device();
    const vk::DeviceSize size = static_cast<vk::DeviceSize>(w) * h * 4 * sizeof(float);

    // Staging buffer
    vk::BufferCreateInfo bi;
    bi.size = size;
    bi.usage = vk::BufferUsageFlagBits::eTransferSrc;
    auto staging = m_device.createBuffer(bi);
    auto req = m_device.getBufferMemoryRequirements(staging);
    vk::MemoryAllocateInfo ai;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory_type(ctx.physical_device(), req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    auto staging_mem = m_device.allocateMemory(ai);
    m_device.bindBufferMemory(staging, staging_mem, 0);
    void* data = m_device.mapMemory(staging_mem, 0, size);
    std::memcpy(data, pixels, static_cast<size_t>(size));
    m_device.unmapMemory(staging_mem);

    // Image (R32G32B32A32_SFLOAT for full HDR precision)
    vk::ImageCreateInfo ii;
    ii.imageType = vk::ImageType::e2D;
    ii.format = vk::Format::eR32G32B32A32Sfloat;
    ii.extent = vk::Extent3D{static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = vk::SampleCountFlagBits::e1;
    ii.tiling = vk::ImageTiling::eOptimal;
    ii.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    ii.initialLayout = vk::ImageLayout::eUndefined;
    m_equirect_image = m_device.createImage(ii);

    auto img_req = m_device.getImageMemoryRequirements(m_equirect_image);
    ai.allocationSize = img_req.size;
    ai.memoryTypeIndex = find_memory_type(ctx.physical_device(), img_req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    m_equirect_memory = m_device.allocateMemory(ai);
    m_device.bindImageMemory(m_equirect_image, m_equirect_memory, 0);

    // Copy
    auto cmd = begin_one_shot(m_device, pool);
    transition_image(cmd, m_equirect_image,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
        {}, vk::AccessFlagBits::eTransferWrite,
        vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, 1, 1);

    vk::BufferImageCopy region;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = vk::Extent3D{static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    cmd.copyBufferToImage(staging, m_equirect_image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

    transition_image(cmd, m_equirect_image,
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, 1, 1);
    end_one_shot(m_device, pool, ctx.graphics_queue(), cmd);

    m_device.destroyBuffer(staging);
    m_device.freeMemory(staging_mem);

    // View
    vk::ImageViewCreateInfo vi;
    vi.image = m_equirect_image;
    vi.viewType = vk::ImageViewType::e2D;
    vi.format = vk::Format::eR32G32B32A32Sfloat;
    vi.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    m_equirect_view = m_device.createImageView(vi);

    // Sampler
    vk::SamplerCreateInfo si;
    si.magFilter = vk::Filter::eLinear;
    si.minFilter = vk::Filter::eLinear;
    si.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    si.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    si.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    si.mipmapMode = vk::SamplerMipmapMode::eLinear;
    m_equirect_sampler = m_device.createSampler(si);
}

void SkyboxLoader::create_cubemap_image(vk::Device dev, vk::PhysicalDevice phys,
                                        uint32_t size, uint32_t mip_levels,
                                        vk::Format format,
                                        vk::Image& out_image, vk::DeviceMemory& out_memory,
                                        vk::ImageView& out_view)
{
    vk::ImageCreateInfo ii;
    ii.imageType = vk::ImageType::e2D;
    ii.format = format;
    ii.extent = vk::Extent3D{size, size, 1};
    ii.mipLevels = mip_levels;
    ii.arrayLayers = 6;
    ii.samples = vk::SampleCountFlagBits::e1;
    ii.tiling = vk::ImageTiling::eOptimal;
    ii.usage = vk::ImageUsageFlagBits::eColorAttachment |
               vk::ImageUsageFlagBits::eSampled |
               vk::ImageUsageFlagBits::eTransferDst;
    ii.flags = vk::ImageCreateFlagBits::eCubeCompatible;
    ii.initialLayout = vk::ImageLayout::eUndefined;
    out_image = dev.createImage(ii);

    auto req = dev.getImageMemoryRequirements(out_image);
    vk::MemoryAllocateInfo ai;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory_type(phys, req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    out_memory = dev.allocateMemory(ai);
    dev.bindImageMemory(out_image, out_memory, 0);

    vk::ImageViewCreateInfo vi;
    vi.image = out_image;
    vi.viewType = vk::ImageViewType::eCube;
    vi.format = format;
    vi.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mip_levels, 0, 6};
    out_view = dev.createImageView(vi);
}

void SkyboxLoader::render_cubemap_faces(VulkanContext& ctx, vk::CommandPool pool,
                                        vk::Image dst_image, uint32_t size, uint32_t mip_level,
                                        vk::RenderPass render_pass,
                                        vk::Pipeline pipeline, vk::PipelineLayout layout,
                                        vk::DescriptorSet descriptor_set,
                                        const void* push_data, uint32_t push_size)
{
    auto dev = ctx.device();
    auto views = cubemap_views();
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    // Create temporary vertex buffer for cube
    vk::DeviceSize vb_size = sizeof(cube_verts);
    vk::BufferCreateInfo bi;
    bi.size = vb_size;
    bi.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    auto vb = dev.createBuffer(bi);
    auto req = dev.getBufferMemoryRequirements(vb);
    vk::MemoryAllocateInfo ai;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory_type(ctx.physical_device(), req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    auto vb_mem = dev.allocateMemory(ai);
    dev.bindBufferMemory(vb, vb_mem, 0);
    void* data = dev.mapMemory(vb_mem, 0, vb_size);
    std::memcpy(data, cube_verts, sizeof(cube_verts));
    dev.unmapMemory(vb_mem);

    for (uint32_t face = 0; face < 6; ++face) {
        // Create per-face image view for framebuffer attachment
        vk::ImageViewCreateInfo fvi;
        fvi.image = dst_image;
        fvi.viewType = vk::ImageViewType::e2D;
        fvi.format = hdr_format;
        fvi.subresourceRange = {vk::ImageAspectFlagBits::eColor, mip_level, 1, face, 1};
        auto face_view = dev.createImageView(fvi);

        vk::FramebufferCreateInfo fi;
        fi.renderPass = render_pass;
        fi.attachmentCount = 1;
        fi.pAttachments = &face_view;
        fi.width = size;
        fi.height = size;
        fi.layers = 1;
        auto fb = dev.createFramebuffer(fi);

        auto cmd = begin_one_shot(dev, pool);

        // Transition this face/mip to color attachment
        vk::ImageMemoryBarrier b;
        b.oldLayout = vk::ImageLayout::eUndefined;
        b.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = dst_image;
        b.subresourceRange = {vk::ImageAspectFlagBits::eColor, mip_level, 1, face, 1};
        b.srcAccessMask = {};
        b.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, 0, nullptr, 0, nullptr, 1, &b);

        vk::ClearValue clear;
        clear.color = vk::ClearColorValue(std::array<float,4>{0,0,0,1});
        vk::RenderPassBeginInfo rp;
        rp.renderPass = render_pass;
        rp.framebuffer = fb;
        rp.renderArea.offset = vk::Offset2D{0, 0};
        rp.renderArea.extent = vk::Extent2D{size, size};
        rp.clearValueCount = 1;
        rp.pClearValues = &clear;
        cmd.beginRenderPass(&rp, vk::SubpassContents::eInline);

        vk::Viewport vp{0, 0, static_cast<float>(size), static_cast<float>(size), 0, 1};
        cmd.setViewport(0, 1, &vp);
        vk::Rect2D sc;
        sc.offset = vk::Offset2D{0, 0};
        sc.extent = vk::Extent2D{size, size};
        cmd.setScissor(0, 1, &sc);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, 1, &descriptor_set, 0, nullptr);

        // Push constants: mat4 view_proj + optional extra data
        glm::mat4 mvp = proj * views[face];
        // Push view_proj (64 bytes) + optional extra push data
        uint8_t push_buf[128] = {};
        std::memcpy(push_buf, &mvp, 64);
        if (push_data && push_size > 0) {
            std::memcpy(push_buf + 64, push_data, push_size);
        }
        cmd.pushConstants(layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                          0, 64 + push_size, push_buf);

        vk::Buffer vbs[] = {vb};
        vk::DeviceSize offs[] = {0};
        cmd.bindVertexBuffers(0, 1, vbs, offs);
        cmd.draw(36, 1, 0, 0);

        cmd.endRenderPass();

        // Transition to shader read
        b.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        b.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        b.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        b.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eFragmentShader, {}, 0, nullptr, 0, nullptr, 1, &b);

        end_one_shot(dev, pool, ctx.graphics_queue(), cmd);

        dev.destroyFramebuffer(fb);
        dev.destroyImageView(face_view);
    }

    dev.destroyBuffer(vb);
    dev.freeMemory(vb_mem);
}

void SkyboxLoader::create_brdf_lut(VulkanContext& ctx, vk::CommandPool pool)
{
    auto dev = ctx.device();

    // Create 2D image for BRDF LUT
    vk::ImageCreateInfo ii;
    ii.imageType = vk::ImageType::e2D;
    ii.format = vk::Format::eR16G16Sfloat;
    ii.extent = vk::Extent3D{brdf_lut_size, brdf_lut_size, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = vk::SampleCountFlagBits::e1;
    ii.tiling = vk::ImageTiling::eOptimal;
    ii.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    ii.initialLayout = vk::ImageLayout::eUndefined;
    m_brdf_lut_image = dev.createImage(ii);

    auto req = dev.getImageMemoryRequirements(m_brdf_lut_image);
    vk::MemoryAllocateInfo ai;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory_type(ctx.physical_device(), req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    m_brdf_lut_memory = dev.allocateMemory(ai);
    dev.bindImageMemory(m_brdf_lut_image, m_brdf_lut_memory, 0);

    vk::ImageViewCreateInfo vi;
    vi.image = m_brdf_lut_image;
    vi.viewType = vk::ImageViewType::e2D;
    vi.format = vk::Format::eR16G16Sfloat;
    vi.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    m_brdf_lut_view = dev.createImageView(vi);

    vk::SamplerCreateInfo si;
    si.magFilter = vk::Filter::eLinear;
    si.minFilter = vk::Filter::eLinear;
    si.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    si.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    si.mipmapMode = vk::SamplerMipmapMode::eLinear;
    m_brdf_lut_sampler = dev.createSampler(si);

    // Render pass for BRDF LUT
    vk::AttachmentDescription att;
    att.format = vk::Format::eR16G16Sfloat;
    att.samples = vk::SampleCountFlagBits::e1;
    att.loadOp = vk::AttachmentLoadOp::eClear;
    att.storeOp = vk::AttachmentStoreOp::eStore;
    att.initialLayout = vk::ImageLayout::eUndefined;
    att.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    vk::AttachmentReference ref;
    ref.attachment = 0;
    ref.layout = vk::ImageLayout::eColorAttachmentOptimal;
    vk::SubpassDescription sp;
    sp.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &ref;
    vk::RenderPassCreateInfo rpi;
    rpi.attachmentCount = 1;
    rpi.pAttachments = &att;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sp;
    auto rp = dev.createRenderPass(rpi);

    // Framebuffer
    vk::FramebufferCreateInfo fi;
    fi.renderPass = rp;
    fi.attachmentCount = 1;
    fi.pAttachments = &m_brdf_lut_view;
    fi.width = brdf_lut_size;
    fi.height = brdf_lut_size;
    fi.layers = 1;
    auto fb = dev.createFramebuffer(fi);

    // Pipeline: fullscreen triangle, no vertex input
    auto vert_spirv = compile_shader("shaders/fullscreen.vert", true);
    auto frag_spirv = compile_shader("shaders/brdf_lut.frag", false);
    if (vert_spirv.empty() || frag_spirv.empty()) {
        KNG_WARN("BRDF LUT shader compilation failed");
        dev.destroyFramebuffer(fb);
        dev.destroyRenderPass(rp);
        return;
    }

    vk::ShaderModuleCreateInfo smi;
    smi.codeSize = vert_spirv.size() * 4;
    smi.pCode = vert_spirv.data();
    auto vert_mod = dev.createShaderModule(smi);
    smi.codeSize = frag_spirv.size() * 4;
    smi.pCode = frag_spirv.data();
    auto frag_mod = dev.createShaderModule(smi);

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages;
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = vert_mod;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = frag_mod;
    stages[1].pName = "main";

    vk::PipelineVertexInputStateCreateInfo vertex_input;
    vk::PipelineInputAssemblyStateCreateInfo input_asm;
    input_asm.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo vp_state;
    vp_state.viewportCount = 1;
    vp_state.scissorCount = 1;
    vk::PipelineRasterizationStateCreateInfo rast;
    rast.polygonMode = vk::PolygonMode::eFill;
    rast.cullMode = vk::CullModeFlagBits::eNone;
    rast.frontFace = vk::FrontFace::eCounterClockwise;
    rast.lineWidth = 1.0f;
    vk::PipelineMultisampleStateCreateInfo ms;
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;
    vk::PipelineColorBlendAttachmentState cba;
    cba.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                         vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo cb;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;
    vk::PipelineDepthStencilStateCreateInfo ds;
    std::array<vk::DynamicState, 2> dyn = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dyn_state;
    dyn_state.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dyn_state.pDynamicStates = dyn.data();

    vk::PipelineLayoutCreateInfo pli;
    auto pipe_layout = dev.createPipelineLayout(pli);

    vk::GraphicsPipelineCreateInfo gpi;
    gpi.stageCount = 2;
    gpi.pStages = stages.data();
    gpi.pVertexInputState = &vertex_input;
    gpi.pInputAssemblyState = &input_asm;
    gpi.pViewportState = &vp_state;
    gpi.pRasterizationState = &rast;
    gpi.pMultisampleState = &ms;
    gpi.pColorBlendState = &cb;
    gpi.pDepthStencilState = &ds;
    gpi.pDynamicState = &dyn_state;
    gpi.layout = pipe_layout;
    gpi.renderPass = rp;

    auto [result, pipe] = dev.createGraphicsPipeline(VK_NULL_HANDLE, gpi);
    if (result != vk::Result::eSuccess) {
        KNG_WARN("BRDF LUT pipeline creation failed");
    }

    // Render
    auto cmd = begin_one_shot(dev, pool);
    vk::ClearValue clear;
    clear.color = vk::ClearColorValue(std::array<float,4>{0,0,0,1});
    vk::RenderPassBeginInfo rpbi;
    rpbi.renderPass = rp;
    rpbi.framebuffer = fb;
    rpbi.renderArea.offset = vk::Offset2D{0, 0};
    rpbi.renderArea.extent = vk::Extent2D{brdf_lut_size, brdf_lut_size};
    rpbi.clearValueCount = 1;
    rpbi.pClearValues = &clear;
    cmd.beginRenderPass(&rpbi, vk::SubpassContents::eInline);

    vk::Viewport viewport{0, 0, static_cast<float>(brdf_lut_size), static_cast<float>(brdf_lut_size), 0, 1};
    cmd.setViewport(0, 1, &viewport);
    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = vk::Extent2D{brdf_lut_size, brdf_lut_size};
    cmd.setScissor(0, 1, &scissor);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipe);
    cmd.draw(3, 1, 0, 0);
    cmd.endRenderPass();
    end_one_shot(dev, pool, ctx.graphics_queue(), cmd);

    // Cleanup temporaries
    dev.destroyPipeline(pipe);
    dev.destroyPipelineLayout(pipe_layout);
    dev.destroyShaderModule(vert_mod);
    dev.destroyShaderModule(frag_mod);
    dev.destroyFramebuffer(fb);
    dev.destroyRenderPass(rp);

    KNG_INFO("BRDF LUT generated ({}x{})", brdf_lut_size, brdf_lut_size);
}

bool SkyboxLoader::load(VulkanContext& ctx, vk::CommandPool pool, const std::string& hdr_path)
{
    m_device = ctx.device();

    // Load HDR
    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(true);
    float* pixels = stbi_loadf(hdr_path.c_str(), &w, &h, &channels, 4);
    stbi_set_flip_vertically_on_load(false);
    if (!pixels) {
        KNG_WARN("Failed to load HDR: {}", hdr_path);
        return false;
    }
    KNG_INFO("Loaded HDR: {} ({}x{}, {} channels)", hdr_path, w, h, channels);

    upload_equirect(ctx, pool, pixels, w, h);
    stbi_image_free(pixels);

    // Create render pass for cubemap face rendering (single color, HDR format)
    vk::AttachmentDescription att;
    att.format = hdr_format;
    att.samples = vk::SampleCountFlagBits::e1;
    att.loadOp = vk::AttachmentLoadOp::eClear;
    att.storeOp = vk::AttachmentStoreOp::eStore;
    att.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
    att.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
    vk::AttachmentReference ref;
    ref.attachment = 0;
    ref.layout = vk::ImageLayout::eColorAttachmentOptimal;
    vk::SubpassDescription sp;
    sp.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &ref;
    vk::RenderPassCreateInfo rpi;
    rpi.attachmentCount = 1;
    rpi.pAttachments = &att;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sp;
    auto face_render_pass = m_device.createRenderPass(rpi);

    // Descriptor set layout: single texture sampler
    vk::DescriptorSetLayoutBinding binding;
    binding.binding = 0;
    binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    binding.descriptorCount = 1;
    binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
    vk::DescriptorSetLayoutCreateInfo dsli;
    dsli.bindingCount = 1;
    dsli.pBindings = &binding;
    auto desc_layout = m_device.createDescriptorSetLayout(dsli);

    // Descriptor pool
    vk::DescriptorPoolSize pool_size;
    pool_size.type = vk::DescriptorType::eCombinedImageSampler;
    pool_size.descriptorCount = 4; // equirect, cubemap, irradiance, prefiltered
    vk::DescriptorPoolCreateInfo dpi;
    dpi.maxSets = 4;
    dpi.poolSizeCount = 1;
    dpi.pPoolSizes = &pool_size;
    auto desc_pool = m_device.createDescriptorPool(dpi);

    // Allocate descriptor for equirect
    vk::DescriptorSetAllocateInfo dsai;
    dsai.descriptorPool = desc_pool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &desc_layout;
    vk::DescriptorSet equirect_set;
    (void)m_device.allocateDescriptorSets(&dsai, &equirect_set);

    vk::DescriptorImageInfo dii;
    dii.sampler = m_equirect_sampler;
    dii.imageView = m_equirect_view;
    dii.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    vk::WriteDescriptorSet wds;
    wds.dstSet = equirect_set;
    wds.dstBinding = 0;
    wds.descriptorCount = 1;
    wds.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    wds.pImageInfo = &dii;
    m_device.updateDescriptorSets(1, &wds, 0, nullptr);

    // Vertex input for cube (vec3 position only, stride 12)
    vk::VertexInputBindingDescription vb_desc;
    vb_desc.binding = 0;
    vb_desc.stride = 12; // 3 floats
    vb_desc.inputRate = vk::VertexInputRate::eVertex;
    vk::VertexInputAttributeDescription va_desc;
    va_desc.binding = 0;
    va_desc.location = 0;
    va_desc.format = vk::Format::eR32G32B32Sfloat;
    va_desc.offset = 0;

    // Compile equirect_to_cube shaders
    // The vertex shader outputs local_pos = inPosition, applies push constant view_proj
    // We need a simple cube vertex shader
    auto cube_vert_spirv = compile_shader("shaders/skybox.vert", true);
    auto equirect_frag_spirv = compile_shader("shaders/equirect_to_cube.frag", false);
    if (cube_vert_spirv.empty() || equirect_frag_spirv.empty()) {
        KNG_WARN("Equirect shader compilation failed");
        m_device.destroyDescriptorPool(desc_pool);
        m_device.destroyDescriptorSetLayout(desc_layout);
        m_device.destroyRenderPass(face_render_pass);
        return false;
    }

    // Shader modules
    vk::ShaderModuleCreateInfo smi;
    smi.codeSize = cube_vert_spirv.size() * 4;
    smi.pCode = cube_vert_spirv.data();
    auto vert_mod = m_device.createShaderModule(smi);
    smi.codeSize = equirect_frag_spirv.size() * 4;
    smi.pCode = equirect_frag_spirv.data();
    auto frag_mod = m_device.createShaderModule(smi);

    // Pipeline layout: push constant (mat4) + descriptor set
    vk::PushConstantRange push;
    push.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    push.offset = 0;
    push.size = 64; // mat4
    vk::PipelineLayoutCreateInfo pli;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &desc_layout;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &push;
    auto equirect_pipe_layout = m_device.createPipelineLayout(pli);

    // Pipeline
    std::array<vk::PipelineShaderStageCreateInfo, 2> stages;
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = vert_mod;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = frag_mod;
    stages[1].pName = "main";

    vk::PipelineVertexInputStateCreateInfo vertex_input;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vb_desc;
    vertex_input.vertexAttributeDescriptionCount = 1;
    vertex_input.pVertexAttributeDescriptions = &va_desc;

    vk::PipelineInputAssemblyStateCreateInfo input_asm;
    input_asm.topology = vk::PrimitiveTopology::eTriangleList;
    vk::PipelineViewportStateCreateInfo vp_state;
    vp_state.viewportCount = 1;
    vp_state.scissorCount = 1;
    vk::PipelineRasterizationStateCreateInfo rast;
    rast.polygonMode = vk::PolygonMode::eFill;
    rast.cullMode = vk::CullModeFlagBits::eNone;
    rast.frontFace = vk::FrontFace::eCounterClockwise;
    rast.lineWidth = 1.0f;
    vk::PipelineMultisampleStateCreateInfo ms;
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;
    vk::PipelineColorBlendAttachmentState cba;
    cba.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                         vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo cb;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;
    vk::PipelineDepthStencilStateCreateInfo ds;
    std::array<vk::DynamicState, 2> dyn = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dyn_state;
    dyn_state.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dyn_state.pDynamicStates = dyn.data();

    vk::GraphicsPipelineCreateInfo gpi;
    gpi.stageCount = 2;
    gpi.pStages = stages.data();
    gpi.pVertexInputState = &vertex_input;
    gpi.pInputAssemblyState = &input_asm;
    gpi.pViewportState = &vp_state;
    gpi.pRasterizationState = &rast;
    gpi.pMultisampleState = &ms;
    gpi.pColorBlendState = &cb;
    gpi.pDepthStencilState = &ds;
    gpi.pDynamicState = &dyn_state;
    gpi.layout = equirect_pipe_layout;
    gpi.renderPass = face_render_pass;

    auto [r1, equirect_pipeline] = m_device.createGraphicsPipeline(VK_NULL_HANDLE, gpi);
    if (r1 != vk::Result::eSuccess) {
        KNG_WARN("Equirect pipeline creation failed");
    }

    // ---- Step 1: Create environment cubemap ----
    create_cubemap_image(m_device, ctx.physical_device(), cubemap_size, 1, hdr_format,
                         m_cubemap_image, m_cubemap_memory, m_cubemap_view);
    render_cubemap_faces(ctx, pool, m_cubemap_image, cubemap_size, 0,
                         face_render_pass, equirect_pipeline, equirect_pipe_layout,
                         equirect_set, nullptr, 0);

    vk::SamplerCreateInfo csi;
    csi.magFilter = vk::Filter::eLinear;
    csi.minFilter = vk::Filter::eLinear;
    csi.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    csi.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    csi.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    csi.mipmapMode = vk::SamplerMipmapMode::eLinear;
    m_cubemap_sampler = m_device.createSampler(csi);
    KNG_INFO("Environment cubemap created ({}x{})", cubemap_size, cubemap_size);

    // ---- Step 2: Irradiance map ----
    // Allocate descriptor for cubemap
    vk::DescriptorSet cubemap_set;
    (void)m_device.allocateDescriptorSets(&dsai, &cubemap_set);
    dii.sampler = m_cubemap_sampler;
    dii.imageView = m_cubemap_view;
    wds.dstSet = cubemap_set;
    m_device.updateDescriptorSets(1, &wds, 0, nullptr);

    auto irradiance_frag_spirv = compile_shader("shaders/irradiance_convolve.frag", false);
    if (!irradiance_frag_spirv.empty()) {
        m_device.destroyShaderModule(frag_mod);
        smi.codeSize = irradiance_frag_spirv.size() * 4;
        smi.pCode = irradiance_frag_spirv.data();
        frag_mod = m_device.createShaderModule(smi);
        stages[1].module = frag_mod;

        m_device.destroyPipeline(equirect_pipeline);
        gpi.pStages = stages.data();
        auto [r2, irr_pipe] = m_device.createGraphicsPipeline(VK_NULL_HANDLE, gpi);

        create_cubemap_image(m_device, ctx.physical_device(), irradiance_size, 1, hdr_format,
                             m_irradiance_image, m_irradiance_memory, m_irradiance_view);
        render_cubemap_faces(ctx, pool, m_irradiance_image, irradiance_size, 0,
                             face_render_pass, irr_pipe, equirect_pipe_layout,
                             cubemap_set, nullptr, 0);
        m_irradiance_sampler = m_device.createSampler(csi);
        m_device.destroyPipeline(irr_pipe);
        KNG_INFO("Irradiance map created ({}x{})", irradiance_size, irradiance_size);
    }

    // ---- Step 3: Prefiltered env map ----
    auto prefilter_frag_spirv = compile_shader("shaders/prefilter_env.frag", false);
    if (!prefilter_frag_spirv.empty()) {
        m_device.destroyShaderModule(frag_mod);
        smi.codeSize = prefilter_frag_spirv.size() * 4;
        smi.pCode = prefilter_frag_spirv.data();
        frag_mod = m_device.createShaderModule(smi);
        stages[1].module = frag_mod;

        // Pipeline layout with extra push constant for roughness
        push.size = 68; // mat4 (64) + float roughness (4)
        vk::PipelineLayoutCreateInfo pli2;
        pli2.setLayoutCount = 1;
        pli2.pSetLayouts = &desc_layout;
        pli2.pushConstantRangeCount = 1;
        pli2.pPushConstantRanges = &push;
        auto prefilter_layout = m_device.createPipelineLayout(pli2);
        gpi.layout = prefilter_layout;

        auto [r3, pf_pipe] = m_device.createGraphicsPipeline(VK_NULL_HANDLE, gpi);

        create_cubemap_image(m_device, ctx.physical_device(), prefilter_size, prefilter_mip_levels,
                             hdr_format, m_prefiltered_image, m_prefiltered_memory, m_prefiltered_view);

        for (uint32_t mip = 0; mip < prefilter_mip_levels; ++mip) {
            float roughness = static_cast<float>(mip) / static_cast<float>(prefilter_mip_levels - 1);
            uint32_t mip_size = static_cast<uint32_t>(prefilter_size * std::pow(0.5f, static_cast<float>(mip)));
            mip_size = std::max(mip_size, 1u);
            render_cubemap_faces(ctx, pool, m_prefiltered_image, mip_size, mip,
                                 face_render_pass, pf_pipe, prefilter_layout,
                                 cubemap_set, &roughness, 4);
        }

        csi.maxLod = static_cast<float>(prefilter_mip_levels);
        m_prefiltered_sampler = m_device.createSampler(csi);
        m_device.destroyPipeline(pf_pipe);
        m_device.destroyPipelineLayout(prefilter_layout);
        KNG_INFO("Prefiltered env map created ({}x{}, {} mips)", prefilter_size, prefilter_size, prefilter_mip_levels);
    }

    // ---- Step 4: BRDF LUT ----
    create_brdf_lut(ctx, pool);

    // Cleanup temporary resources
    m_device.destroyShaderModule(vert_mod);
    m_device.destroyShaderModule(frag_mod);
    m_device.destroyPipelineLayout(equirect_pipe_layout);
    m_device.destroyDescriptorPool(desc_pool);
    m_device.destroyDescriptorSetLayout(desc_layout);
    m_device.destroyRenderPass(face_render_pass);

    // Destroy equirect source (no longer needed)
    m_device.destroySampler(m_equirect_sampler);
    m_device.destroyImageView(m_equirect_view);
    m_device.destroyImage(m_equirect_image);
    m_device.freeMemory(m_equirect_memory);
    m_equirect_sampler = VK_NULL_HANDLE;
    m_equirect_view = VK_NULL_HANDLE;
    m_equirect_image = VK_NULL_HANDLE;
    m_equirect_memory = VK_NULL_HANDLE;

    m_loaded = true;
    KNG_INFO("Skybox + IBL fully loaded from {}", hdr_path);
    return true;
}

void SkyboxLoader::destroy()
{
    if (!m_device) return;

    if (m_cubemap_sampler) { m_device.destroySampler(m_cubemap_sampler); m_cubemap_sampler = VK_NULL_HANDLE; }
    if (m_cubemap_view) { m_device.destroyImageView(m_cubemap_view); m_cubemap_view = VK_NULL_HANDLE; }
    if (m_cubemap_image) { m_device.destroyImage(m_cubemap_image); m_cubemap_image = VK_NULL_HANDLE; }
    if (m_cubemap_memory) { m_device.freeMemory(m_cubemap_memory); m_cubemap_memory = VK_NULL_HANDLE; }

    if (m_irradiance_sampler) { m_device.destroySampler(m_irradiance_sampler); m_irradiance_sampler = VK_NULL_HANDLE; }
    if (m_irradiance_view) { m_device.destroyImageView(m_irradiance_view); m_irradiance_view = VK_NULL_HANDLE; }
    if (m_irradiance_image) { m_device.destroyImage(m_irradiance_image); m_irradiance_image = VK_NULL_HANDLE; }
    if (m_irradiance_memory) { m_device.freeMemory(m_irradiance_memory); m_irradiance_memory = VK_NULL_HANDLE; }

    if (m_prefiltered_sampler) { m_device.destroySampler(m_prefiltered_sampler); m_prefiltered_sampler = VK_NULL_HANDLE; }
    if (m_prefiltered_view) { m_device.destroyImageView(m_prefiltered_view); m_prefiltered_view = VK_NULL_HANDLE; }
    if (m_prefiltered_image) { m_device.destroyImage(m_prefiltered_image); m_prefiltered_image = VK_NULL_HANDLE; }
    if (m_prefiltered_memory) { m_device.freeMemory(m_prefiltered_memory); m_prefiltered_memory = VK_NULL_HANDLE; }

    if (m_brdf_lut_sampler) { m_device.destroySampler(m_brdf_lut_sampler); m_brdf_lut_sampler = VK_NULL_HANDLE; }
    if (m_brdf_lut_view) { m_device.destroyImageView(m_brdf_lut_view); m_brdf_lut_view = VK_NULL_HANDLE; }
    if (m_brdf_lut_image) { m_device.destroyImage(m_brdf_lut_image); m_brdf_lut_image = VK_NULL_HANDLE; }
    if (m_brdf_lut_memory) { m_device.freeMemory(m_brdf_lut_memory); m_brdf_lut_memory = VK_NULL_HANDLE; }

    if (m_equirect_sampler) { m_device.destroySampler(m_equirect_sampler); m_equirect_sampler = VK_NULL_HANDLE; }
    if (m_equirect_view) { m_device.destroyImageView(m_equirect_view); m_equirect_view = VK_NULL_HANDLE; }
    if (m_equirect_image) { m_device.destroyImage(m_equirect_image); m_equirect_image = VK_NULL_HANDLE; }
    if (m_equirect_memory) { m_device.freeMemory(m_equirect_memory); m_equirect_memory = VK_NULL_HANDLE; }

    m_loaded = false;
    KNG_INFO("SkyboxLoader resources destroyed");
}

} // namespace kenga
