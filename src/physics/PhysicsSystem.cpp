/**
 * @file PhysicsSystem.cpp
 * @brief Bullet3 physics world management and ECS sync
 *
 * PROJECT_RULES.md. Фаза 4.1: Physics — Bullet3.
 */

#include "physics/PhysicsSystem.h"
#include "audio/AudioSystem.h"
#include "editor/EditorSystem.h"
#include "particles/ParticleSystem.h"
#include "particles/GpuParticleSystem.h"
#include "core/LogManager.h"
#include "core/LoggerMacros.h"
#include "ecs/Components.h"
#include "ecs/Registry.h"

#include <btBulletDynamicsCommon.h>

#include <glm/gtc/quaternion.hpp>

namespace kenga {

PhysicsSystem::~PhysicsSystem()
{
    shutdown();
}

void PhysicsSystem::init()
{
    m_broadphase = new btDbvtBroadphase();
    m_collision_config = new btDefaultCollisionConfiguration();
    m_dispatcher = new btCollisionDispatcher(m_collision_config);
    m_solver = new btSequentialImpulseConstraintSolver();
    m_world = new btDiscreteDynamicsWorld(m_dispatcher, m_broadphase, m_solver, m_collision_config);
    m_world->setGravity(btVector3(0, -9.81f, 0));

    KNG_INFO("PhysicsSystem initialized (Bullet3, gravity = -9.81)");
}

void PhysicsSystem::shutdown()
{
    if (!m_world) {
        return;
    }

    // Remove all constraints first (they reference the bodies)
    for (btTypedConstraint* c : m_constraints) {
        m_world->removeConstraint(c);
        delete c;
    }
    m_constraints.clear();

    // Remove and delete all rigid bodies and their motion states
    for (int i = m_world->getNumCollisionObjects() - 1; i >= 0; --i) {
        btCollisionObject* obj = m_world->getCollisionObjectArray()[i];
        btRigidBody* body = btRigidBody::upcast(obj);
        if (body && body->getMotionState()) {
            delete body->getMotionState();
        }
        m_world->removeCollisionObject(obj);
        delete obj;
    }

    delete m_world;
    m_world = nullptr;
    delete m_solver;
    m_solver = nullptr;
    delete m_dispatcher;
    m_dispatcher = nullptr;
    delete m_collision_config;
    m_collision_config = nullptr;
    delete m_broadphase;
    m_broadphase = nullptr;

    KNG_INFO("PhysicsSystem shutdown");
}

void PhysicsSystem::fixed_update(Registry& registry, double dt)
{
    if (!m_world) {
        return;
    }

    // Pause physics in editor mode (not playing)
    if (m_editor && m_editor->is_editor_visible() && !m_editor->is_play_mode()) {
        return;
    }

    m_world->stepSimulation(static_cast<btScalar>(dt), 10);

    // Sync Bullet transforms -> ECS Position + Orientation
    int idx = 0;
    for (const Entity e : registry.view<RigidBody>()) {
        const auto& rb = registry.get_component<RigidBody>(e);
        if (!rb.body) {
            continue;
        }

        btTransform trans;
        rb.body->getMotionState()->getWorldTransform(trans);

        if (registry.has_component<Position>(e)) {
            auto& pos = registry.get_component<Position>(e);
            const btVector3& origin = trans.getOrigin();
            pos.x = origin.x();
            pos.y = origin.y();
            pos.z = origin.z();
        }

        if (registry.has_component<Orientation>(e)) {
            auto& ori = registry.get_component<Orientation>(e);
            const btQuaternion& bq = trans.getRotation();
            ori.q = glm::quat(bq.w(), bq.x(), bq.y(), bq.z());
        }
        ++idx;
    }

    // Collision detection for audio + particles
    {
        m_collision_cooldown -= dt;
        std::set<CollisionPair> current_collisions;
        const int num_manifolds = m_dispatcher->getNumManifolds();
        for (int i = 0; i < num_manifolds; ++i) {
            const auto* manifold = m_dispatcher->getManifoldByIndexInternal(i);
            if (manifold->getNumContacts() <= 0) continue;

            const auto* obj_a = manifold->getBody0();
            const auto* obj_b = manifold->getBody1();
            auto pair = (obj_a < obj_b) ? CollisionPair{obj_a, obj_b}
                                        : CollisionPair{obj_b, obj_a};
            current_collisions.insert(pair);

            // New collision — trigger audio + particles
            if (m_active_collisions.find(pair) == m_active_collisions.end() &&
                m_collision_cooldown <= 0.0) {
                if (m_audio) {
                    m_audio->play_impact();
                }

                // Emit collision sparks at contact point
                if (manifold->getNumContacts() > 0) {
                    const btManifoldPoint& pt = manifold->getContactPoint(0);
                    const btVector3& cp = pt.getPositionWorldOnA();
                    const glm::vec3 hit_pos(cp.x(), cp.y(), cp.z());

                    // GPU particles (preferred)
                    if (m_gpu_particles) {
                        m_gpu_particles->emit_burst_typed(hit_pos, 20, 0); // sparks
                    }
                    // CPU particles (fallback)
                    else if (m_particles) {
                        m_particles->emit_burst(hit_pos, 15,
                            glm::vec4(1.0f, 0.7f, 0.1f, 1.0f),
                            glm::vec4(1.0f, 0.1f, 0.0f, 0.0f),
                            6.0f, 0.5f);
                    }
                }

                m_collision_cooldown = 0.15;
            }
        }
        m_active_collisions = std::move(current_collisions);
    }

    // Periodic logging (every ~60 ticks = ~1 second)
    ++m_tick_count;
    if (m_tick_count % 60 == 0) {
        for (const Entity e : registry.view<RigidBody>()) {
            const auto& rb = registry.get_component<RigidBody>(e);
            if (rb.is_static) continue;
            if (registry.has_component<Position>(e)) {
                const auto& pos = registry.get_component<Position>(e);
                KNG_INFO("Physics entity {} pos=({:.2f}, {:.2f}, {:.2f})",
                         static_cast<unsigned>(e), pos.x, pos.y, pos.z);
            }
        }
    }
}

void PhysicsSystem::variable_update(Registry& /*registry*/, double /*dt*/)
{
    // Physics runs in fixed_update only
}

void PhysicsSystem::create_rigid_body(Registry& registry, Entity e, float mass, bool is_static,
                                      const glm::vec3& position)
{
    if (!m_world) return;

    btCollisionShape* shape = nullptr;
    if (is_static) {
        // Static bodies use a large box (ground plane)
        shape = new btBoxShape(btVector3(50.0f, 1.0f, 50.0f));
        mass = 0.0f;
    } else {
        // Dynamic bodies use a unit cube
        shape = new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
    }

    btTransform tf;
    tf.setIdentity();
    tf.setOrigin(btVector3(position.x, position.y, position.z));

    btVector3 inertia(0, 0, 0);
    if (mass > 0.0f) {
        shape->calculateLocalInertia(mass, inertia);
    }

    btRigidBody::btRigidBodyConstructionInfo rb_info(
        mass, new btDefaultMotionState(tf), shape, inertia);
    auto* body = new btRigidBody(rb_info);
    m_world->addRigidBody(body);

    RigidBody rb;
    rb.body = body;
    rb.shape = shape;
    rb.mass = mass;
    rb.is_static = is_static;

    if (registry.has_component<RigidBody>(e)) {
        registry.get_component<RigidBody>(e) = rb;
    } else {
        registry.add_component<RigidBody>(e, std::move(rb));
    }
}

void PhysicsSystem::remove_constraints_involving(btCollisionObject* body)
{
    if (!m_world) return;
    auto it = m_constraints.begin();
    while (it != m_constraints.end()) {
        btTypedConstraint* c = *it;
        const btRigidBody* ra = &c->getRigidBodyA();
        const btRigidBody* rb = &c->getRigidBodyB();
        if (ra == body || rb == body) {
            m_world->removeConstraint(c);
            delete c;
            it = m_constraints.erase(it);
        } else {
            ++it;
        }
    }
}

void PhysicsSystem::clear_all_bodies(Registry& registry)
{
    if (!m_world) return;

    // Remove all constraints first (they reference bodies)
    for (btTypedConstraint* c : m_constraints) {
        m_world->removeConstraint(c);
        delete c;
    }
    m_constraints.clear();

    for (const Entity e : registry.view<RigidBody>()) {
        auto& rb = registry.get_component<RigidBody>(e);
        if (rb.body) {
            m_world->removeRigidBody(rb.body);
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
    m_active_collisions.clear();
}

void PhysicsSystem::remove_rigid_body(Registry& registry, Entity e)
{
    if (!m_world || !registry.has_component<RigidBody>(e)) return;

    auto& rb = registry.get_component<RigidBody>(e);
    if (rb.body) {
        remove_constraints_involving(rb.body);
        m_world->removeRigidBody(rb.body);
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

bool PhysicsSystem::create_hinge(Registry& registry, Entity entity_a, Entity entity_b,
                                const glm::vec3& pivot_a, const glm::vec3& pivot_b,
                                const glm::vec3& axis_a, const glm::vec3& axis_b)
{
    if (!m_world) return false;
    if (!registry.has_component<RigidBody>(entity_a) || !registry.has_component<RigidBody>(entity_b)) {
        return false;
    }
    btRigidBody* body_a = registry.get_component<RigidBody>(entity_a).body;
    btRigidBody* body_b = registry.get_component<RigidBody>(entity_b).body;
    if (!body_a || !body_b) return false;

    // Convert world pivot and axis to local space for each body
    btTransform wt_a = body_a->getWorldTransform();
    btTransform wt_b = body_b->getWorldTransform();
    btVector3 pivot_a_bt(pivot_a.x, pivot_a.y, pivot_a.z);
    btVector3 pivot_b_bt(pivot_b.x, pivot_b.y, pivot_b.z);
    btVector3 axis_a_bt(axis_a.x, axis_a.y, axis_a.z);
    btVector3 axis_b_bt(axis_b.x, axis_b.y, axis_b.z);

    btVector3 pivot_in_a = wt_a.inverse() * pivot_a_bt;
    btVector3 pivot_in_b = wt_b.inverse() * pivot_b_bt;
    btVector3 axis_in_a = (wt_a.getBasis().inverse() * axis_a_bt).normalized();
    btVector3 axis_in_b = (wt_b.getBasis().inverse() * axis_b_bt).normalized();

    auto* hinge = new btHingeConstraint(*body_a, *body_b,
                                         pivot_in_a, pivot_in_b,
                                         axis_in_a, axis_in_b);
    m_world->addConstraint(hinge);
    m_constraints.push_back(hinge);
    return true;
}

} // namespace kenga
