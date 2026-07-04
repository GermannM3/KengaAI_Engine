/**
 * @file InputSystem.h
 * @brief Система ввода — WASD + mouse look для камеры
 *
 * PROJECT_RULES.md. Фаза 3.4: Camera input.
 */

#pragma once

#include "ecs/Components.h"
#include "ecs/Entity.h"
#include "ecs/ISystem.h"
#include "ecs/Registry.h"

#include <functional>

namespace kenga {

class AudioSystem;         // forward declaration
class EditorSystem;        // forward declaration
class GameSystem;
class GpuParticleSystem;   // forward declaration
class ParticleSystem;      // forward declaration
class PhysicsSystem;       // forward declaration

/**
 * @brief Обновляет Camera по WASD и mouse, triggers footstep audio
 */
class InputSystem : public ISystem {
public:
    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;

    void set_camera_entity(Entity e) { m_camera_entity = e; }
    void set_audio_system(AudioSystem* audio) { m_audio = audio; }
    void set_editor_system(EditorSystem* editor) { m_editor = editor; }
    void set_physics_system(PhysicsSystem* physics) { m_physics = physics; }
    void set_particle_system(ParticleSystem* particles) { m_particles = particles; }
    void set_gpu_particle_system(GpuParticleSystem* gpu_particles) { m_gpu_particles = gpu_particles; }
    void set_game_system(GameSystem* game) { m_game_system = game; }
    void set_restart_callback(std::function<void()> cb) { m_restart_callback = std::move(cb); }
    bool get_wireframe() const { return m_wireframe; }
    bool get_ui_mode() const { return m_ui_mode; }

private:
    /// Perform a raycast from camera into the scene. In play mode: push dynamic bodies.
    /// In edit mode: select the hit entity.
    void perform_raycast(Registry& registry, const Camera& cam);

    /// Find the ECS entity that owns a given btRigidBody (linear scan).
    Entity find_entity_for_body(Registry& registry, const void* body_ptr);

    Entity m_camera_entity = INVALID_ENTITY;
    AudioSystem* m_audio = nullptr;
    EditorSystem* m_editor = nullptr;
    GameSystem* m_game_system = nullptr;
    std::function<void()> m_restart_callback;
    PhysicsSystem* m_physics = nullptr;
    ParticleSystem* m_particles = nullptr;
    GpuParticleSystem* m_gpu_particles = nullptr;
    float m_speed = 10.0f;
    float m_sensitivity = 0.1f;
    bool m_wireframe = false;
    bool m_ui_mode = false;
    bool m_tab_was_pressed = false;
    bool m_f_was_pressed = false;
    bool m_was_moving = false;
    bool m_lmb_was_pressed = false;
};

} // namespace kenga
