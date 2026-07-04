/**
 * @file GpuParticleSystem.cpp
 * @brief GPU-rendered particle system implementation
 */

#include "particles/GpuParticleSystem.h"
#include <stdexcept>
#include "rendering/VulkanContext.h"
#include "ecs/Components.h"
#include "ecs/Registry.h"
#include "core/LoggerMacros.h"

#include <shaderc/shaderc.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>

namespace kenga {

// ---- helpers ----

static std::mt19937& rng()
{
    static std::mt19937 gen{std::random_device{}()};
    return gen;
}

static float rand_range(float lo, float hi)
{
    return std::uniform_real_distribution<float>(lo, hi)(rng());
}

static uint32_t find_mem_type(vk::PhysicalDevice phys, uint32_t filter, vk::MemoryPropertyFlags flags)
{
    const auto props = phys.getMemoryProperties();
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((filter & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    KNG_CRITICAL("GpuParticleSystem: no suitable memory type");
    throw std::runtime_error("Fatal engine error");
}

static std::string read_shader_file(const char* path)
{
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::vector<uint32_t> compile_glsl(const std::string& src, shaderc_shader_kind kind, const char* name)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions opts;
    opts.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
    opts.SetOptimizationLevel(shaderc_optimization_level_performance);
    auto result = compiler.CompileGlslToSpv(src, kind, name, opts);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        KNG_CRITICAL("Particle shader compile failed ({}): {}", name, result.GetErrorMessage());
        return {};
    }
    return {result.cbegin(), result.cend()};
}

// ---- GpuParticleSystem ----

GpuParticleSystem::GpuParticleSystem()
{
    m_particles.resize(MAX_PARTICLES);
    for (auto& p : m_particles) {
        p.size_flags.z = 0.0f; // dead
    }
}

GpuParticleSystem::~GpuParticleSystem()
{
    cleanup();
}

void GpuParticleSystem::cleanup()
{
    if (!m_gpu_initialized) return;

    m_device.waitIdle();

    if (m_pipeline) m_device.destroyPipeline(m_pipeline);
    if (m_pipeline_layout) m_device.destroyPipelineLayout(m_pipeline_layout);
    if (m_desc_pool) m_device.destroyDescriptorPool(m_desc_pool);
    if (m_desc_set_layout) m_device.destroyDescriptorSetLayout(m_desc_set_layout);

    if (m_staging_mapped) m_device.unmapMemory(m_staging_memory);
    if (m_staging_buffer) m_device.destroyBuffer(m_staging_buffer);
    if (m_staging_memory) m_device.freeMemory(m_staging_memory);
    if (m_ssbo) m_device.destroyBuffer(m_ssbo);
    if (m_ssbo_memory) m_device.freeMemory(m_ssbo_memory);
    if (m_quad_vb) m_device.destroyBuffer(m_quad_vb);
    if (m_quad_vb_memory) m_device.freeMemory(m_quad_vb_memory);

    m_gpu_initialized = false;
}

void GpuParticleSystem::init_gpu(VulkanContext& context, vk::CommandPool cmd_pool,
                                  vk::RenderPass render_pass, vk::Extent2D extent)
{
    m_device = context.device();
    m_physical_device = context.physical_device();
    m_cmd_pool = cmd_pool;

    create_buffers(context);
    create_pipeline(context, render_pass, extent);

    m_gpu_initialized = true;
    KNG_INFO("GpuParticleSystem initialized ({} max particles)", MAX_PARTICLES);
}

void GpuParticleSystem::create_buffers(VulkanContext& context)
{
    const vk::DeviceSize ssbo_size = sizeof(GpuParticle) * MAX_PARTICLES;

    // SSBO (device-local, used in shader)
    {
        vk::BufferCreateInfo bi;
        bi.size = ssbo_size;
        bi.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        bi.sharingMode = vk::SharingMode::eExclusive;
        m_ssbo = m_device.createBuffer(bi);

        auto reqs = m_device.getBufferMemoryRequirements(m_ssbo);
        vk::MemoryAllocateInfo ai;
        ai.allocationSize = reqs.size;
        ai.memoryTypeIndex = find_mem_type(m_physical_device, reqs.memoryTypeBits,
                                           vk::MemoryPropertyFlagBits::eDeviceLocal);
        m_ssbo_memory = m_device.allocateMemory(ai);
        m_device.bindBufferMemory(m_ssbo, m_ssbo_memory, 0);
    }

    // Staging buffer (host-visible, for CPU->GPU upload)
    {
        vk::BufferCreateInfo bi;
        bi.size = ssbo_size;
        bi.usage = vk::BufferUsageFlagBits::eTransferSrc;
        bi.sharingMode = vk::SharingMode::eExclusive;
        m_staging_buffer = m_device.createBuffer(bi);

        auto reqs = m_device.getBufferMemoryRequirements(m_staging_buffer);
        vk::MemoryAllocateInfo ai;
        ai.allocationSize = reqs.size;
        ai.memoryTypeIndex = find_mem_type(m_physical_device, reqs.memoryTypeBits,
                                           vk::MemoryPropertyFlagBits::eHostVisible |
                                           vk::MemoryPropertyFlagBits::eHostCoherent);
        m_staging_memory = m_device.allocateMemory(ai);
        m_device.bindBufferMemory(m_staging_buffer, m_staging_memory, 0);
        m_staging_mapped = m_device.mapMemory(m_staging_memory, 0, ssbo_size);
    }

    // Quad vertex buffer: 6 vertices (2 triangles) with vec2 offsets
    {
        const float quad_verts[] = {
            -0.5f, -0.5f,
             0.5f, -0.5f,
             0.5f,  0.5f,
            -0.5f, -0.5f,
             0.5f,  0.5f,
            -0.5f,  0.5f,
        };
        const vk::DeviceSize vb_size = sizeof(quad_verts);

        vk::BufferCreateInfo bi;
        bi.size = vb_size;
        bi.usage = vk::BufferUsageFlagBits::eVertexBuffer;
        bi.sharingMode = vk::SharingMode::eExclusive;
        m_quad_vb = m_device.createBuffer(bi);

        auto reqs = m_device.getBufferMemoryRequirements(m_quad_vb);
        vk::MemoryAllocateInfo ai;
        ai.allocationSize = reqs.size;
        ai.memoryTypeIndex = find_mem_type(m_physical_device, reqs.memoryTypeBits,
                                           vk::MemoryPropertyFlagBits::eHostVisible |
                                           vk::MemoryPropertyFlagBits::eHostCoherent);
        m_quad_vb_memory = m_device.allocateMemory(ai);
        m_device.bindBufferMemory(m_quad_vb, m_quad_vb_memory, 0);

        void* data = m_device.mapMemory(m_quad_vb_memory, 0, vb_size);
        std::memcpy(data, quad_verts, vb_size);
        m_device.unmapMemory(m_quad_vb_memory);
    }
}

void GpuParticleSystem::create_pipeline(VulkanContext& context, vk::RenderPass render_pass,
                                         vk::Extent2D extent)
{
    // Load and compile shaders
    std::string vert_src = read_shader_file("shaders/particles/particle.vert");
    std::string frag_src = read_shader_file("shaders/particles/particle.frag");

    if (vert_src.empty() || frag_src.empty()) {
        KNG_WARN("Particle shaders not found, GPU particles disabled");
        return;
    }

    auto vert_spirv = compile_glsl(vert_src, shaderc_glsl_vertex_shader, "particle.vert");
    auto frag_spirv = compile_glsl(frag_src, shaderc_glsl_fragment_shader, "particle.frag");
    if (vert_spirv.empty() || frag_spirv.empty()) {
        KNG_WARN("Particle shader compilation failed, GPU particles disabled");
        return;
    }

    // Shader modules
    vk::ShaderModuleCreateInfo vert_ci;
    vert_ci.codeSize = vert_spirv.size() * sizeof(uint32_t);
    vert_ci.pCode = vert_spirv.data();
    auto vert_module = m_device.createShaderModule(vert_ci);

    vk::ShaderModuleCreateInfo frag_ci;
    frag_ci.codeSize = frag_spirv.size() * sizeof(uint32_t);
    frag_ci.pCode = frag_spirv.data();
    auto frag_module = m_device.createShaderModule(frag_ci);

    vk::PipelineShaderStageCreateInfo stages[2];
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = vert_module;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = frag_module;
    stages[1].pName = "main";

    // Vertex input: single vec2 attribute (quad offset)
    vk::VertexInputBindingDescription binding;
    binding.binding = 0;
    binding.stride = sizeof(float) * 2;
    binding.inputRate = vk::VertexInputRate::eVertex;

    vk::VertexInputAttributeDescription attr;
    attr.binding = 0;
    attr.location = 0;
    attr.format = vk::Format::eR32G32Sfloat;
    attr.offset = 0;

    vk::PipelineVertexInputStateCreateInfo vertex_input;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 1;
    vertex_input.pVertexAttributeDescriptions = &attr;

    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::Viewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor{{0, 0}, extent};

    vk::PipelineViewportStateCreateInfo viewport_state;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone; // billboards face camera
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineDepthStencilStateCreateInfo depth_stencil;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_FALSE; // particles don't write depth
    depth_stencil.depthCompareOp = vk::CompareOp::eLess;

    // Alpha blending (additive for sparks/fire, standard alpha for smoke)
    vk::PipelineColorBlendAttachmentState blend_att;
    blend_att.blendEnable = VK_TRUE;
    blend_att.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    blend_att.dstColorBlendFactor = vk::BlendFactor::eOne; // additive
    blend_att.colorBlendOp = vk::BlendOp::eAdd;
    blend_att.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    blend_att.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    blend_att.alphaBlendOp = vk::BlendOp::eAdd;
    blend_att.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                               vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo color_blend;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &blend_att;

    // Dynamic state for viewport/scissor
    vk::DynamicState dyn_states[] = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dyn_state;
    dyn_state.dynamicStateCount = 2;
    dyn_state.pDynamicStates = dyn_states;

    // Descriptor set layout: one SSBO binding
    vk::DescriptorSetLayoutBinding ssbo_binding;
    ssbo_binding.binding = 0;
    ssbo_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
    ssbo_binding.descriptorCount = 1;
    ssbo_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

    vk::DescriptorSetLayoutCreateInfo dsl_ci;
    dsl_ci.bindingCount = 1;
    dsl_ci.pBindings = &ssbo_binding;
    m_desc_set_layout = m_device.createDescriptorSetLayout(dsl_ci);

    // Push constant: mat4 view_proj + vec4 camera_right + vec4 camera_up = 96 bytes
    vk::PushConstantRange push_range;
    push_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
    push_range.offset = 0;
    push_range.size = 96;

    vk::PipelineLayoutCreateInfo layout_ci;
    layout_ci.setLayoutCount = 1;
    layout_ci.pSetLayouts = &m_desc_set_layout;
    layout_ci.pushConstantRangeCount = 1;
    layout_ci.pPushConstantRanges = &push_range;
    m_pipeline_layout = m_device.createPipelineLayout(layout_ci);

    // Descriptor pool and set
    vk::DescriptorPoolSize pool_size;
    pool_size.type = vk::DescriptorType::eStorageBuffer;
    pool_size.descriptorCount = 1;

    vk::DescriptorPoolCreateInfo pool_ci;
    pool_ci.maxSets = 1;
    pool_ci.poolSizeCount = 1;
    pool_ci.pPoolSizes = &pool_size;
    m_desc_pool = m_device.createDescriptorPool(pool_ci);

    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.descriptorPool = m_desc_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &m_desc_set_layout;
    m_desc_set = m_device.allocateDescriptorSets(alloc_info)[0];

    // Write SSBO descriptor
    vk::DescriptorBufferInfo buf_info;
    buf_info.buffer = m_ssbo;
    buf_info.offset = 0;
    buf_info.range = sizeof(GpuParticle) * MAX_PARTICLES;

    vk::WriteDescriptorSet write;
    write.dstSet = m_desc_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eStorageBuffer;
    write.pBufferInfo = &buf_info;
    m_device.updateDescriptorSets(1, &write, 0, nullptr);

    // Create graphics pipeline
    vk::GraphicsPipelineCreateInfo gpi;
    gpi.stageCount = 2;
    gpi.pStages = stages;
    gpi.pVertexInputState = &vertex_input;
    gpi.pInputAssemblyState = &input_assembly;
    gpi.pViewportState = &viewport_state;
    gpi.pRasterizationState = &rasterizer;
    gpi.pMultisampleState = &multisampling;
    gpi.pDepthStencilState = &depth_stencil;
    gpi.pColorBlendState = &color_blend;
    gpi.pDynamicState = &dyn_state;
    gpi.layout = m_pipeline_layout;
    gpi.renderPass = render_pass;
    gpi.subpass = 0;

    auto [result, pipe] = m_device.createGraphicsPipeline(VK_NULL_HANDLE, gpi);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("Failed to create particle pipeline: {}", vk::to_string(result));
        m_device.destroyShaderModule(vert_module);
        m_device.destroyShaderModule(frag_module);
        return;
    }
    m_pipeline = pipe;

    m_device.destroyShaderModule(vert_module);
    m_device.destroyShaderModule(frag_module);

    KNG_INFO("Particle graphics pipeline created");
}

void GpuParticleSystem::upload_particles(vk::Device device)
{
    if (!m_staging_mapped) return;
    std::memcpy(m_staging_mapped, m_particles.data(), sizeof(GpuParticle) * MAX_PARTICLES);
}

GpuParticle& GpuParticleSystem::get_free_particle()
{
    for (auto& p : m_particles) {
        if (p.size_flags.z < 0.5f) return p;
    }
    return m_particles[0]; // overwrite oldest
}

void GpuParticleSystem::fixed_update(Registry& registry, double dt)
{
    const float fdt = static_cast<float>(dt);

    // Update existing particles
    m_alive_count = 0;
    for (auto& p : m_particles) {
        if (p.size_flags.z < 0.5f) continue; // dead

        p.velocity_age.w += fdt; // age
        if (p.velocity_age.w >= p.position_lifetime.w) {
            p.size_flags.z = 0.0f; // kill
            continue;
        }

        // Gravity
        const float grav = p.size_flags.w; // gravity_scale
        p.velocity_age.y -= 9.81f * grav * fdt;

        // Move
        p.position_lifetime.x += p.velocity_age.x * fdt;
        p.position_lifetime.y += p.velocity_age.y * fdt;
        p.position_lifetime.z += p.velocity_age.z * fdt;

        ++m_alive_count;
    }

    // Emit from ParticleEmitter components
    for (const Entity e : registry.view<ParticleEmitter>()) {
        auto& emitter = registry.get_component<ParticleEmitter>(e);
        if (!emitter.active || !emitter.use_gpu) continue;

        glm::vec3 world_pos = emitter.position_offset;
        if (registry.has_component<Position>(e)) {
            const auto& pos = registry.get_component<Position>(e);
            world_pos += glm::vec3(pos.x, pos.y, pos.z);
        }

        m_emit_accumulator += emitter.emit_rate * fdt;
        const int to_emit = static_cast<int>(m_emit_accumulator);
        m_emit_accumulator -= static_cast<float>(to_emit);

        for (int i = 0; i < to_emit; ++i) {
            GpuParticle& p = get_free_particle();
            p.size_flags.z = 1.0f; // alive
            p.velocity_age.w = 0.0f; // age = 0
            p.position_lifetime.w = rand_range(emitter.lifetime_min, emitter.lifetime_max);
            p.position_lifetime.x = world_pos.x;
            p.position_lifetime.y = world_pos.y;
            p.position_lifetime.z = world_pos.z;
            p.velocity_age.x = rand_range(emitter.velocity_min.x, emitter.velocity_max.x);
            p.velocity_age.y = rand_range(emitter.velocity_min.y, emitter.velocity_max.y);
            p.velocity_age.z = rand_range(emitter.velocity_min.z, emitter.velocity_max.z);
            p.color_start = emitter.color_start;
            p.color_end = emitter.color_end;
            p.size_flags.x = emitter.size_start;
            p.size_flags.y = emitter.size_end;
            p.size_flags.w = emitter.gravity_scale;
        }
    }

    // Upload to staging buffer
    if (m_gpu_initialized) {
        upload_particles(m_device);
    }
}

void GpuParticleSystem::variable_update(Registry& /*registry*/, double /*dt*/)
{
    // Particles update in fixed_update
}

void GpuParticleSystem::upload_to_gpu(vk::CommandBuffer cmd)
{
    if (!m_gpu_initialized || m_alive_count == 0) return;

    // Copy staging -> SSBO
    vk::BufferCopy copy_region;
    copy_region.size = sizeof(GpuParticle) * MAX_PARTICLES;
    cmd.copyBuffer(m_staging_buffer, m_ssbo, 1, &copy_region);

    // Memory barrier: transfer -> shader read
    vk::BufferMemoryBarrier barrier;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    barrier.buffer = m_ssbo;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eVertexShader,
                        {}, 0, nullptr, 1, &barrier, 0, nullptr);
}

