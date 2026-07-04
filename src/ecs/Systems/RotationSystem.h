/**
 * @file RotationSystem.h
 * @brief Система вращения: angle += speed * dt
 *
 * PROJECT_RULES.md. Фаза 2.3: ECS.
 */

#pragma once

#include "ecs/Components.h"
#include "ecs/ISystem.h"
#include "ecs/Registry.h"

namespace kenga {

class RotationSystem : public ISystem {
public:
    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;
};

} // namespace kenga
