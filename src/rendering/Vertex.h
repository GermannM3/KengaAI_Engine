/**
 * @file Vertex.h
 * @brief Vertex struct for simple triangle pipeline
 *
 * PROJECT_RULES.md. Фаза 3.2: Simple triangle.
 */

#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include <array>

namespace kenga {

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription get_binding_description();
    static std::array<vk::VertexInputAttributeDescription, 2> get_attribute_descriptions();
};

} // namespace kenga
