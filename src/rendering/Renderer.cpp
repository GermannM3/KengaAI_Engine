/**
 * @file Renderer.cpp
 * @brief VulkanRenderer — PBR, glTF, Camera
 *
 * PROJECT_RULES.md. Фаза 3.3: Camera, PBR, glTF.
 */

#include "rendering/Renderer.h"
#include "rendering/Swapchain.h"
#include "rendering/RenderPass.h"
#include "rendering/PbrPipeline.h"
#include "rendering/ShadowMap.h"
#include "rendering/PostProcess.h"
#include "rendering/VfxPasses.h"
#include "rendering/GltfMesh.h"
#include "rendering/Texture.h"
#include "rendering/FrustumCuller.h"
#include "particles/GpuParticleSystem.h"
#include "rendering/ShaderCompiler.h"
#include "assets/SkyboxLoader.h"
#include "ecs/Components.h"
#include "ecs/Registry.h"
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include "core/LoggerMacros.h"
#include "core/Profiler.h"
#include "core/Settings.h"
#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map>
#include <memory>
#include <sstream>

namespace kenga {

namespace {

std::string load_file(const std::string& path)
{
    std::ifstream f(path);
    if (!f) {
        return {};
    }
    std::stringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

std::vector<uint32_t> compile_glsl(const std::string& source, shaderc_shader_kind kind,
                                 const char* name)
{
    return ShaderCompiler::compile(source, static_cast<int>(kind), name);
}

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
    KNG_CRITICAL("Failed to find suitable memory type at {}", __FUNCTION__);
    throw std::runtime_error("Fatal engine error");
}

} // namespace

class VulkanRenderer : public Renderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer() override { shutdown(); }

    void init(GLFWwindow* window) override;
    void set_scene(Registry* registry, Entity camera_entity, Entity cube_entity,
                   Entity plane_entity) override;
    void set_wireframe(bool wireframe) override { m_wireframe = wireframe; }
    void set_light_direction(const glm::vec3& dir) override { m_light_dir = glm::normalize(dir); }
    void set_skybox_enabled(bool enabled) override { m_skybox_enabled = enabled; }
    void set_ibl_intensity(float intensity) override { m_ibl_intensity = intensity; }
    void render(double alpha) override;
    void shutdown() override;
    void on_resize(int width, int height) override;

    VkInstance get_vk_instance() const override {
        return static_cast<VkInstance>(m_context->instance());
    }
    VkPhysicalDevice get_vk_physical_device() const override {
        return static_cast<VkPhysicalDevice>(m_context->physical_device());
    }
    VkDevice get_vk_device() const override {
        return static_cast<VkDevice>(m_context->device());
    }
    uint32_t get_graphics_queue_family() const override {
        return m_context->graphics_queue_family();
    }
    VkQueue get_vk_graphics_queue() const override {
        return static_cast<VkQueue>(m_context->graphics_queue());
    }
    VkRenderPass get_vk_render_pass() const override {
        return static_cast<VkRenderPass>(m_render_pass->get());
    }
    VkRenderPass get_vk_imgui_render_pass() const override {
        return static_cast<VkRenderPass>(m_imgui_render_pass);
    }
    uint32_t get_swapchain_image_count() const override {
        return m_swapchain->image_count();
    }
    VkCommandPool get_vk_command_pool() const override {
        return static_cast<VkCommandPool>(m_command_pool);
    }

    GpuParticleSystem* get_gpu_particle_system() override {
        return m_gpu_particles.get();
    }

    VfxSettings* get_vfx_settings() override {
        return m_vfx_passes ? &m_vfx_passes->settings() : nullptr;
    }

private:
    // Skinned mesh GPU buffers (keyed by GltfMesh pointer as uintptr_t)
    struct SkinnedGpuMesh {
        vk::Buffer vertex_buffer = VK_NULL_HANDLE;
        vk::DeviceMemory vertex_memory = VK_NULL_HANDLE;
        vk::Buffer index_buffer = VK_NULL_HANDLE;
        vk::DeviceMemory index_memory = VK_NULL_HANDLE;
        uint32_t index_count = 0;
    };

    void create_tonemap_render_pass();
    void create_mesh_resources();
    void cleanup_mesh_resources();
    void create_sync_objects();
    void cleanup_sync_objects();
    void record_command_buffer(uint32_t image_index);
    void update_uniform_buffer(uint32_t frame_index);
    void create_skybox_pipeline();
    void cleanup_skybox_pipeline();
    const SkinnedGpuMesh& ensure_skinned_gpu_mesh(const GltfMesh* mesh);

    std::vector<uint32_t> load_and_compile_pbr_shaders();
    void load_and_compile_shadow_shaders();
    void create_shadow_pipeline();

    GLFWwindow* m_window = nullptr;
    Registry* m_registry = nullptr;
    Entity m_camera_entity = INVALID_ENTITY;
    Entity m_cube_entity = INVALID_ENTITY;
    Entity m_plane_entity = INVALID_ENTITY;
    bool m_wireframe = false;
    bool m_skybox_enabled = true;
    float m_ibl_intensity = 1.0f;
    glm::vec3 m_light_dir = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.5f));

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Swapchain> m_swapchain;
    std::unique_ptr<RenderPass> m_render_pass;
    std::unique_ptr<PbrPipeline> m_pipeline;
    std::unique_ptr<ShadowMap> m_shadow_map;
    std::unique_ptr<PostProcess> m_post_process;
    std::unique_ptr<VfxPasses> m_vfx_passes;
    vk::RenderPass m_tonemap_render_pass = VK_NULL_HANDLE;
    std::vector<vk::Framebuffer> m_tonemap_framebuffers;
    vk::RenderPass m_imgui_render_pass = VK_NULL_HANDLE;
    vk::Pipeline m_shadow_pipeline = VK_NULL_HANDLE;
    vk::PipelineLayout m_shadow_pipeline_layout = VK_NULL_HANDLE;

    GltfMesh m_mesh;
    GltfMesh m_plane_mesh;
    Texture m_albedo_texture;
    std::vector<uint32_t> m_vert_spirv;
    std::vector<uint32_t> m_frag_spirv;
    std::vector<uint32_t> m_shadow_vert_spirv;

    vk::CommandPool m_command_pool = VK_NULL_HANDLE;
    std::vector<vk::CommandBuffer> m_command_buffers;
    std::vector<vk::Semaphore> m_image_available_semaphores;
    std::vector<vk::Semaphore> m_render_finished_semaphores;
    std::vector<vk::Fence> m_in_flight_fences;

    vk::Buffer m_vertex_buffer = VK_NULL_HANDLE;
    vk::DeviceMemory m_vertex_buffer_memory = VK_NULL_HANDLE;
    vk::Buffer m_index_buffer = VK_NULL_HANDLE;
    vk::DeviceMemory m_index_buffer_memory = VK_NULL_HANDLE;
    vk::Buffer m_plane_vertex_buffer = VK_NULL_HANDLE;
    vk::DeviceMemory m_plane_vertex_buffer_memory = VK_NULL_HANDLE;
    vk::Buffer m_plane_index_buffer = VK_NULL_HANDLE;
    vk::DeviceMemory m_plane_index_buffer_memory = VK_NULL_HANDLE;

    std::map<uintptr_t, SkinnedGpuMesh> m_skinned_gpu_meshes;
    uint32_t m_plane_index_count = 0;

    std::vector<vk::Buffer> m_uniform_buffers;
    std::vector<vk::DeviceMemory> m_uniform_buffers_memory;
    std::vector<void*> m_uniform_buffers_mapped;

    // Joint matrices SSBO (skeletal animation) — per frame in flight
    static constexpr size_t max_joints = 128;
    static constexpr size_t joint_ssbo_size = max_joints * sizeof(glm::mat4); // 8192 bytes
    std::vector<vk::Buffer> m_joint_ssbos;
    std::vector<vk::DeviceMemory> m_joint_ssbo_memory;
    std::vector<void*> m_joint_ssbo_mapped;

    vk::DescriptorPool m_descriptor_pool = VK_NULL_HANDLE;
    std::vector<vk::DescriptorSet> m_descriptor_sets;

    // Skybox
    std::unique_ptr<SkyboxLoader> m_skybox_loader;
    vk::Pipeline m_skybox_pipeline = VK_NULL_HANDLE;
    vk::PipelineLayout m_skybox_pipeline_layout = VK_NULL_HANDLE;
    vk::DescriptorSetLayout m_skybox_desc_layout = VK_NULL_HANDLE;
    vk::DescriptorPool m_skybox_desc_pool = VK_NULL_HANDLE;
    vk::DescriptorSet m_skybox_desc_set = VK_NULL_HANDLE;

    static constexpr size_t max_frames_in_flight = 2;
    size_t m_current_frame = 0;
    int m_framebuffer_width = 1280;
    int m_framebuffer_height = 720;
    uint32_t m_index_count = 0;
    bool m_first_frame_logged = false;
    bool m_first_shadow_frame = true;

    // GPU particle system (owned by renderer for Vulkan resource access)
    std::unique_ptr<GpuParticleSystem> m_gpu_particles;

    // Frustum culling
    FrustumCuller m_frustum_culler;
};

