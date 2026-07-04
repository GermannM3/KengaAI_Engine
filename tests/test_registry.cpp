/**
 * @file test_registry.cpp
 * @brief Unit tests for ECS Registry (create/destroy, components, view)
 */

#include "ecs/Components.h"
#include "ecs/Registry.h"
#include <gtest/gtest.h>

using namespace kenga;

TEST(Registry, CreateEntity)
{
    Registry reg;
    Entity e = reg.create_entity();
    EXPECT_NE(e, INVALID_ENTITY);
    EXPECT_TRUE(reg.is_valid(e));
}

TEST(Registry, DestroyEntity)
{
    Registry reg;
    Entity e = reg.create_entity();
    reg.destroy_entity(e);
    EXPECT_FALSE(reg.is_valid(e));
}

TEST(Registry, ReuseSlotAfterDestroy)
{
    Registry reg;
    Entity a = reg.create_entity();
    std::uint32_t index_a = entity_index(a);
    reg.destroy_entity(a);
    Entity b = reg.create_entity();
    EXPECT_EQ(entity_index(b), index_a);
    EXPECT_NE(a, b); // version differs
}

TEST(Registry, AddGetHasRemoveComponent)
{
    Registry reg;
    Entity e = reg.create_entity();
    EXPECT_FALSE(reg.has_component<Position>(e));
    reg.add_component<Position>(e, Position{1.0f, 2.0f, 3.0f});
    EXPECT_TRUE(reg.has_component<Position>(e));
    const Position& p = reg.get_component<Position>(e);
    EXPECT_FLOAT_EQ(p.x, 1.0f);
    EXPECT_FLOAT_EQ(p.y, 2.0f);
    EXPECT_FLOAT_EQ(p.z, 3.0f);
    reg.remove_component<Position>(e);
    EXPECT_FALSE(reg.has_component<Position>(e));
}

TEST(Registry, ViewEmpty)
{
    Registry reg;
    int count = 0;
    for (Entity e : reg.view<Position>()) {
        (void)e;
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST(Registry, ViewSingleComponent)
{
    Registry reg;
    reg.add_component<Position>(reg.create_entity(), Position{0, 0, 0});
    reg.add_component<Position>(reg.create_entity(), Position{1, 1, 1});
    reg.create_entity(); // no Position
    int count = 0;
    for (Entity e : reg.view<Position>()) {
        (void)e;
        ++count;
    }
    EXPECT_EQ(count, 2);
}

TEST(Registry, ViewTwoComponents)
{
    Registry reg;
    Entity a = reg.create_entity();
    reg.add_component<Position>(a, Position{0, 0, 0});
    reg.add_component<Velocity>(a, Velocity{1, 0, 0});
    reg.add_component<Position>(reg.create_entity(), Position{1, 1, 1}); // no Velocity
    int count = 0;
    for (Entity e : reg.view<Position, Velocity>()) {
        (void)e;
        ++count;
    }
    EXPECT_EQ(count, 1);
}

TEST(Registry, DestroyEntityRemovesFromPools)
{
    Registry reg;
    Entity e = reg.create_entity();
    reg.add_component<Position>(e, Position{0, 0, 0});
    reg.destroy_entity(e);
    int count = 0;
    for (Entity x : reg.view<Position>()) {
        (void)x;
        ++count;
    }
    EXPECT_EQ(count, 0);
}
