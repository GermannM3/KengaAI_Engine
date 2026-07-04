/**
 * @file SystemManager.cpp
 * @brief Реализация SystemManager
 */

#include "ecs/SystemManager.h"

namespace kenga {

void SystemManager::update_fixed(Registry& registry, double dt)
{
    for (auto& [_, system] : m_systems) {
        system->fixed_update(registry, dt);
    }
}

void SystemManager::update_variable(Registry& registry, double dt)
{
    for (auto& [_, system] : m_systems) {
        system->variable_update(registry, dt);
    }
}

} // namespace kenga
