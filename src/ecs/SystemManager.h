/**
 * @file SystemManager.h
 * @brief Менеджер систем — регистрация и вызов update
 *
 * PROJECT_RULES.md. Фаза 2.3: ECS.
 */

#pragma once

#include "ecs/ISystem.h"
#include "ecs/Registry.h"

#include <memory>
#include <typeindex>
#include <unordered_map>

namespace kenga {

/**
 * @brief Реестр систем, вызов update_fixed и update_variable
 */
class SystemManager {
public:
    template <typename T>
    void register_system(std::unique_ptr<T> system);

    template <typename T>
    T* get_system();

    void update_fixed(Registry& registry, double dt);
    void update_variable(Registry& registry, double dt);

private:
    std::unordered_map<std::type_index, std::unique_ptr<ISystem>> m_systems;
};

template <typename T>
void SystemManager::register_system(std::unique_ptr<T> system)
{
    static_assert(std::is_base_of_v<ISystem, T>, "T must inherit from ISystem");
    const std::type_index key{typeid(T)};
    m_systems.emplace(key, std::move(system));
}

template <typename T>
T* SystemManager::get_system()
{
    const std::type_index key{typeid(T)};
    auto it = m_systems.find(key);
    if (it == m_systems.end()) {
        return nullptr;
    }
    return static_cast<T*>(it->second.get());
}

} // namespace kenga
