/**
 * @file SceneSerializer.h
 * @brief Save/load entire scenes to/from JSON
 *
 * PROJECT_RULES.md. Фаза 7: Asset management + save/load.
 */

#pragma once

#include "ecs/Registry.h"

#include <string>

namespace kenga {

class PhysicsSystem;

/// @brief Serializes and deserializes ECS scenes to JSON files
class SceneSerializer {
public:
    /// Save all entities and their components to a JSON file
    static bool save_scene(const Registry& registry, const std::string& path);

    /// Load entities and components from a JSON file, replacing current scene.
    /// @param physics Optional physics system to create rigid bodies for loaded entities
    static bool load_scene(Registry& registry, const std::string& path,
                           PhysicsSystem* physics = nullptr);
};

} // namespace kenga
