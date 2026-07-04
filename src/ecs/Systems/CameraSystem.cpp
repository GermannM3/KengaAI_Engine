/**
 * @file CameraSystem.cpp
 * @brief Реализация CameraSystem
 */

#include "ecs/Systems/CameraSystem.h"
#include "core/LoggerMacros.h"

#include <glm/gtc/matrix_transform.hpp>

namespace kenga {

void CameraSystem::fixed_update(Registry& registry, double dt)
{
    (void)registry;
    (void)dt;
}

void CameraSystem::variable_update(Registry& registry, double dt)
{
    (void)dt;
    for (Entity e : registry.view<Camera>()) {
        Camera& cam = registry.get_component<Camera>(e);
        cam.view = glm::lookAt(cam.position, cam.position + cam.front, cam.up);
        cam.proj = glm::perspective(glm::radians(60.0f), m_aspect, 0.01f, 500.0f);
        cam.proj[1][1] *= -1.0f; // Vulkan Y-down flip
        KNG_DEBUG("Camera pos = ({}, {}, {}), front = ({}, {}, {})",
                  cam.position.x, cam.position.y, cam.position.z,
                  cam.front.x, cam.front.y, cam.front.z);
    }
}

} // namespace kenga