std::vector<uint32_t> VulkanRenderer::load_and_compile_pbr_shaders()
{
    const std::array<std::string, 4> search_paths = {
        "shaders/pbr.vert",
        "src/rendering/shaders/pbr.vert",
        "../src/rendering/shaders/pbr.vert",
        "../../src/rendering/shaders/pbr.vert",
    };

    std::string vert_src;
    std::string frag_src;

    for (const auto& base : search_paths) {
        const std::string vert_path = base;
        const std::string frag_path = base.substr(0, base.rfind('.')) + ".frag";
        vert_src = load_file(vert_path);
        frag_src = load_file(frag_path);
        if (!vert_src.empty() && !frag_src.empty()) {
            break;
        }
    }

    if (vert_src.empty() || frag_src.empty()) {
        vert_src = R"(#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
struct GpuLight { vec4 position_type; vec4 direction_cutoff; vec4 color_intensity; vec4 attenuation; };
layout(set = 0, binding = 0, std140) uniform UniformBufferObject { mat4 model; mat4 view; mat4 proj; mat4 light_mvp; GpuLight lights[8]; int num_lights; int _p0; int _p1; int _p2; vec4 camera_pos; float ibl_intensity; float _p3; float _p4; float _p5; } ubo;
layout(push_constant) uniform PushConstants { mat4 model; } pc;
void main() {
    vec4 world_pos = pc.model * vec4(inPosition, 1.0);
    fragWorldPos = world_pos.xyz;
    fragNormal = mat3(transpose(inverse(pc.model))) * inNormal;
    fragUV = inUV;
    gl_Position = ubo.proj * ubo.view * world_pos;
})";
        frag_src = R"(#version 450
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 0) out vec4 outColor;
struct GpuLight { vec4 position_type; vec4 direction_cutoff; vec4 color_intensity; vec4 attenuation; };
layout(set = 0, binding = 0, std140) uniform UniformBufferObject { mat4 model; mat4 view; mat4 proj; mat4 light_mvp; GpuLight lights[8]; int num_lights; int _p0; int _p1; int _p2; vec4 camera_pos; float ibl_intensity; float _p3; float _p4; float _p5; } ubo;
layout(set = 0, binding = 1) uniform sampler2DShadow shadow_sampler;
layout(set = 0, binding = 2) uniform sampler2D albedo_sampler;
layout(push_constant) uniform PushConstants { layout(offset = 64) vec4 baseColor_metallic_roughness; } pc;
void main() {
    vec3 N = normalize(fragNormal);
    vec3 albedo_tex = texture(albedo_sampler, fragUV).rgb;
    vec3 base_color = pc.baseColor_metallic_roughness.rgb * albedo_tex;
    vec3 ambient = base_color * 0.05;
    vec3 total = vec3(0.0);
    for (int i = 0; i < ubo.num_lights; ++i) {
        vec3 L = -normalize(ubo.lights[i].direction_cutoff.xyz);
        float NdotL = max(dot(N, L), 0.0);
        total += base_color * ubo.lights[i].color_intensity.xyz * ubo.lights[i].color_intensity.w * NdotL;
    }
    outColor = vec4(ambient + total, 1.0);
})";
    }

    m_vert_spirv = compile_glsl(vert_src, shaderc_glsl_vertex_shader, "pbr.vert");
    m_frag_spirv = compile_glsl(frag_src, shaderc_glsl_fragment_shader, "pbr.frag");
    return m_vert_spirv;
}

void VulkanRenderer::load_and_compile_shadow_shaders()
{
    const std::array<std::string, 4> paths = {
        "shaders/shadow.vert", "src/rendering/shaders/shadow.vert",
        "../src/rendering/shaders/shadow.vert", "../../src/rendering/shaders/shadow.vert",
    };
    std::string vert_src;
    for (const auto& p : paths) {
        vert_src = load_file(p);
        if (!vert_src.empty()) break;
    }
    if (vert_src.empty()) {
        vert_src = R"(#version 450
layout(location = 0) in vec3 inPosition;
layout(push_constant) uniform PushConstants { mat4 light_mvp; } pc;
void main() { gl_Position = pc.light_mvp * vec4(inPosition, 1.0); })";
    }
    m_shadow_vert_spirv = compile_glsl(vert_src, shaderc_glsl_vertex_shader, "shadow.vert");
}

void VulkanRenderer::create_shadow_pipeline()
{
    std::string frag_src = R"(#version 450
void main() {})";
    const std::array<std::string, 4> frag_paths = {
        "shaders/shadow.frag", "src/rendering/shaders/shadow.frag",
        "../src/rendering/shaders/shadow.frag", "../../src/rendering/shaders/shadow.frag",
    };
    for (const auto& p : frag_paths) {
        const std::string s = load_file(p);
        if (!s.empty()) {
            frag_src = s;
            break;
        }
    }
    const std::vector<uint32_t> frag_spv =
        compile_glsl(frag_src, shaderc_glsl_fragment_shader, "shadow.frag");

    vk::ShaderModule vert_mod = VK_NULL_HANDLE;
    vk::ShaderModule frag_mod = VK_NULL_HANDLE;

    {
        vk::ShaderModuleCreateInfo vi;
        vi.codeSize = m_shadow_vert_spirv.size() * sizeof(uint32_t);
        vi.pCode = m_shadow_vert_spirv.data();
        vk::Result r = m_context->device().createShaderModule(&vi, nullptr, &vert_mod);
        if (r != vk::Result::eSuccess) {
            KNG_CRITICAL("Shadow vert module: {} at {}", vk::to_string(r), __FUNCTION__);
            throw std::runtime_error("Fatal engine error");
        }
    }
    {
        vk::ShaderModuleCreateInfo fi;
        fi.codeSize = frag_spv.size() * sizeof(uint32_t);
        fi.pCode = frag_spv.data();
        vk::Result r = m_context->device().createShaderModule(&fi, nullptr, &frag_mod);
        if (r != vk::Result::eSuccess) {
            m_context->device().destroyShaderModule(vert_mod);
            KNG_CRITICAL("Shadow frag module: {} at {}", vk::to_string(r), __FUNCTION__);
            throw std::runtime_error("Fatal engine error");
        }
    }

    KNG_DEBUG("Shadow vert module: {}", static_cast<bool>(vert_mod));
    KNG_DEBUG("Shadow frag module: {}", static_cast<bool>(frag_mod));

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = vert_mod;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = frag_mod;
    stages[1].pName = "main";

    vk::PushConstantRange pc_range;
    pc_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
    pc_range.offset = 0;
    pc_range.size = 64;

    vk::PipelineLayoutCreateInfo layout_info;
    layout_info.setLayoutCount = 0;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &pc_range;

    vk::Result r = m_context->device().createPipelineLayout(&layout_info, nullptr,
                                                            &m_shadow_pipeline_layout);
    if (r != vk::Result::eSuccess) {
        m_context->device().destroyShaderModule(frag_mod);
        m_context->device().destroyShaderModule(vert_mod);
        KNG_CRITICAL("Shadow pipeline layout: {} at {}", vk::to_string(r), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    const auto binding = PbrVertex::get_binding_description();
    const auto attributes = PbrVertex::get_attribute_descriptions();

    vk::PipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertex_input.pVertexAttributeDescriptions = attributes.data();

    vk::PipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    const uint32_t sw = m_shadow_map->width();
    const uint32_t sh = m_shadow_map->height();
    vk::Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(sw);
    viewport.height = static_cast<float>(sh);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = vk::Extent2D{sw, sh};

    vk::PipelineViewportStateCreateInfo vp_state = {};
    vp_state.viewportCount = 1;
    vp_state.pViewports = &viewport;
    vp_state.scissorCount = 1;
    vp_state.pScissors = &scissor;

    vk::PipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    vk::PipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    vk::PipelineMultisampleStateCreateInfo ms = {};
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::GraphicsPipelineCreateInfo pipe_info = {};
    pipe_info.stageCount = 2;
    pipe_info.pStages = stages.data();
    pipe_info.pVertexInputState = &vertex_input;
    pipe_info.pInputAssemblyState = &input_assembly;
    pipe_info.pViewportState = &vp_state;
    pipe_info.pRasterizationState = &rasterizer;
    pipe_info.pMultisampleState = &ms;
    pipe_info.pDepthStencilState = &depth_stencil;
    pipe_info.pColorBlendState = nullptr;
    pipe_info.layout = m_shadow_pipeline_layout;
    pipe_info.renderPass = m_shadow_map->render_pass();
    pipe_info.subpass = 0;
    pipe_info.basePipelineHandle = VK_NULL_HANDLE;
    pipe_info.basePipelineIndex = -1;

    KNG_DEBUG("Calling vkCreateGraphicsPipelines with 2 stages, layout={} renderPass={}",
              static_cast<bool>(m_shadow_pipeline_layout), static_cast<bool>(m_shadow_map->render_pass()));

    r = m_context->device().createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipe_info, nullptr,
                                                    &m_shadow_pipeline);

    m_context->device().destroyShaderModule(frag_mod);
    m_context->device().destroyShaderModule(vert_mod);

    if (r != vk::Result::eSuccess) {
        m_context->device().destroyPipelineLayout(m_shadow_pipeline_layout);
        m_shadow_pipeline_layout = VK_NULL_HANDLE;
        m_shadow_pipeline = VK_NULL_HANDLE;
        KNG_CRITICAL("vkCreateGraphicsPipelines (shadow) failed: {}", vk::to_string(r));
        KNG_WARN("Shadow pass disabled due to pipeline creation error");
        return;
    }
    KNG_INFO("Shadow pipeline created OK");
}

