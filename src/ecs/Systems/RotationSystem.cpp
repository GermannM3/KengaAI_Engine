/**
 * @file RotationSystem.cpp
 * @brief Реализация RotationSystem
 */

#include "ecs/Systems/RotationSystem.h"

namespace kenga {

void RotationSystem::fixed_update(Registry& registry, double dt)
{
    for (const Entity e : registry.view<Rotation>()) {
        auto& rot = registry.get_component<Rotation>(e);
        rot.angle += static_cast<float>(rot.speed * dt);
    }
}

void RotationSystem::variable_update(Registry& registry, double dt)
{
    (void)registry;
    (void)dt;
}

} // namespace kenga
