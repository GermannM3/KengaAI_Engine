/**
 * @file IComponentPool.h
 * @brief Абстрактный интерфейс пула компонентов
 *
 * PROJECT_RULES.md. Фаза 2.2: ECS.
 */

#pragma once

#include "ecs/Entity.h"

namespace kenga {

/**
 * @brief Type-erased интерфейс для удаления сущности из любого пула
 */
class IComponentPool {
public:
    virtual ~IComponentPool() = default;

    virtual void remove_entity(Entity e) = 0;
};

} // namespace kenga
