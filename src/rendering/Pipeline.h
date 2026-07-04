/**
 * @file Pipeline.h
 * @brief Graphics pipeline for simple triangle
 *
 * PROJECT_RULES.md. Фаза 3.2: Simple triangle.
 */

#pragma once

#include "rendering/VulkanContext.h"
#include "rendering/RenderPass.h"
#include "rendering/Swapchain.h"

#include <vulkan/vulkan.hpp>

#include <vector>

namespace kenga {

class Pipeline {
public:
    Pipeline(VulkanContext& context, RenderPass& render_pass, Swapchain& swapchain,
             const std::vector<uint32_t>& vert_spirv, const std::vector<uint32_t>& frag_spirv);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    vk::Pipeline get() const { return m_pipeline; }
    vk::PipelineLayout layout() const { return m_pipeline_layout; }
    vk::DescriptorSetLayout descriptor_set_layout() const { return m_descriptor_set_layout; }

private:
    void create_descriptor_set_layout();
    void create_pipeline(const std::vector<uint32_t>& vert_spirv,
                        const std::vector<uint32_t>& frag_spirv);
    void cleanup();

    VulkanContext& m_context;
    RenderPass& m_render_pass;
    Swapchain& m_swapchain;

    vk::DescriptorSetLayout m_descriptor_set_layout = VK_NULL_HANDLE;
    vk::PipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    vk::Pipeline m_pipeline = VK_NULL_HANDLE;
};

} // namespace kenga
