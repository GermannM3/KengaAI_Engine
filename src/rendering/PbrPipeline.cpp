/**
 * @file PbrPipeline.cpp
 * @brief PBR pipeline implementation
 */

#include "rendering/PbrPipeline.h"
#include <stdexcept>
#include "rendering/PbrVertex.h"
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

#include <cstdlib>

namespace kenga {

PbrPipeline::PbrPipeline(VulkanContext& context, vk::RenderPass render_pass, Swapchain& swapchain,
                         const std::vector<uint32_t>& vert_spirv,
                         const std::vector<uint32_t>& frag_spirv)
    : m_context(context)
    , m_render_pass(render_pass)
    , m_swapchain(swapchain)
{
    create_descriptor_set_layout();
    create_pipeline(vert_spirv, frag_spirv);
}

PbrPipeline::~PbrPipeline()
{
    cleanup();
}

void PbrPipeline::create_descriptor_set_layout()
{
    vk::DescriptorSetLayoutBinding ubo_binding;
    ubo_binding.binding = 0;
    ubo_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    ubo_binding.descriptorCount = 1;
    ubo_binding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    ubo_binding.pImmutableSamplers = nullptr;

    vk::DescriptorSetLayoutBinding sampler_binding;
    sampler_binding.binding = 1;
    sampler_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    sampler_binding.descriptorCount = 1;
    sampler_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
    sampler_binding.pImmutableSamplers = nullptr;

    vk::DescriptorSetLayoutBinding albedo_binding;
    albedo_binding.binding = 2;
    albedo_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    albedo_binding.descriptorCount = 1;
    albedo_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
    albedo_binding.pImmutableSamplers = nullptr;

    vk::DescriptorSetLayoutBinding irradiance_binding;
    irradiance_binding.binding = 3;
    irradiance_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    irradiance_binding.descriptorCount = 1;
    irradiance_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
    irradiance_binding.pImmutableSamplers = nullptr;

    vk::DescriptorSetLayoutBinding prefiltered_binding;
    prefiltered_binding.binding = 4;
    prefiltered_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    prefiltered_binding.descriptorCount = 1;
    prefiltered_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
    prefiltered_binding.pImmutableSamplers = nullptr;

    vk::DescriptorSetLayoutBinding brdf_lut_binding;
    brdf_lut_binding.binding = 5;
    brdf_lut_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    brdf_lut_binding.descriptorCount = 1;
    brdf_lut_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
    brdf_lut_binding.pImmutableSamplers = nullptr;

    // Binding 6: Joint matrices SSBO (skeletal animation)
    vk::DescriptorSetLayoutBinding joint_ssbo_binding;
    joint_ssbo_binding.binding = 6;
    joint_ssbo_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
    joint_ssbo_binding.descriptorCount = 1;
    joint_ssbo_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;
    joint_ssbo_binding.pImmutableSamplers = nullptr;

    const std::array<vk::DescriptorSetLayoutBinding, 7> bindings = {
        ubo_binding, sampler_binding, albedo_binding,
        irradiance_binding, prefiltered_binding, brdf_lut_binding,
        joint_ssbo_binding};

    vk::DescriptorSetLayoutCreateInfo layout_info;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    vk::Result result = m_context.device().createDescriptorSetLayout(&layout_info, nullptr,
                                                                     &m_descriptor_set_layout);
    if (result != vk::Result::eSuccess) {
        KNG_CRITICAL("createDescriptorSetLayout: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
}

void PbrPipeline::create_pipeline(const std::vector<uint32_t>& vert_spirv,
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

    const auto binding = PbrVertex::get_binding_description();
    const auto attributes = PbrVertex::get_attribute_descriptions();
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

    vk::PipelineDepthStencilStateCreateInfo depth_stencil;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    vk::PipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    color_blend_attachment.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo color_blending;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    // Push constants: mat4 model (64 bytes, vertex) + vec4 color (16 bytes, fragment) = 80 bytes
    vk::PushConstantRange push_ranges[2];
    push_ranges[0].stageFlags = vk::ShaderStageFlagBits::eVertex;
    push_ranges[0].offset = 0;
    push_ranges[0].size = 64; // mat4 model

    push_ranges[1].stageFlags = vk::ShaderStageFlagBits::eFragment;
    push_ranges[1].offset = 64;
    push_ranges[1].size = 16; // vec4 baseColor_metallic_roughness

    vk::PipelineLayoutCreateInfo layout_info;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &m_descriptor_set_layout;
    layout_info.pushConstantRangeCount = 2;
    layout_info.pPushConstantRanges = push_ranges;

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
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = m_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    KNG_DEBUG("Creating PBR graphics pipeline...");
    result = m_context.device().createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                                        &m_pipeline);

    if (result == vk::Result::eSuccess) {
        rasterizer.polygonMode = vk::PolygonMode::eLine;
        result = m_context.device().createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipeline_info,
                                                            nullptr, &m_wireframe_pipeline);
    }

    m_context.device().destroyShaderModule(frag_module);
    m_context.device().destroyShaderModule(vert_module);

    if (result != vk::Result::eSuccess) {
        if (m_wireframe_pipeline) {
            m_context.device().destroyPipeline(m_wireframe_pipeline);
            m_wireframe_pipeline = VK_NULL_HANDLE;
        }
        if (m_pipeline) {
            m_context.device().destroyPipeline(m_pipeline);
            m_pipeline = VK_NULL_HANDLE;
        }
        m_context.device().destroyPipelineLayout(m_pipeline_layout);
        m_pipeline_layout = VK_NULL_HANDLE;
        KNG_CRITICAL("createGraphicsPipelines PBR: {} at {}", vk::to_string(result), __FUNCTION__);
        throw std::runtime_error("Fatal engine error");
    }
}

void PbrPipeline::cleanup()
{
    if (!m_context.device()) {
        return;
    }
    if (m_wireframe_pipeline) {
        m_context.device().destroyPipeline(m_wireframe_pipeline);
        m_wireframe_pipeline = VK_NULL_HANDLE;
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
