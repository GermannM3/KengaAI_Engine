/**
 * @file PropertyEditor.cpp
 * @brief Property inspector — edit Position, Rotation, Scale, Light, RigidBody
 *
 * PROJECT_RULES.md. Фаза 6: Editor.
 */

#include "editor/PropertyEditor.h"
#include "ecs/Components.h"
#include "ecs/Registry.h"
#include "rendering/GltfMesh.h"
#include "core/LoggerMacros.h"

#include <btBulletDynamicsCommon.h>
#include <imgui.h>

namespace kenga::editor {

static void draw_position(Registry& registry, Entity e)
{
    if (!registry.has_component<Position>(e)) return;
    auto& pos = registry.get_component<Position>(e);
    float v[3] = {pos.x, pos.y, pos.z};
    if (ImGui::DragFloat3("Position", v, 0.1f)) {
        pos.x = v[0]; pos.y = v[1]; pos.z = v[2];
    }
}

static void draw_rotation(Registry& registry, Entity e)
{
    if (!registry.has_component<Rotation>(e)) return;
    auto& rot = registry.get_component<Rotation>(e);
    ImGui::DragFloat("Rotation Angle", &rot.angle, 1.0f, -360.0f, 360.0f);
    ImGui::DragFloat("Rotation Speed", &rot.speed, 0.1f);
}

static void draw_scale(Registry& registry, Entity e)
{
    if (!registry.has_component<Scale>(e)) return;
    auto& sc = registry.get_component<Scale>(e);
    float v[3] = {sc.x, sc.y, sc.z};
    if (ImGui::DragFloat3("Scale", v, 0.05f, 0.01f, 100.0f)) {
        sc.x = v[0]; sc.y = v[1]; sc.z = v[2];
    }
}

static void draw_orientation(Registry& registry, Entity e)
{
    if (!registry.has_component<Orientation>(e)) return;
    const auto& ori = registry.get_component<Orientation>(e);
    ImGui::Text("Orientation: (%.2f, %.2f, %.2f, %.2f)",
                ori.q.w, ori.q.x, ori.q.y, ori.q.z);
}

static void draw_rigid_body(Registry& registry, Entity e)
{
    if (!registry.has_component<RigidBody>(e)) return;
    auto& rb = registry.get_component<RigidBody>(e);

    if (ImGui::TreeNode("RigidBody")) {
        ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.0f, 1000.0f);
        ImGui::Checkbox("Static", &rb.is_static);
        ImGui::Text("Bullet body: %s", rb.body ? "active" : "none");

        if (rb.body && rb.body->getInvMass() != 0.0f) {
            static float impulse_strength = 10.0f;
            ImGui::DragFloat("Impulse Force", &impulse_strength, 0.5f, 1.0f, 200.0f);

            if (ImGui::Button("Apply Impulse (Up)")) {
                rb.body->activate(true);
                rb.body->applyCentralImpulse(btVector3(0, impulse_strength, 0));
                KNG_INFO("Editor: applied upward impulse ({}) to entity {}",
                         impulse_strength, static_cast<unsigned>(e));
            }
            ImGui::SameLine();
            if (ImGui::Button("Apply Impulse (Fwd)")) {
                rb.body->activate(true);
                rb.body->applyCentralImpulse(btVector3(0, 0, impulse_strength));
                KNG_INFO("Editor: applied forward impulse ({}) to entity {}",
                         impulse_strength, static_cast<unsigned>(e));
            }
        }

        ImGui::TreePop();
    }
}

