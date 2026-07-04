/**
 * @file AssetBrowser.h
 * @brief ImGui asset browser with drag&drop support
 *
 * PROJECT_RULES.md. Phase 8: Asset management + editor polish.
 */

#pragma once

#include "ecs/Registry.h"

#include <string>

namespace kenga {

class PrefabManager;
class PhysicsSystem;

/// @brief ImGui panel that shows project assets and allows drag&drop into the scene
class AssetBrowser {
public:
    /// Draw the asset browser panel.
    /// @param registry  ECS registry for spawning entities on drop
    /// @param root_path Filesystem root to browse (relative to working directory)
    void draw(Registry& registry, const std::string& root_path = "assets/");

    void set_prefab_manager(PrefabManager* pm) { m_prefab_manager = pm; }
    void set_physics_system(PhysicsSystem* ps) { m_physics = ps; }

private:
    /// Recursively draw directory tree
    void draw_directory(const std::string& dir_path);

    /// Draw a single file entry with drag source
    void draw_file_entry(const std::string& file_path, const std::string& filename);

    /// Get a short icon/label prefix based on file extension
    static const char* icon_for_extension(const std::string& ext);

    PrefabManager* m_prefab_manager = nullptr;
    PhysicsSystem* m_physics = nullptr;
    std::string m_selected_asset;
};

} // namespace kenga
