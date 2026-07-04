/**
 * @file PbrVertex.cpp
 * @brief PbrVertex binding/attribute descriptions
 */

#include "rendering/PbrVertex.h"

namespace kenga {

vk::VertexInputBindingDescription PbrVertex::get_binding_description()
{
    vk::VertexInputBindingDescription binding;
    binding.binding = 0;
    binding.stride = sizeof(PbrVertex);
    binding.inputRate = vk::VertexInputRate::eVertex;
    return binding;
}

std::array<vk::VertexInputAttributeDescription, 5> PbrVertex::get_attribute_descriptions()
{
    std::array<vk::VertexInputAttributeDescription, 5> attributes;

    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = vk::Format::eR32G32B32Sfloat;
    attributes[0].offset = offsetof(PbrVertex, pos);

    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = vk::Format::eR32G32B32Sfloat;
    attributes[1].offset = offsetof(PbrVertex, normal);

    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = vk::Format::eR32G32Sfloat;
    attributes[2].offset = offsetof(PbrVertex, uv);

    attributes[3].binding = 0;
    attributes[3].location = 3;
    attributes[3].format = vk::Format::eR32G32B32A32Uint;
    attributes[3].offset = offsetof(PbrVertex, joints);

    attributes[4].binding = 0;
    attributes[4].location = 4;
    attributes[4].format = vk::Format::eR32G32B32A32Sfloat;
    attributes[4].offset = offsetof(PbrVertex, weights);

    return attributes;
}

} // namespace kenga
