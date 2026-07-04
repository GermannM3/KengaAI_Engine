/**
 * @file PostProcess.cpp
 * @brief Post-processing: HDR offscreen, bloom, ACES tone mapping
 *
 * PROJECT_RULES.md. Фаза 3.5: Post-processing.
 */

#include "rendering/PostProcess.h"
#include "rendering/Renderer.h"
#include "rendering/ShaderCompiler.h"
#include "core/LoggerMacros.h"
#include "core/Profiler.h"
#include "core/Settings.h"
#include <shaderc/shaderc.hpp>

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

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
    KNG_CRITICAL("PostProcess: failed to find memory type");
    throw std::runtime_error("Fatal engine error");
}

std::string load_file(const std::string& path)
{
    std::ifstream f(path);
    if (!f) return {};
    std::stringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

std::vector<uint32_t> compile_glsl(const std::string& source, shaderc_shader_kind kind,
                                   const char* name)
{
    return ShaderCompiler::compile(source, static_cast<int>(kind), name);
}

std::string try_load_shader(const char* filename)
{
    const std::array<std::string, 4> paths = {
        std::string("shaders/") + filename,
        std::string("src/rendering/shaders/") + filename,
        std::string("../src/rendering/shaders/") + filename,
        std::string("../../src/rendering/shaders/") + filename,
    };
    for (const auto& p : paths) {
        std::string s = load_file(p);
        if (!s.empty()) return s;
    }
    return {};
}

struct ImageResources {
    vk::Image image;
    vk::DeviceMemory memory;
    vk::ImageView view;
};

ImageResources create_image_2d(VulkanContext& ctx, uint32_t w, uint32_t h, vk::Format fmt,
                               vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect)
{
    ImageResources res = {};
    const vk::Device dev = ctx.device();

    vk::ImageCreateInfo info;
    info.imageType = vk::ImageType::e2D;
    info.format = fmt;
    info.extent = vk::Extent3D{w, h, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = vk::ImageTiling::eOptimal;
    info.usage = usage;
    info.sharingMode = vk::SharingMode::eExclusive;
    info.initialLayout = vk::ImageLayout::eUndefined;

    vk::Result r = dev.createImage(&info, nullptr, &res.image);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("PostProcess createImage: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }

    vk::MemoryRequirements mem_req;
    dev.getImageMemoryRequirements(res.image, &mem_req);

    vk::MemoryAllocateInfo alloc;
    alloc.allocationSize = mem_req.size;
    alloc.memoryTypeIndex = find_memory_type(ctx.physical_device(), mem_req.memoryTypeBits,
                                             vk::MemoryPropertyFlagBits::eDeviceLocal);
    r = dev.allocateMemory(&alloc, nullptr, &res.memory);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("PostProcess allocMemory: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }
    dev.bindImageMemory(res.image, res.memory, 0);

    vk::ImageViewCreateInfo vi;
    vi.image = res.image;
    vi.viewType = vk::ImageViewType::e2D;
    vi.format = fmt;
    vi.subresourceRange.aspectMask = aspect;
    vi.subresourceRange.baseMipLevel = 0;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.baseArrayLayer = 0;
    vi.subresourceRange.layerCount = 1;

    r = dev.createImageView(&vi, nullptr, &res.view);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("PostProcess createImageView: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }
    return res;
}

void destroy_image_resources(vk::Device dev, ImageResources& res)
{
    if (res.view) { dev.destroyImageView(res.view); res.view = VK_NULL_HANDLE; }
    if (res.image) { dev.destroyImage(res.image); res.image = VK_NULL_HANDLE; }
    if (res.memory) { dev.freeMemory(res.memory); res.memory = VK_NULL_HANDLE; }
}

vk::Sampler create_linear_sampler(vk::Device dev)
{
    vk::SamplerCreateInfo si;
    si.magFilter = vk::Filter::eLinear;
    si.minFilter = vk::Filter::eLinear;
    si.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    si.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    si.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    si.anisotropyEnable = VK_FALSE;
    si.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    si.mipmapMode = vk::SamplerMipmapMode::eLinear;

    vk::Sampler sampler;
    vk::Result r = dev.createSampler(&si, nullptr, &sampler);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("PostProcess createSampler: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }
    return sampler;
}

vk::ShaderModule create_shader_module(vk::Device dev, const std::vector<uint32_t>& spirv)
{
    vk::ShaderModuleCreateInfo ci;
    ci.codeSize = spirv.size() * sizeof(uint32_t);
    ci.pCode = spirv.data();
    vk::ShaderModule mod;
    vk::Result r = dev.createShaderModule(&ci, nullptr, &mod);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("PostProcess createShaderModule: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }
    return mod;
}

vk::Pipeline create_fullscreen_pipeline(vk::Device dev, vk::RenderPass rp,
                                        vk::PipelineLayout layout,
                                        const std::vector<uint32_t>& vert_spv,
                                        const std::vector<uint32_t>& frag_spv)
{
    vk::ShaderModule vert_mod = create_shader_module(dev, vert_spv);
    vk::ShaderModule frag_mod = create_shader_module(dev, frag_spv);

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = vert_mod;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = frag_mod;
    stages[1].pName = "main";

    vk::PipelineVertexInputStateCreateInfo vertex_input; // no vertex buffers
    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo vp_state;
    vp_state.viewportCount = 1;
    vp_state.scissorCount = 1;

    const std::array<vk::DynamicState, 2> dyn = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dyn_state;
    dyn_state.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dyn_state.pDynamicStates = dyn.data();

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;

    vk::PipelineMultisampleStateCreateInfo ms;
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState blend_att;
    blend_att.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                               vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    blend_att.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo blend;
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_att;

    vk::GraphicsPipelineCreateInfo pi;
    pi.stageCount = 2;
    pi.pStages = stages.data();
    pi.pVertexInputState = &vertex_input;
    pi.pInputAssemblyState = &input_assembly;
    pi.pViewportState = &vp_state;
    pi.pRasterizationState = &rasterizer;
    pi.pMultisampleState = &ms;
    pi.pColorBlendState = &blend;
    pi.pDynamicState = &dyn_state;
    pi.layout = layout;
    pi.renderPass = rp;
    pi.subpass = 0;

    vk::Pipeline pipeline;
    vk::Result r = dev.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pi, nullptr, &pipeline);

    dev.destroyShaderModule(frag_mod);
    dev.destroyShaderModule(vert_mod);

    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("PostProcess createPipeline: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }
    return pipeline;
}

} // namespace

PostProcess::PostProcess(VulkanContext& context, uint32_t width, uint32_t height,
                         vk::RenderPass tonemap_render_pass)
    : m_context(context)
    , m_width(width)
    , m_height(height)
    , m_tonemap_ext_render_pass(tonemap_render_pass)
{
    create_render_passes();
    create_hdr_resources();
    create_bloom_resources();
    create_descriptor_resources();
    create_pipelines();
    KNG_INFO("PostProcess initialized ({}x{})", width, height);
}

PostProcess::~PostProcess()
{
    cleanup();
}

void PostProcess::recreate(uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;
    const vk::Device dev = m_context.device();
    dev.waitIdle();

    // Destroy framebuffers and images (keep render passes and pipelines)
    if (m_hdr_framebuffer) { dev.destroyFramebuffer(m_hdr_framebuffer); m_hdr_framebuffer = VK_NULL_HANDLE; }
    for (int i = 0; i < 2; ++i) {
        if (m_bloom_framebuffers[i]) { dev.destroyFramebuffer(m_bloom_framebuffers[i]); m_bloom_framebuffers[i] = VK_NULL_HANDLE; }
    }

    // Destroy images
    if (m_hdr_image_view) { dev.destroyImageView(m_hdr_image_view); m_hdr_image_view = VK_NULL_HANDLE; }
    if (m_hdr_image) { dev.destroyImage(m_hdr_image); m_hdr_image = VK_NULL_HANDLE; }
    if (m_hdr_memory) { dev.freeMemory(m_hdr_memory); m_hdr_memory = VK_NULL_HANDLE; }
    if (m_hdr_depth_view) { dev.destroyImageView(m_hdr_depth_view); m_hdr_depth_view = VK_NULL_HANDLE; }
    if (m_hdr_depth_image) { dev.destroyImage(m_hdr_depth_image); m_hdr_depth_image = VK_NULL_HANDLE; }
    if (m_hdr_depth_memory) { dev.freeMemory(m_hdr_depth_memory); m_hdr_depth_memory = VK_NULL_HANDLE; }
    for (int i = 0; i < 2; ++i) {
        if (m_bloom_views[i]) { dev.destroyImageView(m_bloom_views[i]); m_bloom_views[i] = VK_NULL_HANDLE; }
        if (m_bloom_images[i]) { dev.destroyImage(m_bloom_images[i]); m_bloom_images[i] = VK_NULL_HANDLE; }
        if (m_bloom_memory[i]) { dev.freeMemory(m_bloom_memory[i]); m_bloom_memory[i] = VK_NULL_HANDLE; }
    }

    create_hdr_resources();
    create_bloom_resources();

    // Re-update descriptor sets
    if (m_descriptor_pool) { dev.destroyDescriptorPool(m_descriptor_pool); m_descriptor_pool = VK_NULL_HANDLE; }
    create_descriptor_resources();
}

void PostProcess::create_render_passes()
{
    const vk::Device dev = m_context.device();

    // HDR render pass: color (R16G16B16A16_SFLOAT) + depth
    {
        std::array<vk::AttachmentDescription, 2> att = {};
        att[0].format = hdr_format;
        att[0].samples = vk::SampleCountFlagBits::e1;
        att[0].loadOp = vk::AttachmentLoadOp::eClear;
        att[0].storeOp = vk::AttachmentStoreOp::eStore;
        att[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        att[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        att[0].initialLayout = vk::ImageLayout::eUndefined;
        att[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        att[1].format = depth_format;
        att[1].samples = vk::SampleCountFlagBits::e1;
        att[1].loadOp = vk::AttachmentLoadOp::eClear;
        att[1].storeOp = vk::AttachmentStoreOp::eStore; // store for VFX sampling
        att[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        att[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        att[1].initialLayout = vk::ImageLayout::eUndefined;
        att[1].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal; // for VFX depth sampling

        vk::AttachmentReference color_ref{0, vk::ImageLayout::eColorAttachmentOptimal};
        vk::AttachmentReference depth_ref{1, vk::ImageLayout::eDepthStencilAttachmentOptimal};

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;
        subpass.pDepthStencilAttachment = &depth_ref;

        vk::SubpassDependency dep;
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                           vk::PipelineStageFlagBits::eEarlyFragmentTests;
        dep.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                           vk::PipelineStageFlagBits::eEarlyFragmentTests;
        dep.srcAccessMask = vk::AccessFlags{};
        dep.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                            vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        vk::RenderPassCreateInfo ci;
        ci.attachmentCount = static_cast<uint32_t>(att.size());
        ci.pAttachments = att.data();
        ci.subpassCount = 1;
        ci.pSubpasses = &subpass;
        ci.dependencyCount = 1;
        ci.pDependencies = &dep;

        vk::Result r = dev.createRenderPass(&ci, nullptr, &m_hdr_render_pass);
        if (r != vk::Result::eSuccess) {
            KNG_CRITICAL("PostProcess HDR render pass: {}", vk::to_string(r));
            throw std::runtime_error("Fatal engine error");
        }
    }

    // Post-process render pass: single color attachment, no depth
    {
        vk::AttachmentDescription att;
        att.format = hdr_format;
        att.samples = vk::SampleCountFlagBits::e1;
        att.loadOp = vk::AttachmentLoadOp::eDontCare;
        att.storeOp = vk::AttachmentStoreOp::eStore;
        att.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        att.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        att.initialLayout = vk::ImageLayout::eUndefined;
        att.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        vk::AttachmentReference color_ref{0, vk::ImageLayout::eColorAttachmentOptimal};

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;

        vk::RenderPassCreateInfo ci;
        ci.attachmentCount = 1;
        ci.pAttachments = &att;
        ci.subpassCount = 1;
        ci.pSubpasses = &subpass;

        vk::Result r = dev.createRenderPass(&ci, nullptr, &m_post_render_pass);
        if (r != vk::Result::eSuccess) {
            KNG_CRITICAL("PostProcess post render pass: {}", vk::to_string(r));
            throw std::runtime_error("Fatal engine error");
        }
    }
}

void PostProcess::create_hdr_resources()
{
    const vk::Device dev = m_context.device();

    auto hdr = create_image_2d(m_context, m_width, m_height, hdr_format,
                               vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                               vk::ImageAspectFlagBits::eColor);
    m_hdr_image = hdr.image;
    m_hdr_memory = hdr.memory;
    m_hdr_image_view = hdr.view;

    auto depth = create_image_2d(m_context, m_width, m_height, depth_format,
                                 vk::ImageUsageFlagBits::eDepthStencilAttachment |
                                 vk::ImageUsageFlagBits::eSampled,
                                 vk::ImageAspectFlagBits::eDepth);
    m_hdr_depth_image = depth.image;
    m_hdr_depth_memory = depth.memory;
    m_hdr_depth_view = depth.view;

    if (!m_hdr_sampler) {
        m_hdr_sampler = create_linear_sampler(dev);
    }

    const std::array<vk::ImageView, 2> attachments = {m_hdr_image_view, m_hdr_depth_view};
    vk::FramebufferCreateInfo fbi;
    fbi.renderPass = m_hdr_render_pass;
    fbi.attachmentCount = static_cast<uint32_t>(attachments.size());
    fbi.pAttachments = attachments.data();
    fbi.width = m_width;
    fbi.height = m_height;
    fbi.layers = 1;

    vk::Result r = dev.createFramebuffer(&fbi, nullptr, &m_hdr_framebuffer);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("PostProcess HDR framebuffer: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }
}

void PostProcess::create_bloom_resources()
{
    const vk::Device dev = m_context.device();
    const uint32_t bw = m_width / 2;
    const uint32_t bh = m_height / 2;

    for (int i = 0; i < 2; ++i) {
        auto res = create_image_2d(m_context, bw, bh, hdr_format,
                                   vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                                   vk::ImageAspectFlagBits::eColor);
        m_bloom_images[i] = res.image;
        m_bloom_memory[i] = res.memory;
        m_bloom_views[i] = res.view;

        vk::FramebufferCreateInfo fbi;
        fbi.renderPass = m_post_render_pass;
        fbi.attachmentCount = 1;
        fbi.pAttachments = &m_bloom_views[i];
        fbi.width = bw;
        fbi.height = bh;
        fbi.layers = 1;

        vk::Result r = dev.createFramebuffer(&fbi, nullptr, &m_bloom_framebuffers[i]);
        if (r != vk::Result::eSuccess) {
            KNG_CRITICAL("PostProcess bloom framebuffer: {}", vk::to_string(r));
            throw std::runtime_error("Fatal engine error");
        }
    }

    if (!m_bloom_sampler) {
        m_bloom_sampler = create_linear_sampler(dev);
    }
}

void PostProcess::create_descriptor_resources()
{
    const vk::Device dev = m_context.device();

    // Layouts
    if (!m_single_tex_layout) {
        vk::DescriptorSetLayoutBinding b;
        b.binding = 0;
        b.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        b.descriptorCount = 1;
        b.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutCreateInfo ci;
        ci.bindingCount = 1;
        ci.pBindings = &b;
        dev.createDescriptorSetLayout(&ci, nullptr, &m_single_tex_layout);
    }

    if (!m_two_tex_layout) {
        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {};
        bindings[0].binding = 0;
        bindings[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = vk::ShaderStageFlagBits::eFragment;
        bindings[1].binding = 1;
        bindings[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutCreateInfo ci;
        ci.bindingCount = 2;
        ci.pBindings = bindings.data();
        dev.createDescriptorSetLayout(&ci, nullptr, &m_two_tex_layout);
    }

    // Pool
    vk::DescriptorPoolSize ps;
    ps.type = vk::DescriptorType::eCombinedImageSampler;
    ps.descriptorCount = 8;

    vk::DescriptorPoolCreateInfo pi;
    pi.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pi.maxSets = 4;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &ps;
    dev.createDescriptorPool(&pi, nullptr, &m_descriptor_pool);

    // Allocate sets
    auto alloc_single = [&]() {
        vk::DescriptorSetAllocateInfo ai;
        ai.descriptorPool = m_descriptor_pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &m_single_tex_layout;
        vk::DescriptorSet set;
        dev.allocateDescriptorSets(&ai, &set);
        return set;
    };

    m_threshold_set = alloc_single();
    m_blur_h_set = alloc_single();
    m_blur_v_set = alloc_single();

    {
        vk::DescriptorSetAllocateInfo ai;
        ai.descriptorPool = m_descriptor_pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &m_two_tex_layout;
        dev.allocateDescriptorSets(&ai, &m_tonemap_set);
    }

    // Write descriptors
    auto write_single = [&](vk::DescriptorSet set, vk::ImageView view, vk::Sampler sampler) {
        vk::DescriptorImageInfo ii;
        ii.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        ii.imageView = view;
        ii.sampler = sampler;

        vk::WriteDescriptorSet w;
        w.dstSet = set;
        w.dstBinding = 0;
        w.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        w.descriptorCount = 1;
        w.pImageInfo = &ii;
        dev.updateDescriptorSets(1, &w, 0, nullptr);
    };

    write_single(m_threshold_set, m_hdr_image_view, m_hdr_sampler);
    write_single(m_blur_h_set, m_bloom_views[0], m_bloom_sampler);
    write_single(m_blur_v_set, m_bloom_views[1], m_bloom_sampler);

    // Tonemap: binding 0 = hdr, binding 1 = bloom[0]
    {
        vk::DescriptorImageInfo ii[2];
        ii[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        ii[0].imageView = m_hdr_image_view;
        ii[0].sampler = m_hdr_sampler;
        ii[1].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        ii[1].imageView = m_bloom_views[0];
        ii[1].sampler = m_bloom_sampler;

        vk::WriteDescriptorSet w[2];
        w[0].dstSet = m_tonemap_set;
        w[0].dstBinding = 0;
        w[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        w[0].descriptorCount = 1;
        w[0].pImageInfo = &ii[0];
        w[1].dstSet = m_tonemap_set;
        w[1].dstBinding = 1;
        w[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        w[1].descriptorCount = 1;
        w[1].pImageInfo = &ii[1];
        dev.updateDescriptorSets(2, w, 0, nullptr);
    }
}

void PostProcess::create_pipelines()
{
    const vk::Device dev = m_context.device();

    // Load fullscreen vertex shader
    std::string fs_vert_src = try_load_shader("fullscreen.vert");
    if (fs_vert_src.empty()) {
        fs_vert_src = R"(#version 450
layout(location = 0) out vec2 fragUV;
void main() {
    fragUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragUV * 2.0 - 1.0, 0.0, 1.0);
})";
    }
    auto fs_vert_spv = compile_glsl(fs_vert_src, shaderc_glsl_vertex_shader, "fullscreen.vert");

    // Threshold pipeline
    {
        vk::PushConstantRange pc;
        pc.stageFlags = vk::ShaderStageFlagBits::eFragment;
        pc.offset = 0;
        pc.size = 8; // threshold + soft_knee

        vk::PipelineLayoutCreateInfo li;
        li.setLayoutCount = 1;
        li.pSetLayouts = &m_single_tex_layout;
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &pc;
        dev.createPipelineLayout(&li, nullptr, &m_threshold_layout);

        std::string src = try_load_shader("bloom_threshold.frag");
        if (src.empty()) {
            KNG_CRITICAL("bloom_threshold.frag not found");
            throw std::runtime_error("Fatal engine error");
        }
        auto frag_spv = compile_glsl(src, shaderc_glsl_fragment_shader, "bloom_threshold.frag");
        m_threshold_pipeline = create_fullscreen_pipeline(dev, m_post_render_pass,
                                                          m_threshold_layout, fs_vert_spv, frag_spv);
    }

    // Blur pipeline
    {
        vk::PushConstantRange pc;
        pc.stageFlags = vk::ShaderStageFlagBits::eFragment;
        pc.offset = 0;
        pc.size = 8; // direction vec2

        vk::PipelineLayoutCreateInfo li;
        li.setLayoutCount = 1;
        li.pSetLayouts = &m_single_tex_layout;
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &pc;
        dev.createPipelineLayout(&li, nullptr, &m_blur_layout);

        std::string src = try_load_shader("bloom_blur.frag");
        if (src.empty()) {
            KNG_CRITICAL("bloom_blur.frag not found");
            throw std::runtime_error("Fatal engine error");
        }
        auto frag_spv = compile_glsl(src, shaderc_glsl_fragment_shader, "bloom_blur.frag");
        m_blur_pipeline = create_fullscreen_pipeline(dev, m_post_render_pass,
                                                     m_blur_layout, fs_vert_spv, frag_spv);
    }

    // Tonemap pipeline (renders to swapchain, so we need a separate render pass)
    // We'll use the final_render_pass passed at record time, so we create the pipeline lazily
    // Actually, let's create it with the post_render_pass for now and use a compatible pass
    {
        vk::PushConstantRange pc;
        pc.stageFlags = vk::ShaderStageFlagBits::eFragment;
        pc.offset = 0;
        pc.size = 8; // exposure + bloom_strength

        vk::PipelineLayoutCreateInfo li;
        li.setLayoutCount = 1;
        li.pSetLayouts = &m_two_tex_layout;
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &pc;
        dev.createPipelineLayout(&li, nullptr, &m_tonemap_layout);

        std::string src = try_load_shader("tonemap.frag");
        if (src.empty()) {
            KNG_CRITICAL("tonemap.frag not found");
            throw std::runtime_error("Fatal engine error");
        }
        auto frag_spv = compile_glsl(src, shaderc_glsl_fragment_shader, "tonemap.frag");
        // Tonemap renders to swapchain via the external tonemap render pass
        m_tonemap_pipeline = create_fullscreen_pipeline(dev, m_tonemap_ext_render_pass,
                                                        m_tonemap_layout, fs_vert_spv, frag_spv);
    }

    KNG_INFO("PostProcess pipelines created");
}

void PostProcess::record_commands(vk::CommandBuffer cmd, vk::Framebuffer final_framebuffer,
                                  vk::RenderPass final_render_pass, vk::Extent2D extent,
                                  float exposure, float bloom_strength, float bloom_threshold_val)
{
    const uint32_t bw = m_width / 2;
    const uint32_t bh = m_height / 2;

    // Pass 1: Bloom threshold (HDR -> bloom[0])
    {
        vk::RenderPassBeginInfo rpi;
        rpi.renderPass = m_post_render_pass;
        rpi.framebuffer = m_bloom_framebuffers[0];
        rpi.renderArea = vk::Rect2D{{0, 0}, {bw, bh}};

        cmd.beginRenderPass(&rpi, vk::SubpassContents::eInline);

        vk::Viewport vp{0, 0, static_cast<float>(bw), static_cast<float>(bh), 0, 1};
        cmd.setViewport(0, 1, &vp);
        vk::Rect2D sc{{0, 0}, {bw, bh}};
        cmd.setScissor(0, 1, &sc);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_threshold_pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_threshold_layout, 0, 1,
                               &m_threshold_set, 0, nullptr);

        struct { float threshold; float soft_knee; } pc_data = {bloom_threshold_val, 0.5f};
        cmd.pushConstants(m_threshold_layout, vk::ShaderStageFlagBits::eFragment, 0, 8, &pc_data);
        cmd.draw(3, 1, 0, 0);
        cmd.endRenderPass();
    }

    // Pass 2: Blur horizontal (bloom[0] -> bloom[1])
    {
        vk::RenderPassBeginInfo rpi;
        rpi.renderPass = m_post_render_pass;
        rpi.framebuffer = m_bloom_framebuffers[1];
        rpi.renderArea = vk::Rect2D{{0, 0}, {bw, bh}};

        cmd.beginRenderPass(&rpi, vk::SubpassContents::eInline);

        vk::Viewport vp{0, 0, static_cast<float>(bw), static_cast<float>(bh), 0, 1};
        cmd.setViewport(0, 1, &vp);
        vk::Rect2D sc{{0, 0}, {bw, bh}};
        cmd.setScissor(0, 1, &sc);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_blur_pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_blur_layout, 0, 1,
                               &m_blur_h_set, 0, nullptr);

        struct { float dx; float dy; } blur_h = {1.0f / static_cast<float>(bw), 0.0f};
        cmd.pushConstants(m_blur_layout, vk::ShaderStageFlagBits::eFragment, 0, 8, &blur_h);
        cmd.draw(3, 1, 0, 0);
        cmd.endRenderPass();
    }

    // Pass 3: Blur vertical (bloom[1] -> bloom[0])
    {
        vk::RenderPassBeginInfo rpi;
        rpi.renderPass = m_post_render_pass;
        rpi.framebuffer = m_bloom_framebuffers[0];
        rpi.renderArea = vk::Rect2D{{0, 0}, {bw, bh}};

        cmd.beginRenderPass(&rpi, vk::SubpassContents::eInline);

        vk::Viewport vp{0, 0, static_cast<float>(bw), static_cast<float>(bh), 0, 1};
        cmd.setViewport(0, 1, &vp);
        vk::Rect2D sc{{0, 0}, {bw, bh}};
        cmd.setScissor(0, 1, &sc);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_blur_pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_blur_layout, 0, 1,
                               &m_blur_v_set, 0, nullptr);

        struct { float dx; float dy; } blur_v = {0.0f, 1.0f / static_cast<float>(bh)};
        cmd.pushConstants(m_blur_layout, vk::ShaderStageFlagBits::eFragment, 0, 8, &blur_v);
        cmd.draw(3, 1, 0, 0);
        cmd.endRenderPass();
    }

    // Pass 4: Tonemap + composite (HDR + bloom[0] -> final framebuffer)
    {
        vk::RenderPassBeginInfo rpi;
        rpi.renderPass = final_render_pass;
        rpi.framebuffer = final_framebuffer;
        rpi.renderArea = vk::Rect2D{{0, 0}, extent};

        cmd.beginRenderPass(&rpi, vk::SubpassContents::eInline);

        vk::Viewport vp{0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1};
        cmd.setViewport(0, 1, &vp);
        vk::Rect2D sc{{0, 0}, extent};
        cmd.setScissor(0, 1, &sc);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_tonemap_pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_tonemap_layout, 0, 1,
                               &m_tonemap_set, 0, nullptr);

        struct { float exposure; float bloom_strength; } pc_data = {exposure, bloom_strength};
        cmd.pushConstants(m_tonemap_layout, vk::ShaderStageFlagBits::eFragment, 0, 8, &pc_data);
        cmd.draw(3, 1, 0, 0);
        cmd.endRenderPass();
    }
}

void PostProcess::cleanup()
{
    const vk::Device dev = m_context.device();
    if (!dev) return;

    if (m_tonemap_pipeline) { dev.destroyPipeline(m_tonemap_pipeline); m_tonemap_pipeline = VK_NULL_HANDLE; }
    if (m_tonemap_layout) { dev.destroyPipelineLayout(m_tonemap_layout); m_tonemap_layout = VK_NULL_HANDLE; }
    if (m_blur_pipeline) { dev.destroyPipeline(m_blur_pipeline); m_blur_pipeline = VK_NULL_HANDLE; }
    if (m_blur_layout) { dev.destroyPipelineLayout(m_blur_layout); m_blur_layout = VK_NULL_HANDLE; }
    if (m_threshold_pipeline) { dev.destroyPipeline(m_threshold_pipeline); m_threshold_pipeline = VK_NULL_HANDLE; }
    if (m_threshold_layout) { dev.destroyPipelineLayout(m_threshold_layout); m_threshold_layout = VK_NULL_HANDLE; }

    if (m_descriptor_pool) { dev.destroyDescriptorPool(m_descriptor_pool); m_descriptor_pool = VK_NULL_HANDLE; }
    if (m_single_tex_layout) { dev.destroyDescriptorSetLayout(m_single_tex_layout); m_single_tex_layout = VK_NULL_HANDLE; }
    if (m_two_tex_layout) { dev.destroyDescriptorSetLayout(m_two_tex_layout); m_two_tex_layout = VK_NULL_HANDLE; }

    if (m_hdr_framebuffer) { dev.destroyFramebuffer(m_hdr_framebuffer); m_hdr_framebuffer = VK_NULL_HANDLE; }
    for (int i = 0; i < 2; ++i) {
        if (m_bloom_framebuffers[i]) { dev.destroyFramebuffer(m_bloom_framebuffers[i]); m_bloom_framebuffers[i] = VK_NULL_HANDLE; }
        if (m_bloom_views[i]) { dev.destroyImageView(m_bloom_views[i]); m_bloom_views[i] = VK_NULL_HANDLE; }
        if (m_bloom_images[i]) { dev.destroyImage(m_bloom_images[i]); m_bloom_images[i] = VK_NULL_HANDLE; }
        if (m_bloom_memory[i]) { dev.freeMemory(m_bloom_memory[i]); m_bloom_memory[i] = VK_NULL_HANDLE; }
    }
    if (m_bloom_sampler) { dev.destroySampler(m_bloom_sampler); m_bloom_sampler = VK_NULL_HANDLE; }

    if (m_hdr_image_view) { dev.destroyImageView(m_hdr_image_view); m_hdr_image_view = VK_NULL_HANDLE; }
    if (m_hdr_image) { dev.destroyImage(m_hdr_image); m_hdr_image = VK_NULL_HANDLE; }
    if (m_hdr_memory) { dev.freeMemory(m_hdr_memory); m_hdr_memory = VK_NULL_HANDLE; }
    if (m_hdr_sampler) { dev.destroySampler(m_hdr_sampler); m_hdr_sampler = VK_NULL_HANDLE; }
    if (m_hdr_depth_view) { dev.destroyImageView(m_hdr_depth_view); m_hdr_depth_view = VK_NULL_HANDLE; }
    if (m_hdr_depth_image) { dev.destroyImage(m_hdr_depth_image); m_hdr_depth_image = VK_NULL_HANDLE; }
    if (m_hdr_depth_memory) { dev.freeMemory(m_hdr_depth_memory); m_hdr_depth_memory = VK_NULL_HANDLE; }

    if (m_post_render_pass) { dev.destroyRenderPass(m_post_render_pass); m_post_render_pass = VK_NULL_HANDLE; }
    if (m_hdr_render_pass) { dev.destroyRenderPass(m_hdr_render_pass); m_hdr_render_pass = VK_NULL_HANDLE; }
}

} // namespace kenga
