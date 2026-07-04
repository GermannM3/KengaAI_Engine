/**
 * @file VfxPasses.cpp
 * @brief Advanced VFX post-process passes implementation
 *
 * PROJECT_RULES.md. Phase 9: Advanced VFX + shaders.
 */

#include "rendering/VfxPasses.h"
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
    KNG_CRITICAL("VfxPasses: failed to find memory type");
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

vk::ShaderModule create_shader_module(vk::Device dev, const std::vector<uint32_t>& spirv)
{
    vk::ShaderModuleCreateInfo ci;
    ci.codeSize = spirv.size() * sizeof(uint32_t);
    ci.pCode = spirv.data();
    vk::ShaderModule mod;
    vk::Result r = dev.createShaderModule(&ci, nullptr, &mod);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("VfxPasses createShaderModule: {}", vk::to_string(r));
        return VK_NULL_HANDLE;
    }
    return mod;
}

vk::Pipeline create_fullscreen_pipeline(vk::Device dev, vk::RenderPass rp,
                                        vk::PipelineLayout layout,
                                        const std::vector<uint32_t>& vert_spv,
                                        const std::vector<uint32_t>& frag_spv)
{
    if (vert_spv.empty() || frag_spv.empty()) return VK_NULL_HANDLE;

    vk::ShaderModule vert_mod = create_shader_module(dev, vert_spv);
    vk::ShaderModule frag_mod = create_shader_module(dev, frag_spv);
    if (!vert_mod || !frag_mod) {
        if (vert_mod) dev.destroyShaderModule(vert_mod);
        if (frag_mod) dev.destroyShaderModule(frag_mod);
        return VK_NULL_HANDLE;
    }

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = vert_mod;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = frag_mod;
    stages[1].pName = "main";

    vk::PipelineVertexInputStateCreateInfo vertex_input;
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

    vk::PipelineMultisampleStateCreateInfo ms;
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState blend_att;
    blend_att.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                               vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    blend_att.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo blend;
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_att;

    vk::PipelineDepthStencilStateCreateInfo depth_stencil;
    depth_stencil.depthTestEnable = VK_FALSE;
    depth_stencil.depthWriteEnable = VK_FALSE;

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
    pi.pDepthStencilState = &depth_stencil;
    pi.layout = layout;
    pi.renderPass = rp;
    pi.subpass = 0;

    vk::Pipeline pipeline;
    vk::Result r = dev.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pi, nullptr, &pipeline);

    dev.destroyShaderModule(frag_mod);
    dev.destroyShaderModule(vert_mod);

    if (r != vk::Result::eSuccess) {
        KNG_WARN("VfxPasses createPipeline failed: {}", vk::to_string(r));
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

} // namespace

VfxPasses::VfxPasses(VulkanContext& context, uint32_t width, uint32_t height,
                     vk::RenderPass post_render_pass,
                     vk::ImageView hdr_view, vk::Sampler hdr_sampler,
                     vk::ImageView depth_view)
    : m_context(context)
    , m_width(width)
    , m_height(height)
    , m_post_render_pass(post_render_pass)
    , m_hdr_sampler(hdr_sampler)
    , m_hdr_view(hdr_view)
    , m_depth_view(depth_view)
{
    create_resources();
    create_pipelines();
    create_descriptors();
    KNG_INFO("VfxPasses initialized ({}x{})", width, height);
}

VfxPasses::~VfxPasses()
{
    cleanup();
}

void VfxPasses::recreate(uint32_t width, uint32_t height,
                         vk::ImageView hdr_view, vk::ImageView depth_view)
{
    m_width = width;
    m_height = height;
    m_hdr_view = hdr_view;
    m_depth_view = depth_view;

    const vk::Device dev = m_context.device();
    dev.waitIdle();

    // Destroy framebuffers and images
    for (int i = 0; i < 2; ++i) {
        if (m_vfx_framebuffers[i]) { dev.destroyFramebuffer(m_vfx_framebuffers[i]); m_vfx_framebuffers[i] = VK_NULL_HANDLE; }
        if (m_vfx_views[i]) { dev.destroyImageView(m_vfx_views[i]); m_vfx_views[i] = VK_NULL_HANDLE; }
        if (m_vfx_images[i]) { dev.destroyImage(m_vfx_images[i]); m_vfx_images[i] = VK_NULL_HANDLE; }
        if (m_vfx_memory[i]) { dev.freeMemory(m_vfx_memory[i]); m_vfx_memory[i] = VK_NULL_HANDLE; }
    }

    if (m_desc_pool) { dev.destroyDescriptorPool(m_desc_pool); m_desc_pool = VK_NULL_HANDLE; }

    create_resources();
    create_descriptors();
}

