/**
 * @file CameraSystem.h
 * @brief Система камеры — обновление view и proj
 *
 * PROJECT_RULES.md. Фаза 3.3: Camera.
 */

#pragma once

#include "ecs/Components.h"
#include "ecs/ISystem.h"
#include "ecs/Registry.h"

namespace kenga {

class CameraSystem : public ISystem {
public:
    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;

    void set_aspect(float aspect) { m_aspect = aspect; }

private:
    float m_aspect = 1280.0f / 720.0f;
};

} // namespace kenga
