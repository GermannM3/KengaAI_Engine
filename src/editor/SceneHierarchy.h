/**
 * @file SceneHierarchy.h
 * @brief Scene hierarchy panel — lists entities, create/delete
 *
 * PROJECT_RULES.md. Фаза 6: Editor.
 */

#pragma once

#include "ecs/Entity.h"

namespace kenga {

class Registry;

namespace editor {

/// Draw the "Scene Hierarchy" ImGui window. Updates selected in-place.
void draw_scene_hierarchy(Registry& registry, Entity& selected);

} // namespace editor
} // namespace kenga