void VulkanRenderer::create_mesh_resources()
{
    const std::array<const char*, 4> gltf_paths = {
        "assets/cube.gltf",
        "cube.gltf",
        "../assets/cube.gltf",
        "../../assets/cube.gltf",
    };

    bool loaded = false;
    for (const char* path : gltf_paths) {
        if (m_mesh.load_from_file(path)) {
            loaded = true;
            break;
        }
    }
    if (!loaded) {
        KNG_INFO("glTF not found, using hardcoded cube");
        m_mesh.create_cube();
    }

    m_index_count = static_cast<uint32_t>(m_mesh.indices.size());

    m_plane_mesh.create_plane(20.0f, 20.0f);
    m_plane_index_count = static_cast<uint32_t>(m_plane_mesh.indices.size());

    const vk::Device dev = m_context->device();
    const vk::PhysicalDevice phys = m_context->physical_device();

    const size_t vertex_buffer_size = m_mesh.vertices.size() * sizeof(PbrVertex);
    const size_t index_buffer_size = m_mesh.indices.size() * sizeof(uint32_t);

    auto create_buffer = [&](vk::DeviceSize size, vk::BufferUsageFlags usage,
                            vk::MemoryPropertyFlags mem_flags) -> std::pair<vk::Buffer, vk::DeviceMemory> {
        vk::BufferCreateInfo buf_info;
        buf_info.size = size;
        buf_info.usage = usage;
        buf_info.sharingMode = vk::SharingMode::eExclusive;

        vk::Buffer buf;
        vk::Result result = dev.createBuffer(&buf_info, nullptr, &buf);
        if (result != vk::Result::eSuccess) {
            KNG_CRITICAL("createBuffer: {} at {}", vk::to_string(result), __FUNCTION__);
            throw std::runtime_error("Fatal engine error");
        }

        vk::MemoryRequirements mem_req;
        dev.getBufferMemoryRequirements(buf, &mem_req);

        vk::MemoryAllocateInfo alloc_info;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex =
            find_memory_type(phys, mem_req.memoryTypeBits, mem_flags);

        vk::DeviceMemory mem;
        result = dev.allocateMemory(&alloc_info, nullptr, &mem);
        if (result != vk::Result::eSuccess) {
            dev.destroyBuffer(buf);
            KNG_CRITICAL("allocateBuffer memory: {} at {}", vk::to_string(result), __FUNCTION__);
            throw std::runtime_error("Fatal engine error");
        }
        dev.bindBufferMemory(buf, mem, 0);
        return {buf, mem};
    };

    auto copy_buffer = [&](vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) {
        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.commandPool = m_command_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = 1;

        vk::CommandBuffer cmd;
        dev.allocateCommandBuffers(&alloc_info, &cmd);

        vk::CommandBufferBeginInfo begin_info;
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmd.begin(&begin_info);

        vk::BufferCopy copy_region;
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        copy_region.size = size;
        cmd.copyBuffer(src, dst, 1, &copy_region);

        vk::BufferMemoryBarrier barrier;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead |
                               vk::AccessFlagBits::eIndexRead;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = dst;
        barrier.offset = 0;
        barrier.size = size;

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                           vk::PipelineStageFlagBits::eVertexInput, vk::DependencyFlags{}, 0,
                           nullptr, 1, &barrier, 0, nullptr);

        cmd.end();

        vk::SubmitInfo submit_info;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;
        m_context->graphics_queue().submit(1, &submit_info, VK_NULL_HANDLE);
        m_context->graphics_queue().waitIdle();

        dev.freeCommandBuffers(m_command_pool, 1, &cmd);
    };

    {
        auto [staging_buf, staging_mem] = create_buffer(
            vertex_buffer_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* data;
        dev.mapMemory(staging_mem, 0, vertex_buffer_size, {}, &data);
        memcpy(data, m_mesh.vertices.data(), vertex_buffer_size);
        dev.unmapMemory(staging_mem);

        auto [vb, vbm] = create_buffer(
            vertex_buffer_size,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        m_vertex_buffer = vb;
        m_vertex_buffer_memory = vbm;
        copy_buffer(staging_buf, m_vertex_buffer, vertex_buffer_size);
        dev.destroyBuffer(staging_buf);
        dev.freeMemory(staging_mem);
    }

    {
        auto [staging_buf, staging_mem] = create_buffer(
            index_buffer_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* data;
        dev.mapMemory(staging_mem, 0, index_buffer_size, {}, &data);
        memcpy(data, m_mesh.indices.data(), index_buffer_size);
        dev.unmapMemory(staging_mem);

        auto [ib, ibm] = create_buffer(
            index_buffer_size,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        m_index_buffer = ib;
        m_index_buffer_memory = ibm;
        copy_buffer(staging_buf, m_index_buffer, index_buffer_size);
        dev.destroyBuffer(staging_buf);
        dev.freeMemory(staging_mem);
    }

    {
        const size_t plane_vertex_size = m_plane_mesh.vertices.size() * sizeof(PbrVertex);
        const size_t plane_index_size = m_plane_mesh.indices.size() * sizeof(uint32_t);
        auto [stg_buf, stg_mem] = create_buffer(
            plane_vertex_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        void* data;
        dev.mapMemory(stg_mem, 0, plane_vertex_size, {}, &data);
        memcpy(data, m_plane_mesh.vertices.data(), plane_vertex_size);
        dev.unmapMemory(stg_mem);
        auto [vb, vbm] = create_buffer(
            plane_vertex_size,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        m_plane_vertex_buffer = vb;
        m_plane_vertex_buffer_memory = vbm;
        copy_buffer(stg_buf, m_plane_vertex_buffer, plane_vertex_size);
        dev.destroyBuffer(stg_buf);
        dev.freeMemory(stg_mem);

        auto [stg_idx, stg_idx_mem] = create_buffer(
            plane_index_size,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        dev.mapMemory(stg_idx_mem, 0, plane_index_size, {}, &data);
        memcpy(data, m_plane_mesh.indices.data(), plane_index_size);
        dev.unmapMemory(stg_idx_mem);
        auto [ib, ibm] = create_buffer(
            plane_index_size,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        m_plane_index_buffer = ib;
        m_plane_index_buffer_memory = ibm;
        copy_buffer(stg_idx, m_plane_index_buffer, plane_index_size);
        dev.destroyBuffer(stg_idx);
        dev.freeMemory(stg_idx_mem);
    }

    constexpr size_t ubo_size = PbrPipeline::ubo_size;
    m_uniform_buffers.resize(max_frames_in_flight);
    m_uniform_buffers_memory.resize(max_frames_in_flight);
    m_uniform_buffers_mapped.resize(max_frames_in_flight);

    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        auto [ub, ubm] = create_buffer(
            ubo_size,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        m_uniform_buffers[i] = ub;
        m_uniform_buffers_memory[i] = ubm;
        dev.mapMemory(ubm, 0, ubo_size, {}, &m_uniform_buffers_mapped[i]);
    }

    // Create joint SSBO (per frame in flight, host-visible for fast upload)
    m_joint_ssbos.resize(max_frames_in_flight);
    m_joint_ssbo_memory.resize(max_frames_in_flight);
    m_joint_ssbo_mapped.resize(max_frames_in_flight);
    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        auto [jb, jbm] = create_buffer(
            joint_ssbo_size,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        m_joint_ssbos[i] = jb;
        m_joint_ssbo_memory[i] = jbm;
        dev.mapMemory(jbm, 0, joint_ssbo_size, {}, &m_joint_ssbo_mapped[i]);
        // Initialize with identity matrices
        auto* mats = static_cast<glm::mat4*>(m_joint_ssbo_mapped[i]);
        for (size_t j = 0; j < max_joints; ++j) mats[j] = glm::mat4(1.0f);
    }

    const std::array<vk::DescriptorPoolSize, 3> pool_sizes = {
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer,
                              static_cast<uint32_t>(max_frames_in_flight)},
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler,
                              static_cast<uint32_t>(max_frames_in_flight * 5)},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer,
                              static_cast<uint32_t>(max_frames_in_flight)},
    };

    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = static_cast<uint32_t>(max_frames_in_flight * 2);

    vk::Result result = dev.createDescriptorPool(&pool_info, nullptr, &m_descriptor_pool);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("createDescriptorPool: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    std::vector<vk::DescriptorSetLayout> layouts(max_frames_in_flight,
                                                 m_pipeline->descriptor_set_layout());

    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = static_cast<uint32_t>(max_frames_in_flight);
    alloc_info.pSetLayouts = layouts.data();

    m_descriptor_sets.resize(max_frames_in_flight);
    result = dev.allocateDescriptorSets(&alloc_info, m_descriptor_sets.data());
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("allocateDescriptorSets: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        vk::DescriptorBufferInfo buffer_info;
        buffer_info.buffer = m_uniform_buffers[i];
        buffer_info.offset = 0;
        buffer_info.range = ubo_size;

        vk::WriteDescriptorSet writes[7];
        writes[0].dstSet = m_descriptor_sets[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &buffer_info;

        vk::DescriptorImageInfo shadow_image_info;
        shadow_image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        shadow_image_info.imageView = m_shadow_map->image_view();
        shadow_image_info.sampler = m_shadow_map->sampler();

        writes[1].dstSet = m_descriptor_sets[i];
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &shadow_image_info;

        vk::DescriptorImageInfo albedo_image_info;
        albedo_image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        albedo_image_info.imageView = m_albedo_texture.image_view();
        albedo_image_info.sampler = m_albedo_texture.sampler();

        writes[2].dstSet = m_descriptor_sets[i];
        writes[2].dstBinding = 2;
        writes[2].dstArrayElement = 0;
        writes[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        writes[2].descriptorCount = 1;
        writes[2].pImageInfo = &albedo_image_info;

        // IBL textures (bindings 3, 4, 5)
        vk::DescriptorImageInfo irradiance_info;
        irradiance_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        irradiance_info.imageView = m_skybox_loader ? m_skybox_loader->irradiance_view() : m_albedo_texture.image_view();
        irradiance_info.sampler = m_skybox_loader ? m_skybox_loader->irradiance_sampler() : m_albedo_texture.sampler();

        writes[3].dstSet = m_descriptor_sets[i];
        writes[3].dstBinding = 3;
        writes[3].dstArrayElement = 0;
        writes[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        writes[3].descriptorCount = 1;
        writes[3].pImageInfo = &irradiance_info;

        vk::DescriptorImageInfo prefiltered_info;
        prefiltered_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        prefiltered_info.imageView = m_skybox_loader ? m_skybox_loader->prefiltered_view() : m_albedo_texture.image_view();
        prefiltered_info.sampler = m_skybox_loader ? m_skybox_loader->prefiltered_sampler() : m_albedo_texture.sampler();

        writes[4].dstSet = m_descriptor_sets[i];
        writes[4].dstBinding = 4;
        writes[4].dstArrayElement = 0;
        writes[4].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        writes[4].descriptorCount = 1;
        writes[4].pImageInfo = &prefiltered_info;

        vk::DescriptorImageInfo brdf_lut_info;
        brdf_lut_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        brdf_lut_info.imageView = m_skybox_loader ? m_skybox_loader->brdf_lut_view() : m_albedo_texture.image_view();
        brdf_lut_info.sampler = m_skybox_loader ? m_skybox_loader->brdf_lut_sampler() : m_albedo_texture.sampler();

        writes[5].dstSet = m_descriptor_sets[i];
        writes[5].dstBinding = 5;
        writes[5].dstArrayElement = 0;
        writes[5].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        writes[5].descriptorCount = 1;
        writes[5].pImageInfo = &brdf_lut_info;

        // Binding 6: Joint matrices SSBO
        vk::DescriptorBufferInfo joint_ssbo_info;
        joint_ssbo_info.buffer = m_joint_ssbos[i];
        joint_ssbo_info.offset = 0;
        joint_ssbo_info.range = joint_ssbo_size;

        writes[6].dstSet = m_descriptor_sets[i];
        writes[6].dstBinding = 6;
        writes[6].dstArrayElement = 0;
        writes[6].descriptorType = vk::DescriptorType::eStorageBuffer;
        writes[6].descriptorCount = 1;
        writes[6].pBufferInfo = &joint_ssbo_info;

        dev.updateDescriptorSets(7, writes, 0, nullptr);
    }
}

void VulkanRenderer::cleanup_mesh_resources()
{
    const vk::Device dev = m_context->device();
    if (!dev) {
        return;
    }
    if (m_descriptor_pool) {
        dev.destroyDescriptorPool(m_descriptor_pool);
        m_descriptor_pool = VK_NULL_HANDLE;
    }
    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        if (m_uniform_buffers_mapped[i]) {
            dev.unmapMemory(m_uniform_buffers_memory[i]);
            m_uniform_buffers_mapped[i] = nullptr;
        }
        if (m_uniform_buffers_memory[i]) {
            dev.freeMemory(m_uniform_buffers_memory[i]);
            m_uniform_buffers_memory[i] = VK_NULL_HANDLE;
        }
        if (m_uniform_buffers[i]) {
            dev.destroyBuffer(m_uniform_buffers[i]);
            m_uniform_buffers[i] = VK_NULL_HANDLE;
        }
    }
    m_uniform_buffers.clear();
    m_uniform_buffers_memory.clear();
    m_uniform_buffers_mapped.clear();

    // Cleanup joint SSBOs
    for (size_t i = 0; i < m_joint_ssbos.size(); ++i) {
        if (m_joint_ssbo_mapped[i]) {
            dev.unmapMemory(m_joint_ssbo_memory[i]);
            m_joint_ssbo_mapped[i] = nullptr;
        }
        if (m_joint_ssbo_memory[i]) {
            dev.freeMemory(m_joint_ssbo_memory[i]);
            m_joint_ssbo_memory[i] = VK_NULL_HANDLE;
        }
        if (m_joint_ssbos[i]) {
            dev.destroyBuffer(m_joint_ssbos[i]);
            m_joint_ssbos[i] = VK_NULL_HANDLE;
        }
    }
    m_joint_ssbos.clear();
    m_joint_ssbo_memory.clear();
    m_joint_ssbo_mapped.clear();

    m_descriptor_sets.clear();

    if (m_index_buffer) {
        dev.destroyBuffer(m_index_buffer);
        dev.freeMemory(m_index_buffer_memory);
        m_index_buffer = VK_NULL_HANDLE;
        m_index_buffer_memory = VK_NULL_HANDLE;
    }
    if (m_vertex_buffer) {
        dev.destroyBuffer(m_vertex_buffer);
        dev.freeMemory(m_vertex_buffer_memory);
        m_vertex_buffer = VK_NULL_HANDLE;
        m_vertex_buffer_memory = VK_NULL_HANDLE;
    }
    if (m_plane_index_buffer) {
        dev.destroyBuffer(m_plane_index_buffer);
        dev.freeMemory(m_plane_index_buffer_memory);
        m_plane_index_buffer = VK_NULL_HANDLE;
        m_plane_index_buffer_memory = VK_NULL_HANDLE;
    }
    if (m_plane_vertex_buffer) {
        dev.destroyBuffer(m_plane_vertex_buffer);
        dev.freeMemory(m_plane_vertex_buffer_memory);
        m_plane_vertex_buffer = VK_NULL_HANDLE;
        m_plane_vertex_buffer_memory = VK_NULL_HANDLE;
    }
    // Cleanup skinned GPU meshes
    for (auto it = m_skinned_gpu_meshes.begin(); it != m_skinned_gpu_meshes.end(); ++it) {
        if (it->second.index_buffer)  { dev.destroyBuffer(it->second.index_buffer);  dev.freeMemory(it->second.index_memory); }
        if (it->second.vertex_buffer) { dev.destroyBuffer(it->second.vertex_buffer); dev.freeMemory(it->second.vertex_memory); }
    }
    m_skinned_gpu_meshes = {};
}

auto VulkanRenderer::ensure_skinned_gpu_mesh(const GltfMesh* mesh) -> const SkinnedGpuMesh&
{
    const uintptr_t key = reinterpret_cast<uintptr_t>(mesh);
    auto it = m_skinned_gpu_meshes.find(key);
    if (it != m_skinned_gpu_meshes.end()) return it->second;

    const vk::Device dev = m_context->device();
    const vk::PhysicalDevice phys = m_context->physical_device();

    auto find_mem = [&](uint32_t type_bits, vk::MemoryPropertyFlags flags) -> uint32_t {
        auto props = phys.getMemoryProperties();
        for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
            if ((type_bits & (1 << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        }
        return 0;
    };

    auto make_buffer = [&](vk::DeviceSize size, vk::BufferUsageFlags usage,
                           vk::MemoryPropertyFlags mem_flags) -> std::pair<vk::Buffer, vk::DeviceMemory> {
        vk::BufferCreateInfo bi; bi.size = size; bi.usage = usage; bi.sharingMode = vk::SharingMode::eExclusive;
        vk::Buffer buf = dev.createBuffer(bi);
        auto req = dev.getBufferMemoryRequirements(buf);
        vk::MemoryAllocateInfo ai; ai.allocationSize = req.size; ai.memoryTypeIndex = find_mem(req.memoryTypeBits, mem_flags);
        vk::DeviceMemory mem = dev.allocateMemory(ai);
        dev.bindBufferMemory(buf, mem, 0);
        return {buf, mem};
    };

    auto copy_buf = [&](vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) {
        vk::CommandBufferAllocateInfo ai; ai.commandPool = m_command_pool; ai.level = vk::CommandBufferLevel::ePrimary; ai.commandBufferCount = 1;
        auto cmds = dev.allocateCommandBuffers(ai);
        vk::CommandBufferBeginInfo bi; bi.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmds[0].begin(bi);
        vk::BufferCopy region; region.size = size;
        cmds[0].copyBuffer(src, dst, 1, &region);
        cmds[0].end();
        vk::SubmitInfo si; si.commandBufferCount = 1; si.pCommandBuffers = &cmds[0];
        m_context->graphics_queue().submit(1, &si, VK_NULL_HANDLE);
        m_context->graphics_queue().waitIdle();
        dev.freeCommandBuffers(m_command_pool, cmds);
    };

    SkinnedGpuMesh sgm;
    sgm.index_count = static_cast<uint32_t>(mesh->indices.size());

    void* data = nullptr;

    // Vertex buffer
    const vk::DeviceSize vb_size = mesh->vertices.size() * sizeof(PbrVertex);
    auto [stg_v, stg_vm] = make_buffer(vb_size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    dev.mapMemory(stg_vm, 0, vb_size, {}, &data);
    memcpy(data, mesh->vertices.data(), vb_size);
    dev.unmapMemory(stg_vm);
    auto [vb, vbm] = make_buffer(vb_size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    copy_buf(stg_v, vb, vb_size);
    dev.destroyBuffer(stg_v); dev.freeMemory(stg_vm);
    sgm.vertex_buffer = vb; sgm.vertex_memory = vbm;

    // Index buffer
    const vk::DeviceSize ib_size = mesh->indices.size() * sizeof(uint32_t);
    auto [stg_i, stg_im] = make_buffer(ib_size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    dev.mapMemory(stg_im, 0, ib_size, {}, &data);
    memcpy(data, mesh->indices.data(), ib_size);
    dev.unmapMemory(stg_im);
    auto [ib, ibm] = make_buffer(ib_size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    copy_buf(stg_i, ib, ib_size);
    dev.destroyBuffer(stg_i); dev.freeMemory(stg_im);
    sgm.index_buffer = ib; sgm.index_memory = ibm;

    KNG_INFO("Uploaded skinned mesh to GPU: {} verts, {} indices", mesh->vertices.size(), mesh->indices.size());
    auto [ins_it, inserted] = m_skinned_gpu_meshes.emplace(key, sgm);
    return ins_it->second;
}

void VulkanRenderer::create_tonemap_render_pass()
{
    // Render pass for tonemap: single color attachment (swapchain format), no depth
    vk::AttachmentDescription att;
    att.format = m_swapchain->format();
    att.samples = vk::SampleCountFlagBits::e1;
    att.loadOp = vk::AttachmentLoadOp::eDontCare;
    att.storeOp = vk::AttachmentStoreOp::eStore;
    att.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    att.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    att.initialLayout = vk::ImageLayout::eUndefined;
    att.finalLayout = vk::ImageLayout::ePresentSrcKHR;

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

    vk::Result r = m_context->device().createRenderPass(&ci, nullptr, &m_tonemap_render_pass);
    if (r != vk::Result::eSuccess) {
        KNG_CRITICAL("Tonemap render pass: {}", vk::to_string(r));
        throw std::runtime_error("Fatal engine error");
    }
}

void VulkanRenderer::init(GLFWwindow* window)
{
    m_window = window;
    glfwGetFramebufferSize(window, &m_framebuffer_width, &m_framebuffer_height);
    if (m_framebuffer_width <= 0) {
        m_framebuffer_width = 1280;
    }
    if (m_framebuffer_height <= 0) {
        m_framebuffer_height = 720;
    }

    m_context = std::make_unique<VulkanContext>(window);
    m_swapchain = std::make_unique<Swapchain>(*m_context, m_framebuffer_width, m_framebuffer_height);
    m_render_pass = std::make_unique<RenderPass>(*m_context, *m_swapchain);

    // Post-processing: HDR offscreen + bloom + tonemap
    create_tonemap_render_pass();
    m_post_process = std::make_unique<PostProcess>(*m_context, m_framebuffer_width,
                                                    m_framebuffer_height, m_tonemap_render_pass);

    // Advanced VFX passes (god rays, fog, SSR)
    m_vfx_passes = std::make_unique<VfxPasses>(*m_context, m_framebuffer_width, m_framebuffer_height,
                                                m_post_process->post_render_pass(),
                                                m_post_process->hdr_image_view(),
                                                m_post_process->hdr_sampler(),
                                                m_post_process->hdr_depth_view());

    // Create tonemap framebuffers (1 attachment = swapchain image, no depth)
    m_tonemap_framebuffers.resize(m_swapchain->image_count());
    for (uint32_t i = 0; i < m_swapchain->image_count(); ++i) {
        vk::FramebufferCreateInfo fbi;
        fbi.renderPass = m_tonemap_render_pass;
        fbi.attachmentCount = 1;
        fbi.pAttachments = &m_swapchain->image_views()[i];
        fbi.width = m_swapchain->extent().width;
        fbi.height = m_swapchain->extent().height;
        fbi.layers = 1;
        vk::Result r = m_context->device().createFramebuffer(&fbi, nullptr, &m_tonemap_framebuffers[i]);
        if (r != vk::Result::eSuccess) {
            KNG_CRITICAL("Tonemap framebuffer: {}", vk::to_string(r));
            throw std::runtime_error("Fatal engine error");
        }
    }

    // ImGui overlay render pass (loadOp=Load to preserve tonemap output)
    {
        vk::AttachmentDescription att;
        att.format = m_swapchain->format();
        att.samples = vk::SampleCountFlagBits::e1;
        att.loadOp = vk::AttachmentLoadOp::eLoad;
        att.storeOp = vk::AttachmentStoreOp::eStore;
        att.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        att.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        att.initialLayout = vk::ImageLayout::ePresentSrcKHR;
        att.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentReference color_ref{0, vk::ImageLayout::eColorAttachmentOptimal};

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;

        vk::SubpassDependency dep;
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dep.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dep.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        dep.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

        vk::RenderPassCreateInfo ci;
        ci.attachmentCount = 1;
        ci.pAttachments = &att;
        ci.subpassCount = 1;
        ci.pSubpasses = &subpass;
        ci.dependencyCount = 1;
        ci.pDependencies = &dep;

        vk::Result r = m_context->device().createRenderPass(&ci, nullptr, &m_imgui_render_pass);
        if (r != vk::Result::eSuccess) {
            KNG_CRITICAL("ImGui render pass: {}", vk::to_string(r));
            throw std::runtime_error("Fatal engine error");
        }
    }

    m_shadow_map = std::make_unique<ShadowMap>(*m_context, 2048, 2048);
    load_and_compile_pbr_shaders();
    load_and_compile_shadow_shaders();
    create_shadow_pipeline();
    // PBR pipeline uses HDR render pass for post-processing
    m_pipeline = std::make_unique<PbrPipeline>(*m_context,
                                               m_post_process->hdr_render_pass(),
                                               *m_swapchain, m_vert_spirv, m_frag_spirv);

    vk::CommandPoolCreateInfo pool_info;
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = m_context->graphics_queue_family();
    vk::Result result =
        m_context->device().createCommandPool(&pool_info, nullptr, &m_command_pool);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("createCommandPool: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    m_command_buffers.resize(m_swapchain->image_count());
    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = static_cast<uint32_t>(m_command_buffers.size());
    result = m_context->device().allocateCommandBuffers(&alloc_info, m_command_buffers.data());
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("allocateCommandBuffers: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    // Create fallback white texture (used when no albedo texture loaded)
    m_albedo_texture.create_white_fallback(*m_context, m_command_pool);

    // Load skybox HDR and precompute IBL maps
    m_skybox_loader = std::make_unique<SkyboxLoader>();
    if (!m_skybox_loader->load(*m_context, m_command_pool, "assets/sky.hdr")) {
        KNG_WARN("Skybox HDR load failed, IBL disabled");
        m_skybox_loader.reset();
    }

    create_mesh_resources();
    create_skybox_pipeline();
    create_sync_objects();

    // Initialize GPU particle system
    m_gpu_particles = std::make_unique<GpuParticleSystem>();
    m_gpu_particles->init_gpu(*m_context, m_command_pool,
                               m_post_process->hdr_render_pass(),
                               m_swapchain->extent());
}

void VulkanRenderer::set_scene(Registry* registry, Entity camera_entity, Entity cube_entity,
                              Entity plane_entity)
{
    m_registry = registry;
    m_camera_entity = camera_entity;
    m_cube_entity = cube_entity;
    m_plane_entity = plane_entity;
}

void VulkanRenderer::create_sync_objects()
{
    m_image_available_semaphores.resize(max_frames_in_flight);
    m_render_finished_semaphores.resize(max_frames_in_flight);
    m_in_flight_fences.resize(max_frames_in_flight);

    vk::SemaphoreCreateInfo sem_info;
    vk::FenceCreateInfo fence_info;
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        m_context->device().createSemaphore(&sem_info, nullptr, &m_image_available_semaphores[i]);
        m_context->device().createSemaphore(&sem_info, nullptr, &m_render_finished_semaphores[i]);
        m_context->device().createFence(&fence_info, nullptr, &m_in_flight_fences[i]);
    }
}

void VulkanRenderer::cleanup_sync_objects()
{
    if (!m_context || !m_context->device()) {
        return;
    }
    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        m_context->device().destroySemaphore(m_image_available_semaphores[i]);
        m_context->device().destroySemaphore(m_render_finished_semaphores[i]);
        m_context->device().destroyFence(m_in_flight_fences[i]);
    }
    m_image_available_semaphores.clear();
    m_render_finished_semaphores.clear();
    m_in_flight_fences.clear();
}

void VulkanRenderer::update_uniform_buffer(uint32_t frame_index)
{
    glm::mat4 model = glm::mat4(1.0f);
    const float aspect = m_framebuffer_width / static_cast<float>(m_framebuffer_height > 0 ? m_framebuffer_height : 1);
    glm::mat4 view = glm::lookAt(glm::vec3(0, 0, -3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.01f, 500.0f);
    proj[1][1] *= -1.0f; // Vulkan Y-down flip (fallback path)

    if (m_registry && m_registry->is_valid(m_camera_entity) &&
        m_registry->has_component<Camera>(m_camera_entity)) {
        const Camera& cam = m_registry->get_component<Camera>(m_camera_entity);
        view = cam.view;
        proj = cam.proj;
    }

    const glm::vec3 light_pos = -m_light_dir * 25.0f + glm::vec3(0.0f, 10.0f, 0.0f);
    const glm::mat4 light_view = glm::lookAt(light_pos, light_pos + m_light_dir, glm::vec3(0, 1, 0));
    const glm::mat4 light_proj = glm::ortho(-30.0f, 30.0f, -30.0f, 30.0f, 0.1f, 120.0f);
    const glm::mat4 light_mvp = light_proj * light_view;

    struct GpuLight {
        glm::vec4 position_type;
        glm::vec4 direction_cutoff;
        glm::vec4 color_intensity;
        glm::vec4 attenuation;
    };

    struct Ubo {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 light_mvp;
        GpuLight lights[PbrPipeline::max_lights];
        int num_lights;
        float _pad0[3];
        glm::vec4 camera_pos;
        float ibl_intensity;
        float _pad1[3];
    } ubo;
    static_assert(sizeof(Ubo) == PbrPipeline::ubo_size, "UBO size mismatch");

    ubo.model = model;
    ubo.view = view;
    ubo.proj = proj;
    ubo.light_mvp = light_mvp;
    std::memset(ubo.lights, 0, sizeof(ubo.lights));
    ubo.num_lights = 0;
    std::memset(ubo._pad0, 0, sizeof(ubo._pad0));
    std::memset(ubo._pad1, 0, sizeof(ubo._pad1));

    // Camera position for IBL view direction
    glm::vec3 cam_pos(0, 0, -3);
    if (m_registry && m_registry->is_valid(m_camera_entity) &&
        m_registry->has_component<Camera>(m_camera_entity)) {
        cam_pos = m_registry->get_component<Camera>(m_camera_entity).position;
    }
    ubo.camera_pos = glm::vec4(cam_pos, 1.0f);
    ubo.ibl_intensity = m_ibl_intensity;

    // Collect lights from ECS
    if (m_registry) {
        int light_idx = 0;
        for (const Entity e : m_registry->view<Light>()) {
            if (light_idx >= PbrPipeline::max_lights) break;
            const auto& lc = m_registry->get_component<Light>(e);
            GpuLight& gl = ubo.lights[light_idx];
            gl.color_intensity = glm::vec4(lc.color, lc.intensity);
            gl.direction_cutoff = glm::vec4(glm::normalize(lc.direction), lc.outer_cutoff);
            gl.attenuation = glm::vec4(lc.radius, lc.inner_cutoff, 0.0f, 0.0f);

            if (lc.type == LightType::directional) {
                gl.position_type = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            } else if (m_registry->has_component<Position>(e)) {
                const auto& pos = m_registry->get_component<Position>(e);
                gl.position_type = glm::vec4(pos.x, pos.y, pos.z,
                                             static_cast<float>(static_cast<int>(lc.type)));
            } else {
                gl.position_type = glm::vec4(0.0f, 0.0f, 0.0f,
                                             static_cast<float>(static_cast<int>(lc.type)));
            }
            ++light_idx;
        }
        ubo.num_lights = light_idx;
    }

    // Fallback: if no lights in ECS, add default directional
    if (ubo.num_lights == 0) {
        GpuLight& gl = ubo.lights[0];
        gl.position_type = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // directional
        gl.direction_cutoff = glm::vec4(m_light_dir, 0.0f);
        gl.color_intensity = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gl.attenuation = glm::vec4(50.0f, 0.0f, 0.0f, 0.0f);
        ubo.num_lights = 1;
    }

    // Model matrix is set per-draw-call via push constants now.
    ubo.model = glm::mat4(1.0f);
    memcpy(m_uniform_buffers_mapped[frame_index], &ubo, PbrPipeline::ubo_size);

}

void VulkanRenderer::record_command_buffer(uint32_t image_index)
{
    vk::CommandBufferBeginInfo begin_info;
    m_command_buffers[image_index].begin(&begin_info);

    update_uniform_buffer(m_current_frame);

    const vk::Device dev = m_context->device();

    vk::ImageMemoryBarrier shadow_barrier;
    shadow_barrier.oldLayout = m_first_shadow_frame ? vk::ImageLayout::eUndefined
                                                    : vk::ImageLayout::eShaderReadOnlyOptimal;
    shadow_barrier.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    shadow_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shadow_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shadow_barrier.image = m_shadow_map->image();
    shadow_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    shadow_barrier.subresourceRange.baseMipLevel = 0;
    shadow_barrier.subresourceRange.levelCount = 1;
    shadow_barrier.subresourceRange.baseArrayLayer = 0;
    shadow_barrier.subresourceRange.layerCount = 1;
    shadow_barrier.srcAccessMask = vk::AccessFlags{};
    shadow_barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    vk::PipelineStageFlags src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    if (!m_first_shadow_frame) {
        shadow_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        src_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    m_command_buffers[image_index].pipelineBarrier(
        src_stage, vk::PipelineStageFlagBits::eEarlyFragmentTests,
        vk::DependencyFlags{}, 0, nullptr, 0, nullptr, 1, &shadow_barrier);
    m_first_shadow_frame = false;

    // Helper: build model matrix from entity components
    auto build_model_matrix = [&](Entity e) -> glm::mat4 {
        glm::mat4 m = glm::mat4(1.0f);
        if (m_registry->has_component<Position>(e)) {
            const Position& p = m_registry->get_component<Position>(e);
            m = glm::translate(m, glm::vec3(p.x, p.y, p.z));
        }
        if (m_registry->has_component<Orientation>(e)) {
            const Orientation& o = m_registry->get_component<Orientation>(e);
            m = m * glm::mat4_cast(o.q);
        } else if (m_registry->has_component<Rotation>(e)) {
            const Rotation& r = m_registry->get_component<Rotation>(e);
            m = m * glm::rotate(glm::mat4(1.0f), glm::radians(r.angle), glm::vec3(0, 1, 0));
        }
        if (m_registry->has_component<Scale>(e)) {
            const Scale& s = m_registry->get_component<Scale>(e);
            m = m * glm::scale(glm::mat4(1.0f), glm::vec3(s.x, s.y, s.z));
        }
        return m;
    };

    glm::mat4 cube_model = build_model_matrix(m_cube_entity);
    glm::mat4 plane_model = build_model_matrix(m_plane_entity);

    KNG_DEBUG("Shadow pass started (pipeline={})", static_cast<bool>(m_shadow_pipeline));
    if (m_shadow_pipeline) {
    vk::ClearValue depth_clear;
    depth_clear.depthStencil = vk::ClearDepthStencilValue{1.0f, 0};
    vk::RenderPassBeginInfo shadow_rp;
    shadow_rp.renderPass = m_shadow_map->render_pass();
    shadow_rp.framebuffer = m_shadow_map->framebuffer();
    shadow_rp.renderArea = vk::Rect2D{{0, 0}, {m_shadow_map->width(), m_shadow_map->height()}};
    shadow_rp.clearValueCount = 1;
    shadow_rp.pClearValues = &depth_clear;

    KNG_DEBUG("Starting shadow pass");

    const glm::vec3 shadow_light_pos = -m_light_dir * 25.0f + glm::vec3(0.0f, 10.0f, 0.0f);
    const glm::mat4 light_view = glm::lookAt(shadow_light_pos, shadow_light_pos + m_light_dir, glm::vec3(0, 1, 0));
    const glm::mat4 light_proj = glm::ortho(-30.0f, 30.0f, -30.0f, 30.0f, 0.1f, 120.0f);

    m_command_buffers[image_index].beginRenderPass(&shadow_rp, vk::SubpassContents::eInline);
    vk::Viewport shadow_vp;
    shadow_vp.x = 0;
    shadow_vp.y = 0;
    shadow_vp.width = static_cast<float>(m_shadow_map->width());
    shadow_vp.height = static_cast<float>(m_shadow_map->height());
    shadow_vp.minDepth = 0.0f;
    shadow_vp.maxDepth = 1.0f;
    m_command_buffers[image_index].setViewport(0, 1, &shadow_vp);
    vk::Rect2D shadow_scissor{{0, 0}, {m_shadow_map->width(), m_shadow_map->height()}};
    m_command_buffers[image_index].setScissor(0, 1, &shadow_scissor);

    m_command_buffers[image_index].bindPipeline(vk::PipelineBindPoint::eGraphics, m_shadow_pipeline);

    // Shadow: draw all RigidBody entities as cubes
    if (m_registry) {
        vk::Buffer vb[] = {m_vertex_buffer};
        vk::DeviceSize off[] = {0};
        for (const Entity e : m_registry->view<RigidBody>()) {
            const auto& rb = m_registry->get_component<RigidBody>(e);
            if (rb.is_static) {
                // Draw plane for static bodies
                glm::mat4 model = build_model_matrix(e);
                glm::mat4 light_mvp = light_proj * light_view * model;
                m_command_buffers[image_index].pushConstants(m_shadow_pipeline_layout,
                    vk::ShaderStageFlagBits::eVertex, 0, 64, &light_mvp);
                vb[0] = m_plane_vertex_buffer;
                m_command_buffers[image_index].bindVertexBuffers(0, 1, vb, off);
                m_command_buffers[image_index].bindIndexBuffer(m_plane_index_buffer, 0, vk::IndexType::eUint32);
                m_command_buffers[image_index].drawIndexed(m_plane_index_count, 1, 0, 0, 0);
            } else {
                // Draw cube for dynamic bodies
                glm::mat4 model = build_model_matrix(e);
                glm::mat4 light_mvp = light_proj * light_view * model;
                m_command_buffers[image_index].pushConstants(m_shadow_pipeline_layout,
                    vk::ShaderStageFlagBits::eVertex, 0, 64, &light_mvp);
                vb[0] = m_vertex_buffer;
                m_command_buffers[image_index].bindVertexBuffers(0, 1, vb, off);
                m_command_buffers[image_index].bindIndexBuffer(m_index_buffer, 0, vk::IndexType::eUint32);
                m_command_buffers[image_index].drawIndexed(m_index_count, 1, 0, 0, 0);
            }
        }
    }

    m_command_buffers[image_index].endRenderPass();
    }

    shadow_barrier.oldLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    shadow_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    shadow_barrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    shadow_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    m_command_buffers[image_index].pipelineBarrier(
        vk::PipelineStageFlagBits::eLateFragmentTests,
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlags{}, 0, nullptr, 0, nullptr, 1, &shadow_barrier);

    // Upload GPU particle data (must be outside render pass)
    if (m_gpu_particles) {
        m_gpu_particles->upload_to_gpu(m_command_buffers[image_index]);
    }

    const std::array<float, 4> clear_color = {0.05f, 0.05f, 0.1f, 1.0f};
    std::array<vk::ClearValue, 2> clear_values;
    clear_values[0].color = vk::ClearColorValue(clear_color);
    clear_values[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    // Render scene into HDR offscreen framebuffer
    vk::RenderPassBeginInfo rp_info;
    rp_info.renderPass = m_post_process->hdr_render_pass();
    rp_info.framebuffer = m_post_process->hdr_framebuffer();
    rp_info.renderArea.offset = vk::Offset2D{0, 0};
    rp_info.renderArea.extent = m_swapchain->extent();
    rp_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    rp_info.pClearValues = clear_values.data();

    m_command_buffers[image_index].beginRenderPass(&rp_info, vk::SubpassContents::eInline);

    vk::Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchain->extent().width);
    viewport.height = static_cast<float>(m_swapchain->extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    m_command_buffers[image_index].setViewport(0, 1, &viewport);

    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = m_swapchain->extent();
    m_command_buffers[image_index].setScissor(0, 1, &scissor);

    vk::Pipeline current_pipeline =
        m_wireframe ? m_pipeline->get_wireframe() : m_pipeline->get();
    m_command_buffers[image_index].bindPipeline(vk::PipelineBindPoint::eGraphics, current_pipeline);

    m_command_buffers[image_index].bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, m_pipeline->layout(), 0, 1,
        &m_descriptor_sets[m_current_frame], 0, nullptr);

    // Update frustum culler from camera VP
    {
        glm::mat4 cam_view(1.0f), cam_proj(1.0f);
        if (m_registry && m_registry->is_valid(m_camera_entity) &&
            m_registry->has_component<Camera>(m_camera_entity)) {
            const auto& cam = m_registry->get_component<Camera>(m_camera_entity);
            cam_view = cam.view;
            cam_proj = cam.proj;
        }
        m_frustum_culler.update(cam_proj * cam_view);
    }

    auto& prof = get_profiler();
    prof.draw_calls = 0;
    prof.culled_count = 0;

    // Camera position for LOD distance calculation
    glm::vec3 cam_pos(0.0f);
    if (m_registry && m_registry->is_valid(m_camera_entity) &&
        m_registry->has_component<Camera>(m_camera_entity)) {
        cam_pos = m_registry->get_component<Camera>(m_camera_entity).position;
    }

    // Helper: get entity AABB center and half-extents
    auto entity_aabb = [&](Entity e, bool is_static) -> std::pair<glm::vec3, glm::vec3> {
        glm::vec3 center(0.0f);
        glm::vec3 extents(0.5f); // default cube half-extent
        if (m_registry->has_component<Position>(e)) {
            const auto& p = m_registry->get_component<Position>(e);
            center = glm::vec3(p.x, p.y, p.z);
        }
        if (m_registry->has_component<Scale>(e)) {
            const auto& s = m_registry->get_component<Scale>(e);
            extents = glm::vec3(s.x, s.y, s.z) * 0.5f;
        }
        if (is_static) {
            // Ground plane is large — always visible
            extents = glm::vec3(50.0f, 0.5f, 50.0f);
        }
        return {center, extents};
    };

    // Draw all RigidBody entities using push constants for per-draw model matrix
    if (m_registry) {
        const glm::vec4 cube_colors[] = {
            {0.8f, 0.2f, 0.2f, 0.5f},
            {0.2f, 0.8f, 0.2f, 0.5f},
            {0.2f, 0.2f, 0.8f, 0.5f},
            {0.8f, 0.8f, 0.2f, 0.5f},
            {0.8f, 0.2f, 0.8f, 0.5f},
        };
        int cube_idx = 0;

        for (const Entity e : m_registry->view<RigidBody>()) {
            const auto& rb = m_registry->get_component<RigidBody>(e);

            // Frustum culling
            auto [aabb_center, aabb_extents] = entity_aabb(e, rb.is_static);
            if (!m_frustum_culler.is_aabb_visible(aabb_center, aabb_extents)) {
                ++prof.culled_count;
                if (!rb.is_static) ++cube_idx;
                continue;
            }

            // LOD distance culling
            if (!rb.is_static && m_registry->has_component<LodInfo>(e)) {
                auto& lod = m_registry->get_component<LodInfo>(e);
                const float dist = glm::distance(cam_pos, aabb_center);
                if (dist < lod.lod1_distance) {
                    lod.current_lod = 0;
                } else if (dist < lod.lod2_distance) {
                    lod.current_lod = 1;
                } else {
                    lod.current_lod = 2;
                    if (lod.cull_at_lod2) {
                        ++prof.culled_count;
                        ++cube_idx;
                        continue;
                    }
                }
            }

            glm::mat4 model = build_model_matrix(e);
            m_command_buffers[image_index].pushConstants(m_pipeline->layout(),
                vk::ShaderStageFlagBits::eVertex, 0, 64, &model);

            if (rb.is_static) {
                vk::Buffer vb[] = {m_plane_vertex_buffer};
                vk::DeviceSize off[] = {0};
                m_command_buffers[image_index].bindVertexBuffers(0, 1, vb, off);
                m_command_buffers[image_index].bindIndexBuffer(m_plane_index_buffer, 0, vk::IndexType::eUint32);
                glm::vec4 color(0.4f, 0.4f, 0.4f, 0.5f);
                m_command_buffers[image_index].pushConstants(m_pipeline->layout(),
                    vk::ShaderStageFlagBits::eFragment, 64, 16, &color);
                m_command_buffers[image_index].drawIndexed(m_plane_index_count, 1, 0, 0, 0);
            } else {
                vk::Buffer vb[] = {m_vertex_buffer};
                vk::DeviceSize off[] = {0};
                m_command_buffers[image_index].bindVertexBuffers(0, 1, vb, off);
                m_command_buffers[image_index].bindIndexBuffer(m_index_buffer, 0, vk::IndexType::eUint32);
                glm::vec4 color = cube_colors[cube_idx % 5];
                m_command_buffers[image_index].pushConstants(m_pipeline->layout(),
                    vk::ShaderStageFlagBits::eFragment, 64, 16, &color);
                m_command_buffers[image_index].drawIndexed(m_index_count, 1, 0, 0, 0);
                ++cube_idx;
            }
            ++prof.draw_calls;
        }
    }

    // Draw skinned mesh entities
    if (m_registry) {
        for (const Entity e : m_registry->view<SkinnedMesh>()) {
            auto& sm = m_registry->get_component<SkinnedMesh>(e);
            if (!sm.mesh || sm.mesh->vertices.empty()) continue;

            // Frustum culling (use sphere test, radius ~2m for humanoid)
            auto [aabb_center, aabb_extents] = entity_aabb(e, false);
            if (!m_frustum_culler.is_sphere_visible(aabb_center, 2.0f)) {
                ++prof.culled_count;
                continue;
            }

            auto* ssbo_data = static_cast<glm::mat4*>(m_joint_ssbo_mapped[m_current_frame]);
            const size_t joint_count = std::min(sm.joint_matrices.size(), max_joints);
            if (joint_count > 0) {
                memcpy(ssbo_data, sm.joint_matrices.data(), joint_count * sizeof(glm::mat4));
            }
            for (size_t j = joint_count; j < max_joints; ++j) {
                ssbo_data[j] = glm::mat4(1.0f);
            }

            const auto& gpu_mesh = ensure_skinned_gpu_mesh(sm.mesh.get());

            glm::mat4 model = build_model_matrix(e);
            m_command_buffers[image_index].pushConstants(m_pipeline->layout(),
                vk::ShaderStageFlagBits::eVertex, 0, 64, &model);

            glm::vec4 color(0.9f, 0.85f, 0.7f, 0.5f);
            m_command_buffers[image_index].pushConstants(m_pipeline->layout(),
                vk::ShaderStageFlagBits::eFragment, 64, 16, &color);

            vk::Buffer vb[] = {gpu_mesh.vertex_buffer};
            vk::DeviceSize off[] = {0};
            m_command_buffers[image_index].bindVertexBuffers(0, 1, vb, off);
            m_command_buffers[image_index].bindIndexBuffer(gpu_mesh.index_buffer, 0, vk::IndexType::eUint32);
            m_command_buffers[image_index].drawIndexed(gpu_mesh.index_count, 1, 0, 0, 0);
            ++prof.draw_calls;
        }
    }

    if (!m_first_frame_logged) {
        KNG_INFO("PBR scene draw executed (RigidBody + SkinnedMesh entities)");
    }

    // Skybox pass (inside HDR render pass, after scene geometry)
    if (m_skybox_enabled && m_skybox_pipeline && m_skybox_loader && m_skybox_loader->is_loaded()) {
        m_command_buffers[image_index].bindPipeline(vk::PipelineBindPoint::eGraphics, m_skybox_pipeline);
        m_command_buffers[image_index].bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, m_skybox_pipeline_layout, 0, 1,
            &m_skybox_desc_set, 0, nullptr);

        // Push constant: proj * mat4(mat3(view)) — strips translation
        glm::mat4 sky_view = glm::mat4(1.0f);
        glm::mat4 sky_proj = glm::perspective(glm::radians(60.0f),
            static_cast<float>(m_swapchain->extent().width) /
            static_cast<float>(std::max(m_swapchain->extent().height, 1u)), 0.01f, 500.0f);
        sky_proj[1][1] *= -1.0f; // Vulkan Y-down flip (fallback path)
        if (m_registry && m_registry->is_valid(m_camera_entity) &&
            m_registry->has_component<Camera>(m_camera_entity)) {
            const Camera& cam = m_registry->get_component<Camera>(m_camera_entity);
            sky_view = glm::mat4(glm::mat3(cam.view)); // strip translation
            sky_proj = cam.proj;
        }
        glm::mat4 sky_vp = sky_proj * sky_view;
        m_command_buffers[image_index].pushConstants(m_skybox_pipeline_layout,
            vk::ShaderStageFlagBits::eVertex, 0, 64, &sky_vp);

        // Draw cube mesh as skybox
        vk::Buffer vb[] = {m_vertex_buffer};
        vk::DeviceSize off[] = {0};
        m_command_buffers[image_index].bindVertexBuffers(0, 1, vb, off);
        m_command_buffers[image_index].bindIndexBuffer(m_index_buffer, 0, vk::IndexType::eUint32);
        m_command_buffers[image_index].drawIndexed(m_index_count, 1, 0, 0, 0);
    }

    // GPU particles (inside HDR render pass, after skybox)
    if (m_gpu_particles && m_gpu_particles->is_gpu_ready()) {
        glm::mat4 p_view = glm::mat4(1.0f);
        glm::mat4 p_proj = glm::perspective(glm::radians(60.0f),
            static_cast<float>(m_swapchain->extent().width) /
            static_cast<float>(std::max(m_swapchain->extent().height, 1u)), 0.01f, 500.0f);
        p_proj[1][1] *= -1.0f;
        glm::vec3 p_cam_pos(0, 0, -3);
        if (m_registry && m_registry->is_valid(m_camera_entity) &&
            m_registry->has_component<Camera>(m_camera_entity)) {
            const Camera& cam = m_registry->get_component<Camera>(m_camera_entity);
            p_view = cam.view;
            p_proj = cam.proj;
            p_cam_pos = cam.position;
        }

        // Set viewport/scissor for particle pipeline (uses dynamic state)
        vk::Viewport part_vp;
        part_vp.x = 0;
        part_vp.y = 0;
        part_vp.width = static_cast<float>(m_swapchain->extent().width);
        part_vp.height = static_cast<float>(m_swapchain->extent().height);
        part_vp.minDepth = 0.0f;
        part_vp.maxDepth = 1.0f;
        m_command_buffers[image_index].setViewport(0, 1, &part_vp);
        vk::Rect2D part_sc{{0, 0}, m_swapchain->extent()};
        m_command_buffers[image_index].setScissor(0, 1, &part_sc);

        m_gpu_particles->record_draw(m_command_buffers[image_index], p_view, p_proj, p_cam_pos);
    }

    // End HDR scene render pass
    m_command_buffers[image_index].endRenderPass();

    // Advanced VFX passes (god rays, fog, SSR) — between HDR scene and bloom/tonemap
    if (m_vfx_passes) {
        glm::mat4 cam_view(1.0f), cam_proj(1.0f);
        float cam_y = 0.0f;
        if (m_registry && m_registry->is_valid(m_camera_entity) &&
            m_registry->has_component<Camera>(m_camera_entity)) {
            const auto& cam = m_registry->get_component<Camera>(m_camera_entity);
            cam_view = cam.view;
            cam_proj = cam.proj;
            cam_y = cam.position.y;
        }
        glm::mat4 vp = cam_proj * cam_view;
        glm::mat4 inv_vp = glm::inverse(vp);

        m_vfx_passes->record_commands(m_command_buffers[image_index],
                                       m_vfx_passes->settings(),
                                       0.1f, 1000.0f, cam_y,
                                       inv_vp, vp);
    }

    // Post-processing: bloom + tonemap -> swapchain
    m_post_process->record_commands(
        m_command_buffers[image_index],
        m_tonemap_framebuffers[image_index],
        m_tonemap_render_pass,
        m_swapchain->extent(),
        1.0f,   // exposure
        0.3f,   // bloom strength
        1.0f    // bloom threshold
    );

    // ImGui overlay pass (loadOp=Load to preserve tonemap output)
    if (ImGui::GetCurrentContext() && ImGui::GetDrawData()) {
        vk::RenderPassBeginInfo imgui_rp;
        imgui_rp.renderPass = m_imgui_render_pass;
        imgui_rp.framebuffer = m_tonemap_framebuffers[image_index];
        imgui_rp.renderArea.offset = vk::Offset2D{0, 0};
        imgui_rp.renderArea.extent = m_swapchain->extent();
        imgui_rp.clearValueCount = 0;
        imgui_rp.pClearValues = nullptr;

        m_command_buffers[image_index].beginRenderPass(&imgui_rp, vk::SubpassContents::eInline);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                        static_cast<VkCommandBuffer>(m_command_buffers[image_index]));
        m_command_buffers[image_index].endRenderPass();
    }

    m_command_buffers[image_index].end();
}

void VulkanRenderer::render(double alpha)
{
    (void)alpha;

    m_context->device().waitForFences(1, &m_in_flight_fences[m_current_frame], VK_TRUE,
                                      UINT64_MAX);
    m_context->device().resetFences(1, &m_in_flight_fences[m_current_frame]);

    KNG_DEBUG("Acquiring image...");
    vk::Semaphore image_available = m_image_available_semaphores[m_current_frame];
    uint32_t image_index = m_swapchain->acquire_next_image(image_available);

    if (image_index == static_cast<uint32_t>(-1)) {
        glfwGetFramebufferSize(m_window, &m_framebuffer_width, &m_framebuffer_height);
        if (m_framebuffer_width > 0 && m_framebuffer_height > 0) {
            on_resize(m_framebuffer_width, m_framebuffer_height);
        }
        return;
    }

    {
        auto& prof = get_profiler();
        prof.begin(Profiler::Section::render_record);
        record_command_buffer(image_index);
        prof.end(Profiler::Section::render_record);
    }

    if (!m_first_frame_logged) {
        m_first_frame_logged = true;
    }

    get_profiler().begin(Profiler::Section::render_submit);

    vk::SubmitInfo submit_info;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_available;
    const vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffers[image_index];
    vk::Semaphore render_finished = m_render_finished_semaphores[m_current_frame];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished;

    m_context->graphics_queue().submit(1, &submit_info, m_in_flight_fences[m_current_frame]);
    KNG_DEBUG("Submit done");

    vk::Result present_result =
        m_swapchain->present(m_context->present_queue(), image_index, render_finished);
    if (present_result == vk::Result::eErrorOutOfDateKHR ||
        present_result == vk::Result::eSuboptimalKHR) {
        on_resize(m_framebuffer_width, m_framebuffer_height);
    }

    get_profiler().end(Profiler::Section::render_submit);

    m_current_frame = (m_current_frame + 1) % max_frames_in_flight;
    KNG_DEBUG("Frame rendered successfully");
}

void VulkanRenderer::create_skybox_pipeline()
{
    if (!m_skybox_loader || !m_skybox_loader->is_loaded()) {
        return;
    }
    auto dev = m_context->device();

    // Compile skybox shaders
    std::string sky_vert_src = load_file("shaders/skybox.vert");
    std::string sky_frag_src = load_file("shaders/skybox.frag");
    if (sky_vert_src.empty() || sky_frag_src.empty()) {
        KNG_WARN("Skybox shaders not found");
        return;
    }
    auto sky_vert_spirv = compile_glsl(sky_vert_src, shaderc_vertex_shader, "skybox.vert");
    auto sky_frag_spirv = compile_glsl(sky_frag_src, shaderc_fragment_shader, "skybox.frag");

    // Descriptor set layout: single cubemap sampler
    vk::DescriptorSetLayoutBinding binding;
    binding.binding = 0;
    binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    binding.descriptorCount = 1;
    binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
    vk::DescriptorSetLayoutCreateInfo dsli;
    dsli.bindingCount = 1;
    dsli.pBindings = &binding;
    m_skybox_desc_layout = dev.createDescriptorSetLayout(dsli);

    // Descriptor pool
    vk::DescriptorPoolSize ps;
    ps.type = vk::DescriptorType::eCombinedImageSampler;
    ps.descriptorCount = 1;
    vk::DescriptorPoolCreateInfo dpi;
    dpi.maxSets = 1;
    dpi.poolSizeCount = 1;
    dpi.pPoolSizes = &ps;
    m_skybox_desc_pool = dev.createDescriptorPool(dpi);

    // Allocate and write descriptor set
    vk::DescriptorSetAllocateInfo dsai;
    dsai.descriptorPool = m_skybox_desc_pool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &m_skybox_desc_layout;
    dev.allocateDescriptorSets(&dsai, &m_skybox_desc_set);

    vk::DescriptorImageInfo dii;
    dii.sampler = m_skybox_loader->cubemap_sampler();
    dii.imageView = m_skybox_loader->cubemap_view();
    dii.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    vk::WriteDescriptorSet wds;
    wds.dstSet = m_skybox_desc_set;
    wds.dstBinding = 0;
    wds.descriptorCount = 1;
    wds.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    wds.pImageInfo = &dii;
    dev.updateDescriptorSets(1, &wds, 0, nullptr);

    // Pipeline layout: push constant mat4 (64 bytes)
    vk::PushConstantRange push;
    push.stageFlags = vk::ShaderStageFlagBits::eVertex;
    push.offset = 0;
    push.size = 64;
    vk::PipelineLayoutCreateInfo pli;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &m_skybox_desc_layout;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &push;
    m_skybox_pipeline_layout = dev.createPipelineLayout(pli);

    // Shader modules
    vk::ShaderModuleCreateInfo smi;
    smi.codeSize = sky_vert_spirv.size() * 4;
    smi.pCode = sky_vert_spirv.data();
    auto vert_mod = dev.createShaderModule(smi);
    smi.codeSize = sky_frag_spirv.size() * 4;
    smi.pCode = sky_frag_spirv.data();
    auto frag_mod = dev.createShaderModule(smi);

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages;
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = vert_mod;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = frag_mod;
    stages[1].pName = "main";

    // Vertex input: reuse PbrVertex layout
    auto vb_binding = PbrVertex::get_binding_description();
    auto vb_attrs = PbrVertex::get_attribute_descriptions();
    vk::PipelineVertexInputStateCreateInfo vertex_input;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vb_binding;
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(vb_attrs.size());
    vertex_input.pVertexAttributeDescriptions = vb_attrs.data();

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

    // Depth: test enabled (LessOrEqual), write DISABLED (skybox behind everything)
    vk::PipelineDepthStencilStateCreateInfo ds;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = vk::CompareOp::eLessOrEqual;

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
    gpi.layout = m_skybox_pipeline_layout;
    gpi.renderPass = m_post_process->hdr_render_pass(); // same HDR render pass as PBR

    auto [result, pipe] = dev.createGraphicsPipeline(VK_NULL_HANDLE, gpi);
    m_skybox_pipeline = pipe;

    dev.destroyShaderModule(vert_mod);
    dev.destroyShaderModule(frag_mod);

    if (result != vk::Result::eSuccess) {
        KNG_WARN("Skybox pipeline creation failed: {}", vk::to_string(result));
        m_skybox_pipeline = VK_NULL_HANDLE;
    } else {
        KNG_INFO("Skybox pipeline created");
    }
}

void VulkanRenderer::cleanup_skybox_pipeline()
{
    if (!m_context || !m_context->device()) return;
    auto dev = m_context->device();
    if (m_skybox_pipeline) { dev.destroyPipeline(m_skybox_pipeline); m_skybox_pipeline = VK_NULL_HANDLE; }
    if (m_skybox_pipeline_layout) { dev.destroyPipelineLayout(m_skybox_pipeline_layout); m_skybox_pipeline_layout = VK_NULL_HANDLE; }
    if (m_skybox_desc_pool) { dev.destroyDescriptorPool(m_skybox_desc_pool); m_skybox_desc_pool = VK_NULL_HANDLE; }
    if (m_skybox_desc_layout) { dev.destroyDescriptorSetLayout(m_skybox_desc_layout); m_skybox_desc_layout = VK_NULL_HANDLE; }
}

void VulkanRenderer::shutdown()
{
    if (!m_context || !m_context->device()) {
        return;
    }
    m_context->device().waitIdle();
    if (m_gpu_particles) { m_gpu_particles->cleanup(); m_gpu_particles.reset(); }
    cleanup_skybox_pipeline();
    if (m_skybox_loader) { m_skybox_loader->destroy(); m_skybox_loader.reset(); }
    m_albedo_texture.destroy();
    m_vfx_passes.reset();
    m_post_process.reset();
    for (auto fb : m_tonemap_framebuffers) {
        m_context->device().destroyFramebuffer(fb);
    }
    m_tonemap_framebuffers.clear();
    if (m_tonemap_render_pass) {
        m_context->device().destroyRenderPass(m_tonemap_render_pass);
        m_tonemap_render_pass = VK_NULL_HANDLE;
    }
    if (m_imgui_render_pass) {
        m_context->device().destroyRenderPass(m_imgui_render_pass);
        m_imgui_render_pass = VK_NULL_HANDLE;
    }
    cleanup_mesh_resources();
    cleanup_sync_objects();
    if (m_shadow_pipeline) {
        m_context->device().destroyPipeline(m_shadow_pipeline);
        m_shadow_pipeline = VK_NULL_HANDLE;
    }
    if (m_shadow_pipeline_layout) {
        m_context->device().destroyPipelineLayout(m_shadow_pipeline_layout);
        m_shadow_pipeline_layout = VK_NULL_HANDLE;
    }
    m_shadow_map.reset();
    m_pipeline.reset();
    if (m_command_pool) {
        m_context->device().destroyCommandPool(m_command_pool);
        m_command_pool = VK_NULL_HANDLE;
    }
    m_render_pass.reset();
    m_swapchain.reset();
    m_context.reset();
    m_window = nullptr;
    m_registry = nullptr;
    m_camera_entity = INVALID_ENTITY;
    m_cube_entity = INVALID_ENTITY;
    m_plane_entity = INVALID_ENTITY;
}

void VulkanRenderer::on_resize(int width, int height)
{
    m_framebuffer_width = width;
    m_framebuffer_height = height;
    if (m_framebuffer_width <= 0 || m_framebuffer_height <= 0) {
        return;
    }
    m_context->device().waitIdle();
    cleanup_mesh_resources();
    m_pipeline.reset();
    m_swapchain->recreate(m_framebuffer_width, m_framebuffer_height);
    m_render_pass->recreate();

    // Recreate post-processing for new resolution
    m_post_process->recreate(m_framebuffer_width, m_framebuffer_height);

    // Recreate VFX passes
    if (m_vfx_passes) {
        m_vfx_passes->recreate(m_framebuffer_width, m_framebuffer_height,
                               m_post_process->hdr_image_view(),
                               m_post_process->hdr_depth_view());
    }

    // Recreate tonemap framebuffers
    for (auto fb : m_tonemap_framebuffers) {
        m_context->device().destroyFramebuffer(fb);
    }
    m_tonemap_framebuffers.resize(m_swapchain->image_count());
    for (uint32_t i = 0; i < m_swapchain->image_count(); ++i) {
        vk::FramebufferCreateInfo fbi;
        fbi.renderPass = m_tonemap_render_pass;
        fbi.attachmentCount = 1;
        fbi.pAttachments = &m_swapchain->image_views()[i];
        fbi.width = m_swapchain->extent().width;
        fbi.height = m_swapchain->extent().height;
        fbi.layers = 1;
        m_context->device().createFramebuffer(&fbi, nullptr, &m_tonemap_framebuffers[i]);
    }

    m_pipeline = std::make_unique<PbrPipeline>(*m_context,
                                               m_post_process->hdr_render_pass(),
                                               *m_swapchain, m_vert_spirv, m_frag_spirv);
    create_mesh_resources();

    // Recreate skybox pipeline (references HDR render pass)
    cleanup_skybox_pipeline();
    create_skybox_pipeline();

    // Recreate GPU particle pipeline for new extent
    if (m_gpu_particles) {
        m_gpu_particles->cleanup();
        m_gpu_particles->init_gpu(*m_context, m_command_pool,
                                   m_post_process->hdr_render_pass(),
                                   m_swapchain->extent());
    }

    m_context->device().freeCommandBuffers(m_command_pool, m_command_buffers);
    m_command_buffers.resize(m_swapchain->image_count());
    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = static_cast<uint32_t>(m_command_buffers.size());
    m_context->device().allocateCommandBuffers(&alloc_info, m_command_buffers.data());
}

std::unique_ptr<Renderer> create_vulkan_renderer()
{
    return std::make_unique<VulkanRenderer>();
}

} // namespace kenga
