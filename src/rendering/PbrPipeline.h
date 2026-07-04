/**
 * @file PbrPipeline.h
 * @brief PBR graphics pipeline (model, view, proj in UBO)
 *
 * PROJECT_RULES.md. Фаза 3.3: PBR.
 */

#pragma once

#include "rendering/VulkanContext.h"
#include "rendering/Swapchain.h"

#include <vulkan/vulkan.hpp>

#include <vector>

namespace kenga {

class PbrPipeline {
public:
    PbrPipeline(VulkanContext& context, vk::RenderPass render_pass, Swapchain& swapchain,
                const std::vector<uint32_t>& vert_spirv, const std::vector<uint32_t>& frag_spirv);
    ~PbrPipeline();

    PbrPipeline(const PbrPipeline&) = delete;
    PbrPipeline& operator=(const PbrPipeline&) = delete;

    vk::Pipeline get() const { return m_pipeline; }
    vk::Pipeline get_wireframe() const { return m_wireframe_pipeline; }
    vk::PipelineLayout layout() const { return m_pipeline_layout; }
    vk::DescriptorSetLayout descriptor_set_layout() const { return m_descriptor_set_layout; }

    /// Max number of lights supported in UBO
    static constexpr int max_lights = 8;

    /// UBO size: 4*mat4(256) + 8*GpuLight(512) + num_lights(4) + pad(12) + camera_pos(16) + ibl_intensity(4) + pad(12) = 816
    static constexpr size_t ubo_size = 816;

private:
    void create_descriptor_set_layout();
    void create_pipeline(const std::vector<uint32_t>& vert_spirv,
                        const std::vector<uint32_t>& frag_spirv);
    void cleanup();

    VulkanContext& m_context;
    vk::RenderPass m_render_pass;
    Swapchain& m_swapchain;

    vk::DescriptorSetLayout m_descriptor_set_layout = VK_NULL_HANDLE;
    vk::PipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    vk::Pipeline m_pipeline = VK_NULL_HANDLE;
    vk::Pipeline m_wireframe_pipeline = VK_NULL_HANDLE;
};

} // namespace kenga
