/**
 * @file PropertyEditor.h
 * @brief Property inspector panel — edit components of selected entity
 *
 * PROJECT_RULES.md. Фаза 6: Editor.
 */

#pragma once

#include "ecs/Entity.h"

namespace kenga {

class Registry;

namespace editor {

/// Draw the "Properties" ImGui window for the selected entity.
void draw_property_editor(Registry& registry, Entity selected);

} // namespace editor
} // namespace kenga
