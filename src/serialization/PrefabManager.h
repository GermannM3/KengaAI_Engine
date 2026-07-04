/**
 * @file PrefabManager.h
 * @brief Prefab system — save/load entity templates as JSON
 *
 * PROJECT_RULES.md. Фаза 7: Asset management + save/load.
 */

#pragma once

#include "ecs/Registry.h"

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

namespace kenga {

class PhysicsSystem;

/// @brief Manages prefab templates that can be instantiated as entities
class PrefabManager {
public:
    /// Load a prefab from a JSON file and store it under the given name
    bool load_prefab(const std::string& name, const std::string& path);

    /// Save the selected entity as a prefab JSON file
    bool save_prefab(const Registry& registry, Entity e,
                     const std::string& name, const std::string& path);

    /// Instantiate a prefab by name, creating a new entity at the given position
    Entity instantiate(const std::string& name, Registry& registry,
                       const glm::vec3& position, PhysicsSystem* physics = nullptr);

    /// Get list of loaded prefab names
    std::vector<std::string> prefab_names() const;

    /// Check if a prefab with the given name is loaded
    bool has_prefab(const std::string& name) const;

private:
    /// Stored prefab data as raw JSON strings (parsed on instantiation)
    std::unordered_map<std::string, std::string> m_prefabs;
};

} // namespace kenga
