/**
 * @file Components.h
 * @brief Базовые компоненты для тестов и демо
 *
 * PROJECT_RULES.md. Фаза 2.2: ECS, 3.3: Camera, 4.1: Physics.
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <string>
#include <vector>

class btRigidBody;
class btCollisionShape;

namespace kenga {

struct Camera {
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);
    glm::vec3 position = glm::vec3(0.0f, 0.0f, -3.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = 0.0f;
    float pitch = 0.0f;
};

struct Position {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Velocity {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Rotation {
    float angle = 0.0f;
    float speed = 0.0f;
};

struct Scale {
    float x = 1.0f;
    float y = 1.0f;
    float z = 1.0f;
};

/// @brief Orientation as quaternion (for physics-driven rotation)
struct Orientation {
    glm::quat q = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); ///< identity quaternion
};

/// @brief Rigid body component (Bullet3 physics)
struct RigidBody {
    btRigidBody* body = nullptr;
    btCollisionShape* shape = nullptr;
    float mass = 1.0f;
    bool is_static = false;
};

/// @brief Light types for multi-light rendering
enum class LightType : int {
    directional = 0,
    point = 1,
    spot = 2,
};

/// @brief Light component for ECS
struct Light {
    LightType type = LightType::directional;
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    float radius = 50.0f;         ///< attenuation radius (point/spot)
    float inner_cutoff = 0.9f;    ///< cos(inner angle) for spot
    float outer_cutoff = 0.8f;    ///< cos(outer angle) for spot
};

/// @brief Particle effect type presets
enum class ParticleType : int {
    sparks = 0,
    smoke = 1,
    fire = 2,
    trail = 3,
    custom = 4,
};

/// @brief Particle emitter component
struct ParticleEmitter {
    glm::vec3 position_offset = {0.0f, 0.0f, 0.0f};
    float emit_rate = 50.0f;           ///< particles per second
    float lifetime_min = 0.5f;
    float lifetime_max = 2.0f;
    glm::vec3 velocity_min = {-1.0f, 1.0f, -1.0f};
    glm::vec3 velocity_max = {1.0f, 5.0f, 1.0f};
    glm::vec4 color_start = {1.0f, 0.8f, 0.2f, 1.0f};  ///< orange/yellow
    glm::vec4 color_end = {1.0f, 0.2f, 0.0f, 0.0f};     ///< red, fade out
    float size_start = 4.0f;           ///< screen pixels
    float size_end = 1.0f;
    bool loop = true;
    bool active = true;
    ParticleType type = ParticleType::sparks;
    bool use_gpu = true;               ///< use GPU particle system when available
    float gravity_scale = 1.0f;        ///< multiplier for gravity (0 = no gravity)
};

// Forward-declare GltfMesh to avoid heavy include in header
struct GltfMesh;

/// @brief Skinned mesh component — holds shared mesh data and per-instance animation state
struct SkinnedMesh {
    std::shared_ptr<GltfMesh> mesh;                  ///< shared mesh (skeleton, clips, geometry)
    std::vector<glm::mat4> joint_matrices;           ///< current-frame final joint matrices
    int current_clip = 0;                            ///< index into mesh->animations
    float current_time = 0.0f;                       ///< playback position (seconds)
    float speed = 1.0f;                              ///< playback speed multiplier
    bool playing = true;
    bool loop = true;
};

/// @brief LOD (Level of Detail) component — controls mesh detail by distance
struct LodInfo {
    float lod1_distance = 20.0f;   ///< switch to LOD1 beyond this distance
    float lod2_distance = 50.0f;   ///< switch to LOD2 (or cull) beyond this
    int current_lod = 0;           ///< 0 = full, 1 = medium, 2 = low/culled
    bool cull_at_lod2 = true;      ///< if true, don't draw at LOD2 distance
};

/// @brief Player component — attached to camera entity in FPS demo
struct Player {
    float health = 100.0f;
    float max_health = 100.0f;
    int ammo = 30;
    int max_ammo = 30;
};

/// @brief Enemy component — for demo game; health and damage
struct Enemy {
    float health = 50.0f;
    float damage = 10.0f;
    float attack_range = 2.0f;
    float chase_range = 15.0f;
    float attack_cooldown = 0.0f; ///< seconds until next attack
};

/// @brief Lua script component — attaches a script file to an entity
struct Script {
    std::string script_path;           ///< path to .lua file (relative to working dir)
    bool loaded = false;               ///< true after first successful load
    bool needs_reload = false;         ///< set to true to force reload next frame
};

} // namespace kenga
