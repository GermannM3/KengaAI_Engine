/**
 * @file MovementSystem.h
 * @brief Система движения: position += velocity * dt
 *
 * PROJECT_RULES.md. Фаза 2.2: ECS.
 */

#pragma once

#include "ecs/Components.h"
#include "ecs/ISystem.h"
#include "ecs/Registry.h"

namespace kenga {

class MovementSystem : public ISystem {
public:
    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;
};

} // namespace kenga
