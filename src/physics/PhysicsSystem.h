/**
 * @file PhysicsSystem.h
 * @brief Bullet3 physics integration as an ECS system
 *
 * PROJECT_RULES.md. Фаза 4.1: Physics — Bullet3.
 */

#pragma once

#include "ecs/Entity.h"
#include "ecs/ISystem.h"

#include <glm/glm.hpp>

#include <set>
#include <utility>
#include <vector>

class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;
class btCollisionObject;
class btTypedConstraint;

namespace kenga {

class AudioSystem;         // forward declaration
class EditorSystem;        // forward declaration
class ParticleSystem;      // forward declaration
class GpuParticleSystem;   // forward declaration

/**
 * @brief Wraps Bullet3 btDiscreteDynamicsWorld.
 *
 * Call init() after construction, shutdown() before destruction.
 * fixed_update() steps the simulation and syncs Position/Orientation
 * from btRigidBody transforms. Detects new collisions for audio.
 */
class PhysicsSystem : public ISystem {
public:
    PhysicsSystem() = default;
    ~PhysicsSystem() override;

    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;

    /// Create Bullet world, broadphase, solver, etc.
    void init();

    /// Destroy Bullet world and all owned resources.
    void shutdown();

    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;

    /// Direct access to the dynamics world (for adding rigid bodies from outside).
    btDiscreteDynamicsWorld* world() const { return m_world; }

    void set_audio_system(AudioSystem* audio) { m_audio = audio; }
    void set_editor_system(EditorSystem* editor) { m_editor = editor; }
    void set_particle_system(ParticleSystem* particles) { m_particles = particles; }
    void set_gpu_particle_system(GpuParticleSystem* gpu_particles) { m_gpu_particles = gpu_particles; }

    /// Create a rigid body for an entity and add it to the physics world.
    /// If is_static, mass is ignored and set to 0.
    void create_rigid_body(Registry& registry, Entity e, float mass, bool is_static,
                           const glm::vec3& position);

    /// Remove all rigid bodies from the world and clean up their resources.
    void clear_all_bodies(Registry& registry);

    /// Remove rigid body for a single entity (e.g. when entity is destroyed).
    void remove_rigid_body(Registry& registry, Entity e);

    /// Create a hinge constraint between two entities with RigidBody.
    /// Pivots and axes in world space (Bullet converts internally).
    /// Returns true on success; both entities must have a valid btRigidBody.
    bool create_hinge(Registry& registry, Entity entity_a, Entity entity_b,
                     const glm::vec3& pivot_a, const glm::vec3& pivot_b,
                     const glm::vec3& axis_a, const glm::vec3& axis_b);

private:
    void remove_constraints_involving(btCollisionObject* body);

    using CollisionPair = std::pair<const btCollisionObject*, const btCollisionObject*>;

    btBroadphaseInterface* m_broadphase = nullptr;
    btDefaultCollisionConfiguration* m_collision_config = nullptr;
    btCollisionDispatcher* m_dispatcher = nullptr;
    btSequentialImpulseConstraintSolver* m_solver = nullptr;
    btDiscreteDynamicsWorld* m_world = nullptr;
    AudioSystem* m_audio = nullptr;
    EditorSystem* m_editor = nullptr;
    ParticleSystem* m_particles = nullptr;
    GpuParticleSystem* m_gpu_particles = nullptr;
    std::set<CollisionPair> m_active_collisions;
    std::vector<btTypedConstraint*> m_constraints;
    int m_tick_count = 0;
    double m_collision_cooldown = 0.0; ///< global cooldown to avoid audio spam
};

} // namespace kenga