static void draw_light(Registry& registry, Entity e)
{
    if (!registry.has_component<Light>(e)) return;
    auto& light = registry.get_component<Light>(e);

    if (ImGui::TreeNode("Light")) {
        const char* type_names[] = {"Directional", "Point", "Spot"};
        int current_type = static_cast<int>(light.type);
        if (ImGui::Combo("Type", &current_type, type_names, 3)) {
            light.type = static_cast<LightType>(current_type);
        }

        ImGui::ColorEdit3("Color", &light.color.x);
        ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);

        float dir[3] = {light.direction.x, light.direction.y, light.direction.z};
        if (ImGui::DragFloat3("Direction", dir, 0.01f, -1.0f, 1.0f)) {
            light.direction = glm::vec3(dir[0], dir[1], dir[2]);
        }

        if (light.type == LightType::point || light.type == LightType::spot) {
            ImGui::DragFloat("Radius", &light.radius, 0.5f, 0.1f, 500.0f);
        }
        if (light.type == LightType::spot) {
            ImGui::DragFloat("Inner Cutoff", &light.inner_cutoff, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Outer Cutoff", &light.outer_cutoff, 0.01f, 0.0f, 1.0f);
        }
        ImGui::TreePop();
    }
}

static void draw_camera(Registry& registry, Entity e)
{
    if (!registry.has_component<Camera>(e)) return;
    auto& cam = registry.get_component<Camera>(e);

    if (ImGui::TreeNode("Camera")) {
        float pos[3] = {cam.position.x, cam.position.y, cam.position.z};
        if (ImGui::DragFloat3("Cam Position", pos, 0.1f)) {
            cam.position = glm::vec3(pos[0], pos[1], pos[2]);
        }
        ImGui::Text("Yaw: %.1f  Pitch: %.1f", cam.yaw, cam.pitch);
        ImGui::TreePop();
    }
}

static void draw_add_component(Registry& registry, Entity e)
{
    if (ImGui::Button("Add Component...")) {
        ImGui::OpenPopup("AddComponentPopup");
    }
    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (!registry.has_component<Position>(e) && ImGui::MenuItem("Position")) {
            registry.add_component<Position>(e, Position{});
            KNG_INFO("Editor: added Position to entity {}", static_cast<unsigned>(e));
        }
        if (!registry.has_component<Rotation>(e) && ImGui::MenuItem("Rotation")) {
            registry.add_component<Rotation>(e, Rotation{});
            KNG_INFO("Editor: added Rotation to entity {}", static_cast<unsigned>(e));
        }
        if (!registry.has_component<Scale>(e) && ImGui::MenuItem("Scale")) {
            registry.add_component<Scale>(e, Scale{});
            KNG_INFO("Editor: added Scale to entity {}", static_cast<unsigned>(e));
        }
        if (!registry.has_component<Orientation>(e) && ImGui::MenuItem("Orientation")) {
            registry.add_component<Orientation>(e, Orientation{});
            KNG_INFO("Editor: added Orientation to entity {}", static_cast<unsigned>(e));
        }
        if (!registry.has_component<Light>(e) && ImGui::MenuItem("Light")) {
            registry.add_component<Light>(e, Light{});
            KNG_INFO("Editor: added Light to entity {}", static_cast<unsigned>(e));
        }
        if (!registry.has_component<ParticleEmitter>(e) && ImGui::MenuItem("ParticleEmitter")) {
            registry.add_component<ParticleEmitter>(e, ParticleEmitter{});
            KNG_INFO("Editor: added ParticleEmitter to entity {}", static_cast<unsigned>(e));
        }
        if (!registry.has_component<Script>(e) && ImGui::MenuItem("Script")) {
            registry.add_component<Script>(e, Script{});
            KNG_INFO("Editor: added Script to entity {}", static_cast<unsigned>(e));
        }
        if (!registry.has_component<SkinnedMesh>(e) && ImGui::MenuItem("SkinnedMesh")) {
            registry.add_component<SkinnedMesh>(e, SkinnedMesh{});
            KNG_INFO("Editor: added SkinnedMesh to entity {}", static_cast<unsigned>(e));
        }
        if (!registry.has_component<LodInfo>(e) && ImGui::MenuItem("LodInfo")) {
            registry.add_component<LodInfo>(e, LodInfo{});
            KNG_INFO("Editor: added LodInfo to entity {}", static_cast<unsigned>(e));
        }
        ImGui::EndPopup();
    }
}

