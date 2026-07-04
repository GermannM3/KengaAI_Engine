/**
 * @file EditorSystem.cpp
 * @brief Editor system — delegates to SceneHierarchy and PropertyEditor
 *
 * PROJECT_RULES.md. Фаза 6: Editor.
 */

#include "editor/EditorSystem.h"
#include "editor/AssetBrowser.h"
#include "editor/PropertyEditor.h"
#include "editor/SceneHierarchy.h"
#include "ecs/Registry.h"
#include "ecs/Components.h"
#include "serialization/SceneSerializer.h"
#include "serialization/PrefabManager.h"
#include "core/LoggerMacros.h"

#include <glm/glm.hpp>
#include <imgui.h>

#include <algorithm>
#include <string>

namespace kenga {

EditorSystem::EditorSystem() = default;
EditorSystem::~EditorSystem() = default;

void EditorSystem::fixed_update(Registry& /*registry*/, double /*dt*/)
{
    // Editor has no fixed-step logic
}

void EditorSystem::variable_update(Registry& /*registry*/, double /*dt*/)
{
    // Rendering-side updates handled in draw_ui()
}

void EditorSystem::set_prefab_manager(PrefabManager* pm)
{
    m_prefab_manager = pm;
    if (m_asset_browser) {
        m_asset_browser->set_prefab_manager(pm);
    }
}

void EditorSystem::set_physics_system(PhysicsSystem* ps)
{
    m_physics = ps;
    if (m_asset_browser) {
        m_asset_browser->set_physics_system(ps);
    }
}

AssetBrowser* EditorSystem::get_asset_browser()
{
    if (!m_asset_browser) {
        m_asset_browser = std::make_unique<AssetBrowser>();
        m_asset_browser->set_prefab_manager(m_prefab_manager);
        m_asset_browser->set_physics_system(m_physics);
    }
    return m_asset_browser.get();
}

void EditorSystem::handle_asset_drop(Registry& registry, const std::string& path)
{
    // Determine file extension
    std::string ext;
    {
        auto dot = path.rfind('.');
        if (dot != std::string::npos) {
            ext = path.substr(dot);
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }
    }

    if (ext == ".json" && m_prefab_manager) {
        // Try to instantiate as prefab
        // Extract prefab name from filename
        std::string name;
        {
            auto slash = path.rfind('/');
            if (slash == std::string::npos) slash = path.rfind('\\');
            std::string filename = (slash != std::string::npos) ? path.substr(slash + 1) : path;
            auto dot_pos = filename.rfind('.');
            name = (dot_pos != std::string::npos) ? filename.substr(0, dot_pos) : filename;
        }

        // Load prefab if not already loaded
        if (!m_prefab_manager->has_prefab(name)) {
            m_prefab_manager->load_prefab(name, path);
        }

        Entity e = m_prefab_manager->instantiate(name, registry,
                                                  glm::vec3(0.0f, 5.0f, 0.0f), m_physics);
        if (e != INVALID_ENTITY) {
            m_selected = e;
            KNG_INFO("Spawned prefab '{}' from drag&drop", name);
        }
    } else if (ext == ".gltf" || ext == ".glb") {
        // Create a new entity with basic components (cube placeholder)
        Entity e = registry.create_entity();
        registry.add_component<Position>(e, Position{0.0f, 5.0f, 0.0f});
        registry.add_component<Scale>(e, Scale{1.0f, 1.0f, 1.0f});
        // TODO: load actual glTF mesh when asset pipeline supports it
        m_selected = e;
        KNG_INFO("Created entity from glTF drag&drop: {}", path);
    } else if (ext == ".hdr") {
        KNG_INFO("HDR skybox dropped: {} (switch skybox not yet implemented)", path);
    } else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") {
        KNG_INFO("Audio asset dropped: {} (audio emitter not yet implemented)", path);
    } else {
        KNG_WARN("Unknown asset type dropped: {}", path);
    }
}

