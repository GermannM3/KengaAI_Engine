/**
 * @file ISystem.h
 * @brief Интерфейс системы ECS
 *
 * PROJECT_RULES.md. Фаза 2.3: ECS — SystemManager.
 */

#pragma once

namespace kenga {

class Registry;

/**
 * @brief Базовый интерфейс для всех систем
 */
class ISystem {
public:
    virtual ~ISystem() = default;

    virtual void fixed_update(Registry& registry, double dt) = 0;
    virtual void variable_update(Registry& registry, double dt) = 0;
};

} // namespace kenga
