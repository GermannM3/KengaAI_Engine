/**
 * @file InputSystem.cpp
 * @brief Реализация InputSystem — WASD + mouse look
 */

#include "core/InputSystem.h"
#include "audio/AudioSystem.h"
#include "game/GameSystem.h"
#include "ecs/Components.h"
#include "editor/EditorSystem.h"
#include "particles/ParticleSystem.h"
#include "particles/GpuParticleSystem.h"
#include "physics/PhysicsSystem.h"
#include "core/Input.h"
#include "core/LoggerMacros.h"

#include <btBulletDynamicsCommon.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace kenga {

void InputSystem::fixed_update(Registry& registry, double dt)
{
    (void)registry;
    (void)dt;
}

void InputSystem::variable_update(Registry& registry, double dt)
{
    if (!registry.is_valid(m_camera_entity)) {
        KNG_ERROR("Invalid camera entity!");
        return;
    }
    if (!registry.has_component<Camera>(m_camera_entity)) {
        return;
    }

    // Determine if we're in editor edit mode (no fly camera)
    const bool in_edit_mode = m_editor && m_editor->is_editor_visible() && !m_editor->is_play_mode();

    /* Tab toggle: UI mode vs fly camera (only meaningful when not in editor edit mode) */
    {
        const bool tab_down = Input::is_key_pressed(GLFW_KEY_TAB);
        if (tab_down && !m_tab_was_pressed) {
            m_ui_mode = !m_ui_mode;
            KNG_INFO("UI mode: {}", m_ui_mode ? "ON" : "OFF");
        }
        m_tab_was_pressed = tab_down;
    }

    // Cursor mode: normal when UI mode or editor edit mode, disabled for fly camera
    {
        GLFWwindow* win = Input::get_window();
        if (win) {
            const bool want_cursor = m_ui_mode || in_edit_mode;
            glfwSetInputMode(win, GLFW_CURSOR,
                             want_cursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        }
    }

    /* F toggle: wireframe */
    {
        const bool f_down = Input::is_key_pressed(GLFW_KEY_F);
        if (f_down && !m_f_was_pressed) {
            m_wireframe = !m_wireframe;
        }
        m_f_was_pressed = f_down;
    }

    Camera& cam = registry.get_component<Camera>(m_camera_entity);

    // LMB raycast (works in both play and edit modes)
    {
        const bool lmb_down = Input::is_mouse_button_pressed(GLFW_MOUSE_BUTTON_LEFT);
        if (lmb_down && !m_lmb_was_pressed && m_physics) {
            perform_raycast(registry, cam);
        }
        m_lmb_was_pressed = lmb_down;
    }

    if (Input::is_key_pressed(GLFW_KEY_R)) {
        if (m_game_system && (m_game_system->state() == GameState::won ||
                              m_game_system->state() == GameState::lost ||
                              m_game_system->state() == GameState::menu)) {
            if (m_restart_callback) {
                m_restart_callback();
            }
        } else {
            cam.position = glm::vec3(0.0f, 5.0f, -15.0f);
            cam.front = glm::normalize(glm::vec3(0.0f, -0.3f, 1.0f));
            KNG_INFO("Camera reset by R key");
        }
    }

    /* В UI mode пропускаем движение и mouse look */
    if (m_ui_mode) {
        return;
    }

    /* В editor edit mode (не play) — пропускаем fly camera */
    if (in_edit_mode) {
        return;
    }

    const float move_speed = m_speed * static_cast<float>(dt);
    const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 right = glm::normalize(glm::cross(cam.front, up));

    if (Input::is_key_pressed(GLFW_KEY_W)) {
        cam.position += cam.front * move_speed;
    }
    if (Input::is_key_pressed(GLFW_KEY_S)) {
        cam.position -= cam.front * move_speed;
    }
    if (Input::is_key_pressed(GLFW_KEY_A)) {
        cam.position -= right * move_speed;
    }
    if (Input::is_key_pressed(GLFW_KEY_D)) {
        cam.position += right * move_speed;
    }
    if (Input::is_key_pressed(GLFW_KEY_Q)) {
        cam.position += cam.up * move_speed;
    }
    if (Input::is_key_pressed(GLFW_KEY_E)) {
        cam.position -= cam.up * move_speed;
    }

    // Footstep audio: trigger on movement start
    const bool is_moving = Input::is_key_pressed(GLFW_KEY_W) ||
                           Input::is_key_pressed(GLFW_KEY_A) ||
                           Input::is_key_pressed(GLFW_KEY_S) ||
                           Input::is_key_pressed(GLFW_KEY_D) ||
                           Input::is_key_pressed(GLFW_KEY_Q) ||
                           Input::is_key_pressed(GLFW_KEY_E);
    if (is_moving && !m_was_moving && m_audio) {
        m_audio->play_step();
    }
    m_was_moving = is_moving;

    KNG_DEBUG("Camera pos: {:.2f} {:.2f} {:.2f}", cam.position.x, cam.position.y, cam.position.z);

    const double dx = Input::mouse_delta_x();
    const double dy = Input::mouse_delta_y();

    cam.yaw += static_cast<float>(dx) * m_sensitivity;
    cam.pitch += static_cast<float>(dy) * m_sensitivity;
    cam.pitch = std::clamp(cam.pitch, -89.0f, 89.0f);

    cam.front = glm::normalize(glm::vec3{
        std::cos(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch)),
        std::sin(glm::radians(cam.pitch)),
        std::sin(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch)),
    });
}

