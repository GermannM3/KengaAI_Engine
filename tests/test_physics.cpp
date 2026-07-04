/**
 * @file test_physics.cpp
 * @brief Unit tests for PhysicsSystem (create/remove rigid body, clear_all_bodies)
 * RELEASE_PLAN.md M1 — тесты для PhysicsSystem.
 * LogManager::init() вызывается один раз в test_main.cpp.
 */

#include "ecs/Components.h"
#include "ecs/Registry.h"
#include "physics/PhysicsSystem.h"
#include <gtest/gtest.h>
#include <glm/glm.hpp>

using namespace kenga;

class PhysicsSystemTest : public ::testing::Test {
protected:
    void SetUp() override { physics.init(); }
    void TearDown() override { physics.shutdown(); }
    PhysicsSystem physics;
};

TEST_F(PhysicsSystemTest, CreateRigidBody)
{
    Registry reg;
    Entity e = reg.create_entity();
    reg.add_component<Position>(e, Position{0.0f, 0.0f, 0.0f});
    physics.create_rigid_body(reg, e, 1.0f, false, glm::vec3(0.0f, 0.0f, 0.0f));
    EXPECT_TRUE(reg.has_component<RigidBody>(e));
    const RigidBody& rb = reg.get_component<RigidBody>(e);
    EXPECT_NE(rb.body, nullptr);
    EXPECT_FALSE(rb.is_static);
    physics.remove_rigid_body(reg, e);
    EXPECT_TRUE(reg.has_component<RigidBody>(e));
    EXPECT_EQ(reg.get_component<RigidBody>(e).body, nullptr);
}

TEST_F(PhysicsSystemTest, RemoveRigidBody)
{
    Registry reg;
    Entity e = reg.create_entity();
    reg.add_component<Position>(e, Position{1.0f, 0.0f, 0.0f});
    physics.create_rigid_body(reg, e, 2.0f, false, glm::vec3(1.0f, 0.0f, 0.0f));
    physics.remove_rigid_body(reg, e);
    const RigidBody& rb = reg.get_component<RigidBody>(e);
    EXPECT_EQ(rb.body, nullptr);
    EXPECT_EQ(rb.shape, nullptr);
}

TEST_F(PhysicsSystemTest, CreateStaticBody)
{
    Registry reg;
    Entity e = reg.create_entity();
    reg.add_component<Position>(e, Position{0.0f, 0.0f, 0.0f});
    physics.create_rigid_body(reg, e, 10.0f, true, glm::vec3(0.0f, -1.0f, 0.0f));
    EXPECT_TRUE(reg.has_component<RigidBody>(e));
    const RigidBody& rb = reg.get_component<RigidBody>(e);
    EXPECT_NE(rb.body, nullptr);
    EXPECT_TRUE(rb.is_static);
    physics.remove_rigid_body(reg, e);
}

TEST_F(PhysicsSystemTest, ClearAllBodies)
{
    Registry reg;
    for (int i = 0; i < 3; ++i) {
        Entity e = reg.create_entity();
        reg.add_component<Position>(e, Position{static_cast<float>(i), 0.0f, 0.0f});
        physics.create_rigid_body(reg, e, 1.0f, false, glm::vec3(static_cast<float>(i), 0.0f, 0.0f));
    }
    int count = 0;
    for (Entity x : reg.view<RigidBody>()) {
        (void)x;
        ++count;
    }
    EXPECT_EQ(count, 3);
    physics.clear_all_bodies(reg);
    count = 0;
    for (Entity x : reg.view<RigidBody>()) {
        const auto& rb = reg.get_component<RigidBody>(x);
        EXPECT_EQ(rb.body, nullptr);
        ++count;
    }
    EXPECT_EQ(count, 3);
}
