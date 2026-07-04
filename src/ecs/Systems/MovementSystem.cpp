/**
 * @file MovementSystem.cpp
 * @brief Реализация MovementSystem
 */

#include "ecs/Systems/MovementSystem.h"
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

#include <vector>

namespace kenga {

void MovementSystem::fixed_update(Registry& registry, double dt)
{
    for (const Entity e : registry.view<Position, Velocity>()) {
        auto& pos = registry.get_component<Position>(e);
        const auto& vel = registry.get_component<Velocity>(e);

        pos.x += static_cast<float>(vel.x * dt);
        pos.y += static_cast<float>(vel.y * dt);
        pos.z += static_cast<float>(vel.z * dt);
    }
}

void MovementSystem::variable_update(Registry& registry, double dt)
{
    (void)registry;
    (void)dt;
}

} // namespace kenga
