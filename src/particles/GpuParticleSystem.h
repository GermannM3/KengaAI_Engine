/**
 * @file GpuParticleSystem.h
 * @brief GPU-rendered particle system with Vulkan pipeline
 *
 * Particles are updated on CPU, uploaded to SSBO, and rendered as
 * billboard quads with alpha blending in the HDR render pass.
 */

#pragma once

#include "ecs/Entity.h"
#include "ecs/ISystem.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <vector>

namespace kenga {

class Registry;
class VulkanContext;

/// @brief GPU-side particle data (matches GLSL GpuParticle struct, 80 bytes)
struct GpuParticle {
    glm::vec4 position_lifetime;  ///< xyz = world pos, w = total lifetime
    glm::vec4 velocity_age;       ///< xyz = velocity, w = current age
    glm::vec4 color_start;
    glm::vec4 color_end;
    glm::vec4 size_flags;         ///< x = size_start, y = size_end, z = alive, w = gravity_scale
};
static_assert(sizeof(GpuParticle) == 80, "GpuParticle must be 80 bytes");

/**
 * @brief Manages GPU-rendered particles.
 *
 * CPU simulation + GPU rendering via SSBO + instanced billboard quads.
 * Integrates into the HDR render pass for correct blending with scene.
 */
class GpuParticleSystem : public ISystem {
public:
    static constexpr int MAX_PARTICLES = 8192;

    GpuParticleSystem();
    ~GpuParticleSystem();

    /// Initialize Vulkan resources (call after VulkanContext is ready)
    void init_gpu(VulkanContext& context, vk::CommandPool cmd_pool,
                  vk::RenderPass render_pass, vk::Extent2D extent);

    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;

    /// Upload particle data to GPU (call BEFORE render pass begins)
    void upload_to_gpu(vk::CommandBuffer cmd);

    /// Record draw commands into the given command buffer (call INSIDE HDR render pass)
    void record_draw(vk::CommandBuffer cmd, const glm::mat4& view,
                     const glm::mat4& proj, const glm::vec3& camera_pos);

    /// Emit a burst of particles at a world position
    void emit_burst(const glm::vec3& position, int count,
                    const glm::vec4& color_start = {1.0f, 0.7f, 0.1f, 1.0f},
                    const glm::vec4& color_end = {1.0f, 0.1f, 0.0f, 0.0f},
                    float speed = 5.0f, float lifetime = 0.6f,
                    float gravity_scale = 1.0f);

    /// Emit burst with preset type
    void emit_burst_typed(const glm::vec3& position, int count, int particle_type);

    int alive_count() const { return m_alive_count; }
    bool is_gpu_ready() const { return m_gpu_initialized; }

    void cleanup();

private:
    void create_pipeline(VulkanContext& context, vk::RenderPass render_pass, vk::Extent2D extent);
    void create_buffers(VulkanContext& context);
    void upload_particles(vk::Device device);

    GpuParticle& get_free_particle();

    // CPU particle pool
    std::vector<GpuParticle> m_particles;
    int m_alive_count = 0;
    float m_emit_accumulator = 0.0f;

    // Vulkan resources
    bool m_gpu_initialized = false;
    vk::Device m_device;
    vk::PhysicalDevice m_physical_device;

    // SSBO (device-local + host-visible staging)
    vk::Buffer m_ssbo;
    vk::DeviceMemory m_ssbo_memory;
    vk::Buffer m_staging_buffer;
    vk::DeviceMemory m_staging_memory;
    void* m_staging_mapped = nullptr;

    // Quad vertex buffer (6 vertices for a quad)
    vk::Buffer m_quad_vb;
    vk::DeviceMemory m_quad_vb_memory;

    // Pipeline
    vk::Pipeline m_pipeline;
    vk::PipelineLayout m_pipeline_layout;
    vk::DescriptorSetLayout m_desc_set_layout;
    vk::DescriptorPool m_desc_pool;
    vk::DescriptorSet m_desc_set;

    // Command pool for one-time transfers
    vk::CommandPool m_cmd_pool;
};

} // namespace kenga
