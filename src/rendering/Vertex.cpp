/**
 * @file Vertex.cpp
 * @brief Vertex binding/attribute descriptions
 */

#include "rendering/Vertex.h"

namespace kenga {

vk::VertexInputBindingDescription Vertex::get_binding_description()
{
    vk::VertexInputBindingDescription binding;
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = vk::VertexInputRate::eVertex;
    return binding;
}

std::array<vk::VertexInputAttributeDescription, 2> Vertex::get_attribute_descriptions()
{
    std::array<vk::VertexInputAttributeDescription, 2> attributes;

    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = vk::Format::eR32G32Sfloat;
    attributes[0].offset = offsetof(Vertex, pos);

    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = vk::Format::eR32G32B32Sfloat;
    attributes[1].offset = offsetof(Vertex, color);

    return attributes;
}

} // namespace kenga