void draw_property_editor(Registry& registry, Entity selected)
{
    ImGui::Begin("Properties");

    if (selected == INVALID_ENTITY || !registry.is_valid(selected)) {
        ImGui::TextDisabled("No entity selected");
        ImGui::End();
        return;
    }

    ImGui::Text("Entity %u", static_cast<unsigned>(selected));
    ImGui::Separator();

    draw_position(registry, selected);
    draw_rotation(registry, selected);
    draw_scale(registry, selected);
    draw_orientation(registry, selected);
    draw_rigid_body(registry, selected);
    draw_light(registry, selected);
    draw_camera(registry, selected);

    // ParticleEmitter
    if (registry.has_component<ParticleEmitter>(selected)) {
        auto& em = registry.get_component<ParticleEmitter>(selected);
        if (ImGui::TreeNode("ParticleEmitter")) {
            ImGui::Checkbox("Active", &em.active);
            ImGui::Checkbox("Loop", &em.loop);
            ImGui::DragFloat("Emit Rate", &em.emit_rate, 1.0f, 0.0f, 500.0f);
            ImGui::DragFloat("Lifetime Min", &em.lifetime_min, 0.05f, 0.01f, 10.0f);
            ImGui::DragFloat("Lifetime Max", &em.lifetime_max, 0.05f, 0.01f, 10.0f);

            float vmin[3] = {em.velocity_min.x, em.velocity_min.y, em.velocity_min.z};
            if (ImGui::DragFloat3("Vel Min", vmin, 0.1f)) {
                em.velocity_min = {vmin[0], vmin[1], vmin[2]};
            }
            float vmax[3] = {em.velocity_max.x, em.velocity_max.y, em.velocity_max.z};
            if (ImGui::DragFloat3("Vel Max", vmax, 0.1f)) {
                em.velocity_max = {vmax[0], vmax[1], vmax[2]};
            }

            ImGui::ColorEdit4("Color Start", &em.color_start.x);
            ImGui::ColorEdit4("Color End", &em.color_end.x);
            ImGui::DragFloat("Size Start", &em.size_start, 0.1f, 0.01f, 50.0f);
            ImGui::DragFloat("Size End", &em.size_end, 0.1f, 0.01f, 50.0f);

            // Particle type dropdown
            const char* type_names[] = {"Sparks", "Smoke", "Fire", "Trail", "Custom"};
            int type_idx = static_cast<int>(em.type);
            if (ImGui::Combo("Type", &type_idx, type_names, 5)) {
                em.type = static_cast<ParticleType>(type_idx);
                // Apply preset values based on type
                switch (em.type) {
                case ParticleType::sparks:
                    em.color_start = {1.0f, 0.7f, 0.1f, 1.0f};
                    em.color_end = {1.0f, 0.1f, 0.0f, 0.0f};
                    em.velocity_min = {-3.0f, 2.0f, -3.0f};
                    em.velocity_max = {3.0f, 8.0f, 3.0f};
                    em.lifetime_min = 0.3f; em.lifetime_max = 0.8f;
                    em.gravity_scale = 1.0f;
                    break;
                case ParticleType::smoke:
                    em.color_start = {0.5f, 0.5f, 0.5f, 0.6f};
                    em.color_end = {0.3f, 0.3f, 0.3f, 0.0f};
                    em.velocity_min = {-0.5f, 1.0f, -0.5f};
                    em.velocity_max = {0.5f, 3.0f, 0.5f};
                    em.lifetime_min = 1.0f; em.lifetime_max = 3.0f;
                    em.gravity_scale = -0.3f;
                    break;
                case ParticleType::fire:
                    em.color_start = {1.0f, 0.6f, 0.0f, 1.0f};
                    em.color_end = {1.0f, 0.0f, 0.0f, 0.0f};
                    em.velocity_min = {-1.0f, 2.0f, -1.0f};
                    em.velocity_max = {1.0f, 5.0f, 1.0f};
                    em.lifetime_min = 0.5f; em.lifetime_max = 1.5f;
                    em.gravity_scale = -0.5f;
                    break;
                case ParticleType::trail:
                    em.color_start = {0.3f, 0.6f, 1.0f, 0.8f};
                    em.color_end = {0.1f, 0.2f, 0.5f, 0.0f};
                    em.velocity_min = {-0.2f, -0.2f, -0.2f};
                    em.velocity_max = {0.2f, 0.2f, 0.2f};
                    em.lifetime_min = 0.5f; em.lifetime_max = 2.0f;
                    em.gravity_scale = 0.0f;
                    break;
                default: break;
                }
            }

            ImGui::Checkbox("GPU Mode", &em.use_gpu);
            ImGui::DragFloat("Gravity Scale", &em.gravity_scale, 0.05f, -2.0f, 2.0f);
            ImGui::TreePop();
        }
    }

    // SkinnedMesh (animation controls)
    if (registry.has_component<SkinnedMesh>(selected)) {
        auto& sm = registry.get_component<SkinnedMesh>(selected);
        if (ImGui::TreeNode("Skinned Mesh")) {
            if (sm.mesh && sm.mesh->has_skeleton) {
                ImGui::Text("Joints: %d", static_cast<int>(sm.mesh->skeleton.size()));
                ImGui::Text("Animations: %d", static_cast<int>(sm.mesh->animations.size()));

                // Animation clip selector
                if (!sm.mesh->animations.empty()) {
                    if (ImGui::BeginCombo("Clip", sm.mesh->animations[sm.current_clip].name.c_str())) {
                        for (int i = 0; i < static_cast<int>(sm.mesh->animations.size()); ++i) {
                            const bool is_selected = (sm.current_clip == i);
                            if (ImGui::Selectable(sm.mesh->animations[i].name.c_str(), is_selected)) {
                                sm.current_clip = i;
                                sm.current_time = 0.0f;
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    const float dur = sm.mesh->animations[sm.current_clip].duration;
                    ImGui::SliderFloat("Time", &sm.current_time, 0.0f, dur);
                }

                ImGui::DragFloat("Speed", &sm.speed, 0.05f, 0.0f, 5.0f);
                ImGui::Checkbox("Playing", &sm.playing);
                ImGui::Checkbox("Loop", &sm.loop);
            } else {
                ImGui::Text("No skeleton data");
            }
            ImGui::TreePop();
        }
    }

    // Script
    if (registry.has_component<Script>(selected)) {
        auto& sc = registry.get_component<Script>(selected);
        if (ImGui::TreeNode("Script")) {
            // Editable script path
            static char path_buf[256] = {};
            if (path_buf[0] == '\0' && !sc.script_path.empty()) {
                std::strncpy(path_buf, sc.script_path.c_str(), sizeof(path_buf) - 1);
            }
            if (ImGui::InputText("Script Path", path_buf, sizeof(path_buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                sc.script_path = path_buf;
                sc.needs_reload = true;
                sc.loaded = false;
                KNG_INFO("Editor: script path changed to '{}'", sc.script_path);
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload")) {
                sc.needs_reload = true;
                sc.loaded = false;
                KNG_INFO("Editor: reload requested for '{}'", sc.script_path);
            }
            ImGui::Text("Status: %s", sc.loaded ? "loaded" : "not loaded");
            ImGui::TreePop();
        }
    }

    // LodInfo
    if (registry.has_component<LodInfo>(selected)) {
        auto& lod = registry.get_component<LodInfo>(selected);
        if (ImGui::TreeNode("LOD")) {
            ImGui::DragFloat("LOD1 Distance", &lod.lod1_distance, 0.5f, 1.0f, 200.0f);
            ImGui::DragFloat("LOD2 Distance", &lod.lod2_distance, 0.5f, 5.0f, 500.0f);
            ImGui::Checkbox("Cull at LOD2", &lod.cull_at_lod2);
            ImGui::Text("Current LOD: %d", lod.current_lod);
            ImGui::TreePop();
        }
    }

    ImGui::Separator();
    draw_add_component(registry, selected);

    ImGui::End();
}

} // namespace kenga::editor