void GpuParticleSystem::record_draw(vk::CommandBuffer cmd, const glm::mat4& view,
                                     const glm::mat4& proj, const glm::vec3& camera_pos)
{
    if (!m_gpu_initialized || !m_pipeline || m_alive_count == 0) return;

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout,
                           0, 1, &m_desc_set, 0, nullptr);

    // Push constants: view_proj (64) + camera_right (16) + camera_up (16) = 96 bytes
    const glm::mat4 view_proj = proj * view;

    // Extract camera right and up from view matrix
    const glm::vec3 cam_right = glm::vec3(view[0][0], view[1][0], view[2][0]);
    const glm::vec3 cam_up = glm::vec3(view[0][1], view[1][1], view[2][1]);

    struct PushData {
        glm::mat4 view_proj;
        glm::vec4 camera_right;
        glm::vec4 camera_up;
    } push;
    push.view_proj = view_proj;
    push.camera_right = glm::vec4(cam_right, 0.0f);
    push.camera_up = glm::vec4(cam_up, 0.0f);

    cmd.pushConstants(m_pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, 96, &push);

    // Bind quad vertex buffer and draw instanced
    vk::Buffer vb[] = {m_quad_vb};
    vk::DeviceSize offsets[] = {0};
    cmd.bindVertexBuffers(0, 1, vb, offsets);
    cmd.draw(6, MAX_PARTICLES, 0, 0); // 6 verts per quad, MAX_PARTICLES instances
}

