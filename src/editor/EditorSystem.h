/**
 * @file EditorSystem.h
 * @brief In-engine editor system (hierarchy, properties, play/pause)
 *
 * PROJECT_RULES.md. Фаза 6: Editor.
 */

#pragma once

#include "ecs/Entity.h"
#include "ecs/ISystem.h"

#include <memory>
#include <string>

namespace kenga {

class AssetBrowser;
class PrefabManager;
class PhysicsSystem;
class Registry;

/**
 * @brief Editor overlay — scene hierarchy, property inspector, play/pause.
 *
 * When editor mode is active and play mode is off, physics and gameplay
 * systems should skip their updates (checked via is_play_mode()).
 */
class EditorSystem : public ISystem {
public:
    EditorSystem();
    ~EditorSystem() override;

    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;

    /// Draw the full editor UI (call inside ImGui frame).
    void draw_ui(Registry& registry);

    bool is_editor_visible() const { return m_show_editor; }
    void set_editor_visible(bool v) { m_show_editor = v; }

    bool is_play_mode() const { return m_play_mode; }
    void set_play_mode(bool v) { m_play_mode = v; }

    Entity get_selected_entity() const { return m_selected; }
    void set_selected_entity(Entity e) { m_selected = e; }

    void set_prefab_manager(PrefabManager* pm);
    void set_physics_system(PhysicsSystem* ps);

    /// Access the asset browser (created lazily on first call)
    AssetBrowser* get_asset_browser();

private:
    /// Handle an asset path dropped into the scene
    void handle_asset_drop(Registry& registry, const std::string& path);

    bool m_show_editor = true;
    bool m_play_mode = false;
    Entity m_selected = INVALID_ENTITY;
    PrefabManager* m_prefab_manager = nullptr;
    PhysicsSystem* m_physics = nullptr;
    std::unique_ptr<AssetBrowser> m_asset_browser;
};

} // namespace kenga
