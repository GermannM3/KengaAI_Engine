/**
 * @file PbrVertex.h
 * @brief Vertex for PBR pipeline (position + normal)
 *
 * PROJECT_RULES.md. Фаза 3.3: PBR.
 */

#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include <array>

namespace kenga {

struct PbrVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::uvec4 joints = {0, 0, 0, 0};   ///< bone indices (up to 4 influences)
    glm::vec4 weights = {1, 0, 0, 0};    ///< bone weights (default: 100% joint 0)

    static vk::VertexInputBindingDescription get_binding_description();
    static std::array<vk::VertexInputAttributeDescription, 5> get_attribute_descriptions();
};

} // namespace kenga