Entity InputSystem::find_entity_for_body(Registry& registry, const void* body_ptr)
{
    for (const Entity e : registry.view<RigidBody>()) {
        const auto& rb = registry.get_component<RigidBody>(e);
        if (rb.body == body_ptr) {
            return e;
        }
    }
    return INVALID_ENTITY;
}

void InputSystem::perform_raycast(Registry& registry, const Camera& cam)
{
    if (!m_physics || !m_physics->world()) return;

    // Check ammo in play mode (when camera has Player component)
    const bool has_player = registry.has_component<Player>(m_camera_entity);
    if (!m_editor || (m_editor->is_editor_visible() && !m_editor->is_play_mode())) {
        // Edit mode — no ammo check
    } else if (has_player) {
        auto& player = registry.get_component<Player>(m_camera_entity);
        if (player.ammo <= 0) return;
    }

    if (m_audio) m_audio->play_shoot();

    const glm::vec3& origin = cam.position;
    const glm::vec3& dir = cam.front;
    constexpr float ray_length = 200.0f;

    const btVector3 from(origin.x, origin.y, origin.z);
    const btVector3 to(origin.x + dir.x * ray_length,
                       origin.y + dir.y * ray_length,
                       origin.z + dir.z * ray_length);

    btCollisionWorld::ClosestRayResultCallback callback(from, to);
    m_physics->world()->rayTest(from, to, callback);

    if (!callback.hasHit()) return;

    const btVector3& hit_point = callback.m_hitPointWorld;
    const btCollisionObject* hit_obj = callback.m_collisionObject;
    btRigidBody* body = const_cast<btRigidBody*>(btRigidBody::upcast(hit_obj));
    const Entity hit_entity = find_entity_for_body(registry, body);

    const bool raycast_edit = m_editor && m_editor->is_editor_visible() && !m_editor->is_play_mode();

    if (raycast_edit) {
        // Edit mode: select the hit entity
        if (hit_entity != INVALID_ENTITY && m_editor) {
            m_editor->set_selected_entity(hit_entity);
            KNG_INFO("Raycast select: entity {} at ({:.2f}, {:.2f}, {:.2f})",
                     static_cast<unsigned>(hit_entity),
                     hit_point.x(), hit_point.y(), hit_point.z());
        }
    } else {
        // Play mode: shoot — damage enemy or push body
        const glm::vec3 hp(hit_point.x(), hit_point.y(), hit_point.z());

        if (hit_entity != INVALID_ENTITY && registry.has_component<Enemy>(hit_entity)) {
            // Hit enemy: apply damage
            auto& enemy = registry.get_component<Enemy>(hit_entity);
            constexpr float damage = 25.0f;
            enemy.health -= damage;

            if (enemy.health <= 0.0f) {
                // Kill enemy
                m_physics->remove_rigid_body(registry, hit_entity);
                registry.destroy_entity(hit_entity);
                if (m_game_system) {
                    m_game_system->add_kill();
                    m_game_system->add_score(100);
                }
                if (m_audio) m_audio->play_enemy_death();
                KNG_INFO("Enemy killed at ({:.2f}, {:.2f}, {:.2f})", hp.x, hp.y, hp.z);
            } else {
                // Apply impulse
                if (body && body->getInvMass() != 0.0f) {
                    const btVector3 impulse(dir.x * 50.0f, dir.y * 50.0f, dir.z * 50.0f);
                    const btVector3 rel_pos = hit_point - body->getCenterOfMassPosition();
                    body->activate(true);
                    body->applyImpulse(impulse, rel_pos);
                }
            }

            if (m_audio) m_audio->play_impact();
            if (m_gpu_particles) m_gpu_particles->emit_burst_typed(hp, 25, 0);
            else if (m_particles) m_particles->emit_burst(hp, 20,
                glm::vec4(1.0f, 0.8f, 0.2f, 1.0f), glm::vec4(1.0f, 0.2f, 0.0f, 0.0f), 8.0f, 0.4f);

            // Deduct ammo
            if (has_player) {
                auto& player = registry.get_component<Player>(m_camera_entity);
                player.ammo = (player.ammo > 0) ? player.ammo - 1 : 0;
            }
        } else if (body && body->getInvMass() != 0.0f) {
            // Push dynamic body
            const btVector3 impulse(dir.x * 50.0f, dir.y * 50.0f, dir.z * 50.0f);
            const btVector3 rel_pos = hit_point - body->getCenterOfMassPosition();
            body->activate(true);
            body->applyImpulse(impulse, rel_pos);
            KNG_INFO("Hit rigid body at ({:.2f}, {:.2f}, {:.2f}), impulse applied",
                     hit_point.x(), hit_point.y(), hit_point.z());

            if (m_audio) m_audio->play_impact();
            if (m_gpu_particles) m_gpu_particles->emit_burst_typed(hp, 25, 0);
            else if (m_particles) m_particles->emit_burst(hp, 20,
                glm::vec4(1.0f, 0.8f, 0.2f, 1.0f), glm::vec4(1.0f, 0.2f, 0.0f, 0.0f), 8.0f, 0.4f);

            if (has_player) {
                auto& player = registry.get_component<Player>(m_camera_entity);
                player.ammo = (player.ammo > 0) ? player.ammo - 1 : 0;
            }
        }
    }
}

} // namespace kenga