void EditorSystem::draw_ui(Registry& registry)
{
    if (!m_show_editor) return;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float hierarchy_w = 250.0f;
    const float properties_w = 300.0f;
    const float toolbar_h = 40.0f;

    // Toolbar — top center
    const float toolbar_actual_h = 70.0f;
    ImGui::SetNextWindowPos(ImVec2(hierarchy_w, 0.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(display.x - hierarchy_w - properties_w, toolbar_actual_h), ImGuiCond_Once);
    ImGui::Begin("Editor Toolbar", nullptr, ImGuiWindowFlags_NoCollapse);
    {
        // Play/Pause
        if (m_play_mode) {
            if (ImGui::Button("Pause")) {
                m_play_mode = false;
            }
        } else {
            if (ImGui::Button("Play")) {
                m_play_mode = true;
            }
        }
        ImGui::SameLine();
        ImGui::Text(m_play_mode ? "PLAYING" : "EDITING");

        // Save / Load
        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();
        if (ImGui::Button("Save Scene")) {
            SceneSerializer::save_scene(registry, "scene.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Scene")) {
            SceneSerializer::load_scene(registry, "scene.json", m_physics);
            m_selected = INVALID_ENTITY;
        }

        // Prefab spawning
        if (m_prefab_manager) {
            ImGui::SameLine();
            ImGui::Text("|");
            ImGui::SameLine();

            const auto names = m_prefab_manager->prefab_names();
            if (!names.empty()) {
                static int current_prefab = 0;
                if (current_prefab >= static_cast<int>(names.size())) {
                    current_prefab = 0;
                }

                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::BeginCombo("##Prefab", names[current_prefab].c_str())) {
                    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                        const bool selected = (i == current_prefab);
                        if (ImGui::Selectable(names[i].c_str(), selected)) {
                            current_prefab = i;
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::Button("Spawn")) {
                    Entity e = m_prefab_manager->instantiate(
                        names[current_prefab], registry,
                        glm::vec3(0.0f, 5.0f, 0.0f), m_physics);
                    if (e != INVALID_ENTITY) {
                        m_selected = e;
                    }
                }
            }
        }

        // Make Prefab from selected entity
        if (m_prefab_manager && m_selected != INVALID_ENTITY && registry.is_valid(m_selected)) {
            ImGui::SameLine();
            if (ImGui::Button("Make Prefab")) {
                ImGui::OpenPopup("MakePrefabPopup");
            }
            if (ImGui::BeginPopup("MakePrefabPopup")) {
                static char prefab_name[64] = "MyPrefab";
                ImGui::InputText("Name", prefab_name, sizeof(prefab_name));
                if (ImGui::Button("Save")) {
                    std::string path = std::string("assets/prefabs/") + prefab_name + ".json";
                    m_prefab_manager->save_prefab(registry, m_selected, prefab_name, path);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }
    ImGui::End();

    // Scene hierarchy — left side
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(hierarchy_w, display.y * 0.6f), ImGuiCond_Once);
    editor::draw_scene_hierarchy(registry, m_selected);

    // Property editor — right side
    ImGui::SetNextWindowPos(ImVec2(display.x - properties_w, 0.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(properties_w, display.y), ImGuiCond_Once);
    editor::draw_property_editor(registry, m_selected);

    // Asset browser — bottom-left, below hierarchy
    {
        const float browser_h = display.y * 0.4f;
        ImGui::SetNextWindowPos(ImVec2(0.0f, display.y - browser_h), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(hierarchy_w + (display.x - hierarchy_w - properties_w) * 0.5f,
                                        browser_h), ImGuiCond_Once);
        get_asset_browser()->draw(registry);
    }

    // Drop zone — covers the viewport area (between hierarchy and properties, below toolbar)
    // Uses an invisible full-area button that accepts drag&drop
    {
        const float vp_x = hierarchy_w;
        const float vp_y = toolbar_actual_h;
        const float vp_w = display.x - hierarchy_w - properties_w;
        const float vp_h = display.y - toolbar_actual_h;

        if (vp_w > 0 && vp_h > 0) {
            ImGui::SetNextWindowPos(ImVec2(vp_x, vp_y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(vp_w, vp_h), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
            ImGui::Begin("##DropZone", nullptr,
                         ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoFocusOnAppearing);

            // Invisible button to receive drops
            ImGui::InvisibleButton("##DropArea", ImVec2(vp_w, vp_h));
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    const std::string path(static_cast<const char*>(payload->Data));
                    handle_asset_drop(registry, path);
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
    }
}

} // namespace kenga