void GpuParticleSystem::emit_burst(const glm::vec3& position, int count,
                                    const glm::vec4& color_start,
                                    const glm::vec4& color_end,
                                    float speed, float lifetime,
                                    float gravity_scale)
{
    for (int i = 0; i < count; ++i) {
        GpuParticle& p = get_free_particle();
        p.size_flags.z = 1.0f; // alive
        p.velocity_age.w = 0.0f; // age
        p.position_lifetime.w = rand_range(lifetime * 0.5f, lifetime * 1.5f);
        p.position_lifetime.x = position.x;
        p.position_lifetime.y = position.y;
        p.position_lifetime.z = position.z;

        // Random direction on sphere
        const float theta = rand_range(0.0f, 6.2831853f);
        const float phi = rand_range(-1.0f, 1.0f);
        const float r = std::sqrt(1.0f - phi * phi);
        const float spd = rand_range(speed * 0.5f, speed * 1.5f);
        p.velocity_age.x = r * std::cos(theta) * spd;
        p.velocity_age.y = (std::abs(phi) + 0.5f) * spd;
        p.velocity_age.z = r * std::sin(theta) * spd;

        p.color_start = color_start;
        p.color_end = color_end;
        p.size_flags.x = 0.15f; // world-space size start
        p.size_flags.y = 0.02f; // world-space size end
        p.size_flags.w = gravity_scale;
    }
}

