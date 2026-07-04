/**
 * @file SceneHierarchy.cpp
 * @brief Scene hierarchy panel implementation
 *
 * PROJECT_RULES.md. Фаза 6: Editor.
 */

#include "editor/SceneHierarchy.h"
#include "ecs/Components.h"
#include "ecs/Registry.h"
#include "core/LoggerMacros.h"

#include <imgui.h>

#include <btBulletDynamicsCommon.h>

#include <string>

namespace kenga::editor {

/// Build a short label for an entity based on its components
static std::string entity_label(Registry& registry, Entity e)
{
    std::string label = "Entity " + std::to_string(static_cast<unsigned>(e));

    if (registry.has_component<Camera>(e)) {
        label += " [Camera]";
    }
    if (registry.has_component<Light>(e)) {
        const auto& light = registry.get_component<Light>(e);
        switch (light.type) {
        case LightType::directional: label += " [DirLight]"; break;
        case LightType::point:       label += " [PointLight]"; break;
        case LightType::spot:        label += " [SpotLight]"; break;
        }
    }
    if (registry.has_component<RigidBody>(e)) {
        const auto& rb = registry.get_component<RigidBody>(e);
        label += rb.is_static ? " [Static]" : " [Dynamic]";
    }
    return label;
}

void draw_scene_hierarchy(Registry& registry, Entity& selected)
{
    ImGui::Begin("Scene Hierarchy");

    const auto entities = registry.all_entities();
    for (const Entity e : entities) {
        const std::string label = entity_label(registry, e);
        const bool is_selected = (e == selected);
        if (ImGui::Selectable(label.c_str(), is_selected)) {
            selected = e;
        }
    }

    ImGui::Separator();

    // Create buttons
    if (ImGui::Button("Create Cube")) {
        const Entity e = registry.create_entity();
        registry.add_component<Position>(e, Position{0.0f, 5.0f, 0.0f});
        registry.add_component<Rotation>(e, Rotation{0.0f, 0.0f});
        registry.add_component<Orientation>(e, Orientation{});
        registry.add_component<Scale>(e, Scale{1.0f, 1.0f, 1.0f});
        selected = e;
        KNG_INFO("Editor: created cube entity {}", static_cast<unsigned>(e));
    }

    ImGui::SameLine();

    if (ImGui::Button("Create Light")) {
        const Entity e = registry.create_entity();
        registry.add_component<Position>(e, Position{0.0f, 3.0f, 0.0f});
        registry.add_component<Light>(e, Light{
            LightType::point,
            glm::vec3(1.0f, 1.0f, 1.0f),
            1.0f,
            glm::vec3(0.0f, -1.0f, 0.0f),
            15.0f, 0.9f, 0.8f
        });
        selected = e;
        KNG_INFO("Editor: created light entity {}", static_cast<unsigned>(e));
    }

    ImGui::SameLine();

    if (ImGui::Button("Delete Selected") && selected != INVALID_ENTITY) {
        // Clean up physics body if present
        if (registry.has_component<RigidBody>(selected)) {
            auto& rb = registry.get_component<RigidBody>(selected);
            if (rb.body) {
                // Caller should remove from world before destroying
                if (rb.body->getMotionState()) {
                    delete rb.body->getMotionState();
                }
                delete rb.body;
                rb.body = nullptr;
            }
            if (rb.shape) {
                delete rb.shape;
                rb.shape = nullptr;
            }
        }
        KNG_INFO("Editor: deleted entity {}", static_cast<unsigned>(selected));
        registry.destroy_entity(selected);
        selected = INVALID_ENTITY;
    }

    ImGui::Text("Entities: %zu", entities.size());
    ImGui::End();
}

} // namespace kenga::editor
