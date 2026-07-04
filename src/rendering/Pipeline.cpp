/**
 * @file Pipeline.cpp
 * @brief Реализация graphics pipeline
 */

#include "rendering/Pipeline.h"
#include <stdexcept>
#include "rendering/Vertex.h"
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

#include <cstdlib>

namespace kenga {

Pipeline::Pipeline(VulkanContext& context, RenderPass& render_pass, Swapchain& swapchain,
                   const std::vector<uint32_t>& vert_spirv, const std::vector<uint32_t>& frag_spirv)
    : m_context(context)
    , m_render_pass(render_pass)
    , m_swapchain(swapchain)
{
    create_descriptor_set_layout();
    create_pipeline(vert_spirv, frag_spirv);
}

Pipeline::~Pipeline()
{
    cleanup();
}

void Pipeline::create_descriptor_set_layout()
{
    vk::DescriptorSetLayoutBinding ubo_binding;
    ubo_binding.binding = 0;
    ubo_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    ubo_binding.descriptorCount = 1;
    ubo_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;
    ubo_binding.pImmutableSamplers = nullptr;

    vk::DescriptorSetLayoutCreateInfo layout_info;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &ubo_binding;

    vk::Result result = m_context.device().createDescriptorSetLayout(&layout_info, nullptr,
                                                                     &m_descriptor_set_layout);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("createDescriptorSetLayout: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
}

void Pipeline::create_pipeline(const std::vector<uint32_t>& vert_spirv,
                              const std::vector<uint32_t>& frag_spirv)
{
    vk::ShaderModule vert_module;
    vk::ShaderModuleCreateInfo vert_info;
    vert_info.codeSize = vert_spirv.size() * sizeof(uint32_t);
    vert_info.pCode = vert_spirv.data();
    vk::Result result = m_context.device().createShaderModule(&vert_info, nullptr, &vert_module);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("createVertexShaderModule: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    vk::ShaderModule frag_module;
    vk::ShaderModuleCreateInfo frag_info;
    frag_info.codeSize = frag_spirv.size() * sizeof(uint32_t);
    frag_info.pCode = frag_spirv.data();
    result = m_context.device().createShaderModule(&frag_info, nullptr, &frag_module);
    if (result != vk::Result::eSuccess) {
        m_context.device().destroyShaderModule(vert_module);
        KNG_CRITICAL("createFragmentShaderModule: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    vk::PipelineShaderStageCreateInfo vert_stage;
    vert_stage.stage = vk::ShaderStageFlagBits::eVertex;
    vert_stage.module = vert_module;
    vert_stage.pName = "main";

    vk::PipelineShaderStageCreateInfo frag_stage;
    frag_stage.stage = vk::ShaderStageFlagBits::eFragment;
    frag_stage.module = frag_module;
    frag_stage.pName = "main";

    const std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {vert_stage, frag_stage};

    const auto binding = Vertex::get_binding_description();
    const auto attributes = Vertex::get_attribute_descriptions();
    vk::PipelineVertexInputStateCreateInfo vertex_input;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertex_input.pVertexAttributeDescriptions = attributes.data();

    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchain.extent().width);
    viewport.height = static_cast<float>(m_swapchain.extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = m_swapchain.extent();

    const std::array<vk::DynamicState, 2> dynamic_states = {vk::DynamicState::eViewport,
                                                           vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamic_state;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    vk::PipelineViewportStateCreateInfo viewport_state;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    color_blend_attachment.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo color_blending;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    vk::PipelineLayoutCreateInfo layout_info;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &m_descriptor_set_layout;

    result = m_context.device().createPipelineLayout(&layout_info, nullptr, &m_pipeline_layout);
    if (result != vk::Result::eSuccess) {
        m_context.device().destroyShaderModule(frag_module);
        m_context.device().destroyShaderModule(vert_module);
        KNG_CRITICAL("createPipelineLayout: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }

    vk::GraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages.data();
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = m_render_pass.get();
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    KNG_DEBUG("Creating graphics pipeline...");
    result = m_context.device().createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                                        &m_pipeline);

    m_context.device().destroyShaderModule(frag_module);
    m_context.device().destroyShaderModule(vert_module);

    if (result != vk::Result::eSuccess) {
        m_context.device().destroyPipelineLayout(m_pipeline_layout);
        m_pipeline_layout = VK_NULL_HANDLE;
        KNG_CRITICAL("createGraphicsPipelines: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
}

void Pipeline::cleanup()
{
    if (!m_context.device()) {
        return;
    }
    if (m_pipeline) {
        m_context.device().destroyPipeline(m_pipeline);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipeline_layout) {
        m_context.device().destroyPipelineLayout(m_pipeline_layout);
        m_pipeline_layout = VK_NULL_HANDLE;
    }
    if (m_descriptor_set_layout) {
        m_context.device().destroyDescriptorSetLayout(m_descriptor_set_layout);
        m_descriptor_set_layout = VK_NULL_HANDLE;
    }
}

} // namespace kenga