void GpuParticleSystem::emit_burst_typed(const glm::vec3& position, int count, int particle_type)
{
    switch (particle_type) {
    case 0: // sparks
        emit_burst(position, count,
                   {1.0f, 0.7f, 0.1f, 1.0f}, {1.0f, 0.1f, 0.0f, 0.0f},
                   8.0f, 0.5f, 1.0f);
        break;
    case 1: // smoke
        emit_burst(position, count,
                   {0.5f, 0.5f, 0.5f, 0.6f}, {0.3f, 0.3f, 0.3f, 0.0f},
                   2.0f, 2.0f, -0.3f); // negative gravity = floats up
        break;
    case 2: // fire
        emit_burst(position, count,
                   {1.0f, 0.6f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 0.0f},
                   3.0f, 1.0f, -0.5f);
        break;
    case 3: // trail
        emit_burst(position, count,
                   {0.3f, 0.6f, 1.0f, 0.8f}, {0.1f, 0.2f, 0.5f, 0.0f},
                   1.0f, 1.5f, 0.0f); // no gravity
        break;
    default: // custom / sparks fallback
        emit_burst(position, count,
                   {1.0f, 0.7f, 0.1f, 1.0f}, {1.0f, 0.1f, 0.0f, 0.0f},
                   5.0f, 0.6f, 1.0f);
        break;
    }
}

} // namespace kenga