void VfxPasses::create_resources()
{
    const vk::Device dev = m_context.device();
    constexpr vk::Format fmt = vk::Format::eR16G16B16A16Sfloat;

    for (int i = 0; i < 2; ++i) {
        vk::ImageCreateInfo info;
        info.imageType = vk::ImageType::e2D;
        info.format = fmt;
        info.extent = vk::Extent3D{m_width, m_height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = vk::SampleCountFlagBits::e1;
        info.tiling = vk::ImageTiling::eOptimal;
        info.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
        info.sharingMode = vk::SharingMode::eExclusive;
        info.initialLayout = vk::ImageLayout::eUndefined;

        dev.createImage(&info, nullptr, &m_vfx_images[i]);

        vk::MemoryRequirements mem_req;
        dev.getImageMemoryRequirements(m_vfx_images[i], &mem_req);

        vk::MemoryAllocateInfo alloc;
        alloc.allocationSize = mem_req.size;
        alloc.memoryTypeIndex = find_memory_type(m_context.physical_device(), mem_req.memoryTypeBits,
                                                 vk::MemoryPropertyFlagBits::eDeviceLocal);
        dev.allocateMemory(&alloc, nullptr, &m_vfx_memory[i]);
        dev.bindImageMemory(m_vfx_images[i], m_vfx_memory[i], 0);

        vk::ImageViewCreateInfo vi;
        vi.image = m_vfx_images[i];
        vi.viewType = vk::ImageViewType::e2D;
        vi.format = fmt;
        vi.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        dev.createImageView(&vi, nullptr, &m_vfx_views[i]);

        vk::FramebufferCreateInfo fbi;
        fbi.renderPass = m_post_render_pass;
        fbi.attachmentCount = 1;
        fbi.pAttachments = &m_vfx_views[i];
        fbi.width = m_width;
        fbi.height = m_height;
        fbi.layers = 1;
        dev.createFramebuffer(&fbi, nullptr, &m_vfx_framebuffers[i]);
    }

    if (!m_vfx_sampler) {
        vk::SamplerCreateInfo si;
        si.magFilter = vk::Filter::eLinear;
        si.minFilter = vk::Filter::eLinear;
        si.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        si.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        si.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        dev.createSampler(&si, nullptr, &m_vfx_sampler);
    }

    if (!m_depth_sampler) {
        vk::SamplerCreateInfo si;
        si.magFilter = vk::Filter::eNearest;
        si.minFilter = vk::Filter::eNearest;
        si.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        si.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        si.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        dev.createSampler(&si, nullptr, &m_depth_sampler);
    }
}

void VfxPasses::create_pipelines()
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

    // Descriptor set layouts
    {
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
    {
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

    // God rays pipeline (single texture input)
    {
        vk::PushConstantRange pc;
        pc.stageFlags = vk::ShaderStageFlagBits::eFragment;
        pc.offset = 0;
        pc.size = 32; // 2 floats + 4 floats + 1 int + pad = 32 bytes

        vk::PipelineLayoutCreateInfo li;
        li.setLayoutCount = 1;
        li.pSetLayouts = &m_single_tex_layout;
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &pc;
        dev.createPipelineLayout(&li, nullptr, &m_god_rays_layout);

        std::string src = try_load_shader("god_rays.frag");
        if (!src.empty()) {
            auto frag_spv = compile_glsl(src, shaderc_glsl_fragment_shader, "god_rays.frag");
            if (!frag_spv.empty()) {
                m_god_rays_pipeline = create_fullscreen_pipeline(dev, m_post_render_pass,
                                                                 m_god_rays_layout, fs_vert_spv, frag_spv);
            }
        }
        if (!m_god_rays_pipeline) {
            KNG_WARN("God rays shader not found or failed to compile — effect disabled");
        }
    }

    // Fog pipeline (two textures: HDR + depth)
    {
        vk::PushConstantRange pc;
        pc.stageFlags = vk::ShaderStageFlagBits::eFragment;
        pc.offset = 0;
        pc.size = 48; // 3 floats color + density + start + end + height + falloff + near + far + cam_y + pad = 48

        vk::PipelineLayoutCreateInfo li;
        li.setLayoutCount = 1;
        li.pSetLayouts = &m_two_tex_layout;
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &pc;
        dev.createPipelineLayout(&li, nullptr, &m_fog_layout);

        std::string src = try_load_shader("fog.frag");
        if (!src.empty()) {
            auto frag_spv = compile_glsl(src, shaderc_glsl_fragment_shader, "fog.frag");
            if (!frag_spv.empty()) {
                m_fog_pipeline = create_fullscreen_pipeline(dev, m_post_render_pass,
                                                            m_fog_layout, fs_vert_spv, frag_spv);
            }
        }
        if (!m_fog_pipeline) {
            KNG_WARN("Fog shader not found or failed to compile — effect disabled");
        }
    }

    // SSR pipeline (two textures: HDR + depth, large push constants for matrices)
    {
        vk::PushConstantRange pc;
        pc.stageFlags = vk::ShaderStageFlagBits::eFragment;
        pc.offset = 0;
        pc.size = 160; // 2x mat4 (128) + 5 floats + 1 int + pad = 160

        vk::PipelineLayoutCreateInfo li;
        li.setLayoutCount = 1;
        li.pSetLayouts = &m_two_tex_layout;
        li.pushConstantRangeCount = 1;
        li.pPushConstantRanges = &pc;
        dev.createPipelineLayout(&li, nullptr, &m_ssr_layout);

        std::string src = try_load_shader("ssr.frag");
        if (!src.empty()) {
            auto frag_spv = compile_glsl(src, shaderc_glsl_fragment_shader, "ssr.frag");
            if (!frag_spv.empty()) {
                m_ssr_pipeline = create_fullscreen_pipeline(dev, m_post_render_pass,
                                                            m_ssr_layout, fs_vert_spv, frag_spv);
            }
        }
        if (!m_ssr_pipeline) {
            KNG_WARN("SSR shader not found or failed to compile — effect disabled");
        }
    }

    KNG_INFO("VfxPasses pipelines created (god_rays={}, fog={}, ssr={})",
             static_cast<bool>(m_god_rays_pipeline),
             static_cast<bool>(m_fog_pipeline),
             static_cast<bool>(m_ssr_pipeline));
}

void VfxPasses::create_descriptors()
{
    const vk::Device dev = m_context.device();

    // Pool: 3 single-tex sets + 3 two-tex sets = 6 sets, 9 samplers total
    vk::DescriptorPoolSize ps;
    ps.type = vk::DescriptorType::eCombinedImageSampler;
    ps.descriptorCount = 12;

    vk::DescriptorPoolCreateInfo pi;
    pi.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pi.maxSets = 6;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &ps;
    dev.createDescriptorPool(&pi, nullptr, &m_desc_pool);

    // Allocate single-tex sets
    for (int i = 0; i < 3; ++i) {
        vk::DescriptorSetAllocateInfo ai;
        ai.descriptorPool = m_desc_pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &m_single_tex_layout;
        dev.allocateDescriptorSets(&ai, &m_single_sets[i]);
    }

    // Allocate two-tex sets
    for (int i = 0; i < 3; ++i) {
        vk::DescriptorSetAllocateInfo ai;
        ai.descriptorPool = m_desc_pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &m_two_tex_layout;
        dev.allocateDescriptorSets(&ai, &m_two_tex_sets[i]);
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

    auto write_two = [&](vk::DescriptorSet set, vk::ImageView color_view, vk::Sampler color_sampler,
                         vk::ImageView depth_v, vk::Sampler depth_s) {
        vk::DescriptorImageInfo ii[2];
        ii[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        ii[0].imageView = color_view;
        ii[0].sampler = color_sampler;
        ii[1].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        ii[1].imageView = depth_v;
        ii[1].sampler = depth_s;

        vk::WriteDescriptorSet w[2];
        w[0].dstSet = set;
        w[0].dstBinding = 0;
        w[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        w[0].descriptorCount = 1;
        w[0].pImageInfo = &ii[0];
        w[1].dstSet = set;
        w[1].dstBinding = 1;
        w[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        w[1].descriptorCount = 1;
        w[1].pImageInfo = &ii[1];
        dev.updateDescriptorSets(2, w, 0, nullptr);
    };

    // Single tex: [0]=HDR, [1]=vfx[0], [2]=vfx[1]
    write_single(m_single_sets[0], m_hdr_view, m_hdr_sampler);
    write_single(m_single_sets[1], m_vfx_views[0], m_vfx_sampler);
    write_single(m_single_sets[2], m_vfx_views[1], m_vfx_sampler);

    // Two tex: [0]=HDR+depth, [1]=vfx[0]+depth, [2]=vfx[1]+depth
    write_two(m_two_tex_sets[0], m_hdr_view, m_hdr_sampler, m_depth_view, m_depth_sampler);
    write_two(m_two_tex_sets[1], m_vfx_views[0], m_vfx_sampler, m_depth_view, m_depth_sampler);
    write_two(m_two_tex_sets[2], m_vfx_views[1], m_vfx_sampler, m_depth_view, m_depth_sampler);
}

void VfxPasses::record_commands(vk::CommandBuffer cmd, const VfxSettings& settings,
                                float near_plane, float far_plane, float camera_y,
                                const glm::mat4& inv_view_proj, const glm::mat4& view_proj)
{
    // Track which source to read from: 0 = HDR original, 1 = vfx[0], 2 = vfx[1]
    int current_source = 0; // 0 means read from HDR (m_single_sets[0] / m_two_tex_sets[0])
    int next_target = 0;    // which vfx buffer to write to

    auto set_viewport_scissor = [&](vk::CommandBuffer c) {
        vk::Viewport vp{0, 0, static_cast<float>(m_width), static_cast<float>(m_height), 0, 1};
        c.setViewport(0, 1, &vp);
        vk::Rect2D sc{{0, 0}, {m_width, m_height}};
        c.setScissor(0, 1, &sc);
    };

    // Pass 1: God rays
    if (settings.god_rays_enabled && m_god_rays_pipeline) {
        vk::RenderPassBeginInfo rpi;
        rpi.renderPass = m_post_render_pass;
        rpi.framebuffer = m_vfx_framebuffers[next_target];
        rpi.renderArea = vk::Rect2D{{0, 0}, {m_width, m_height}};

        cmd.beginRenderPass(&rpi, vk::SubpassContents::eInline);
        set_viewport_scissor(cmd);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_god_rays_pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_god_rays_layout, 0, 1,
                               &m_single_sets[current_source], 0, nullptr);

        struct {
            float light_x, light_y;
            float density;
            float weight;
            float decay;
            float exposure_rays;
            int num_samples;
            float _pad;
        } pc_data = {
            settings.light_screen_pos.x, settings.light_screen_pos.y,
            settings.god_rays_density, settings.god_rays_weight,
            settings.god_rays_decay, settings.god_rays_exposure,
            settings.god_rays_samples, 0.0f
        };
        cmd.pushConstants(m_god_rays_layout, vk::ShaderStageFlagBits::eFragment, 0,
                          sizeof(pc_data), &pc_data);
        cmd.draw(3, 1, 0, 0);
        cmd.endRenderPass();

        // Update source tracking
        current_source = next_target + 1; // 1 or 2 (maps to m_single_sets[1] or [2])
        next_target = 1 - next_target;    // ping-pong
    }

    // Pass 2: Volumetric fog
    if (settings.fog_enabled && m_fog_pipeline) {
        vk::RenderPassBeginInfo rpi;
        rpi.renderPass = m_post_render_pass;
        rpi.framebuffer = m_vfx_framebuffers[next_target];
        rpi.renderArea = vk::Rect2D{{0, 0}, {m_width, m_height}};

        cmd.beginRenderPass(&rpi, vk::SubpassContents::eInline);
        set_viewport_scissor(cmd);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_fog_pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_fog_layout, 0, 1,
                               &m_two_tex_sets[current_source], 0, nullptr);

        struct {
            float fog_r, fog_g, fog_b;
            float fog_density;
            float fog_start;
            float fog_end;
            float fog_height;
            float fog_height_falloff;
            float near_p;
            float far_p;
            float cam_y;
            float _pad;
        } pc_data = {
            settings.fog_color.r, settings.fog_color.g, settings.fog_color.b,
            settings.fog_density, settings.fog_start, settings.fog_end,
            settings.fog_height, settings.fog_height_falloff,
            near_plane, far_plane, camera_y, 0.0f
        };
        cmd.pushConstants(m_fog_layout, vk::ShaderStageFlagBits::eFragment, 0,
                          sizeof(pc_data), &pc_data);
        cmd.draw(3, 1, 0, 0);
        cmd.endRenderPass();

        current_source = next_target + 1;
        next_target = 1 - next_target;
    }

    // Pass 3: SSR (expensive, off by default)
    if (settings.ssr_enabled && m_ssr_pipeline) {
        vk::RenderPassBeginInfo rpi;
        rpi.renderPass = m_post_render_pass;
        rpi.framebuffer = m_vfx_framebuffers[next_target];
        rpi.renderArea = vk::Rect2D{{0, 0}, {m_width, m_height}};

        cmd.beginRenderPass(&rpi, vk::SubpassContents::eInline);
        set_viewport_scissor(cmd);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_ssr_pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_ssr_layout, 0, 1,
                               &m_two_tex_sets[current_source], 0, nullptr);

        struct {
            glm::mat4 inv_vp;
            glm::mat4 vp;
            float step_size;
            float max_distance;
            float thickness;
            float intensity;
            float near_p;
            float far_p;
            int max_steps;
            float _pad;
        } pc_data = {
            inv_view_proj, view_proj,
            settings.ssr_step_size, settings.ssr_max_distance,
            settings.ssr_thickness, settings.ssr_intensity,
            near_plane, far_plane, settings.ssr_max_steps, 0.0f
        };
        cmd.pushConstants(m_ssr_layout, vk::ShaderStageFlagBits::eFragment, 0,
                          sizeof(pc_data), &pc_data);
        cmd.draw(3, 1, 0, 0);
        cmd.endRenderPass();

        current_source = next_target + 1;
        next_target = 1 - next_target;
    }

    // Record which buffer has the final output
    if (current_source == 0) {
        // No VFX passes ran — output is the original HDR
        m_output_index = -1; // signal to use original HDR
    } else {
        m_output_index = current_source - 1; // 0 or 1
    }
}

void VfxPasses::cleanup()
{
    const vk::Device dev = m_context.device();
    if (!dev) return;

    if (m_ssr_pipeline) { dev.destroyPipeline(m_ssr_pipeline); m_ssr_pipeline = VK_NULL_HANDLE; }
    if (m_ssr_layout) { dev.destroyPipelineLayout(m_ssr_layout); m_ssr_layout = VK_NULL_HANDLE; }
    if (m_fog_pipeline) { dev.destroyPipeline(m_fog_pipeline); m_fog_pipeline = VK_NULL_HANDLE; }
    if (m_fog_layout) { dev.destroyPipelineLayout(m_fog_layout); m_fog_layout = VK_NULL_HANDLE; }
    if (m_god_rays_pipeline) { dev.destroyPipeline(m_god_rays_pipeline); m_god_rays_pipeline = VK_NULL_HANDLE; }
    if (m_god_rays_layout) { dev.destroyPipelineLayout(m_god_rays_layout); m_god_rays_layout = VK_NULL_HANDLE; }

    if (m_desc_pool) { dev.destroyDescriptorPool(m_desc_pool); m_desc_pool = VK_NULL_HANDLE; }
    if (m_single_tex_layout) { dev.destroyDescriptorSetLayout(m_single_tex_layout); m_single_tex_layout = VK_NULL_HANDLE; }
    if (m_two_tex_layout) { dev.destroyDescriptorSetLayout(m_two_tex_layout); m_two_tex_layout = VK_NULL_HANDLE; }

    for (int i = 0; i < 2; ++i) {
        if (m_vfx_framebuffers[i]) { dev.destroyFramebuffer(m_vfx_framebuffers[i]); m_vfx_framebuffers[i] = VK_NULL_HANDLE; }
        if (m_vfx_views[i]) { dev.destroyImageView(m_vfx_views[i]); m_vfx_views[i] = VK_NULL_HANDLE; }
        if (m_vfx_images[i]) { dev.destroyImage(m_vfx_images[i]); m_vfx_images[i] = VK_NULL_HANDLE; }
        if (m_vfx_memory[i]) { dev.freeMemory(m_vfx_memory[i]); m_vfx_memory[i] = VK_NULL_HANDLE; }
    }

    if (m_vfx_sampler) { dev.destroySampler(m_vfx_sampler); m_vfx_sampler = VK_NULL_HANDLE; }
    if (m_depth_sampler) { dev.destroySampler(m_depth_sampler); m_depth_sampler = VK_NULL_HANDLE; }
}

} // namespace kenga
