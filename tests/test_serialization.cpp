/**
 * @file test_serialization.cpp
 * @brief Unit tests for SceneSerializer, PrefabManager, and JsonHelpers
 */

#include "serialization/SceneSerializer.h"
#include "serialization/PrefabManager.h"
#include "serialization/JsonHelpers.h"
#include "ecs/Components.h"
#include "ecs/Registry.h"
#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <fstream>
#include <filesystem>

using namespace kenga;

namespace fs = std::filesystem;

// ========== JsonHelpers tests ==========

TEST(JsonHelpers, Vec3RoundTrip)
{
    glm::vec3 original(1.0f, 2.5f, -3.0f);
    json j = vec3_to_json(original);
    glm::vec3 result = json_to_vec3(j);
    EXPECT_FLOAT_EQ(result.x, original.x);
    EXPECT_FLOAT_EQ(result.y, original.y);
    EXPECT_FLOAT_EQ(result.z, original.z);
}

TEST(JsonHelpers, Vec4RoundTrip)
{
    glm::vec4 original(1.0f, 2.0f, 3.0f, 4.0f);
    json j = vec4_to_json(original);
    glm::vec4 result = json_to_vec4(j);
    EXPECT_FLOAT_EQ(result.x, original.x);
    EXPECT_FLOAT_EQ(result.w, original.w);
}

TEST(JsonHelpers, QuatRoundTrip)
{
    glm::quat original = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    json j = quat_to_json(original);
    glm::quat result = json_to_quat(j);
    EXPECT_FLOAT_EQ(result.w, original.w);
    EXPECT_FLOAT_EQ(result.x, original.x);
}

TEST(JsonHelpers, Vec3BoundsCheck)
{
    json too_short = json::array({1.0f, 2.0f});
    EXPECT_THROW(json_to_vec3(too_short), std::runtime_error);

    json not_array = json::object();
    EXPECT_THROW(json_to_vec3(not_array), std::runtime_error);
}

TEST(JsonHelpers, Vec4BoundsCheck)
{
    json too_short = json::array({1.0f, 2.0f, 3.0f});
    EXPECT_THROW(json_to_vec4(too_short), std::runtime_error);
}

TEST(JsonHelpers, QuatBoundsCheck)
{
    json too_short = json::array({1.0f, 0.0f});
    EXPECT_THROW(json_to_quat(too_short), std::runtime_error);
}

TEST(JsonHelpers, Vec3FromFloats)
{
    json j = vec3_to_json(1.0f, 2.0f, 3.0f);
    EXPECT_EQ(j.size(), 3u);
    EXPECT_FLOAT_EQ(j[0].get<float>(), 1.0f);
    EXPECT_FLOAT_EQ(j[2].get<float>(), 3.0f);
}

// ========== SceneSerializer tests ==========

class SceneSerializerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "kenga_testSerialization";
        fs::create_directories(test_dir);
        scene_path = (test_dir / "test_scene.json").string();
    }

    void TearDown() override {
        fs::remove_all(test_dir);
    }

    fs::path test_dir;
    std::string scene_path;
};

TEST_F(SceneSerializerTest, SaveAndLoadEmptyScene)
{
    Registry reg;
    EXPECT_TRUE(SceneSerializer::save_scene(reg, scene_path));
    EXPECT_TRUE(fs::exists(scene_path));

    Registry reg2;
    EXPECT_TRUE(SceneSerializer::load_scene(reg2, scene_path));
}

TEST_F(SceneSerializerTest, SaveAndLoadEntityWithPosition)
{
    Registry reg;
    Entity e = reg.create_entity();
    reg.add_component<Position>(e, Position{1.0f, 2.0f, 3.0f});

    EXPECT_TRUE(SceneSerializer::save_scene(reg, scene_path));

    Registry reg2;
    EXPECT_TRUE(SceneSerializer::load_scene(reg2, scene_path));

    // Should have at least one entity with Position
    bool found = false;
    for (Entity e2 : reg2.view<Position>()) {
        const auto& p = reg2.get_component<Position>(e2);
        if (p.x == 1.0f && p.y == 2.0f && p.z == 3.0f) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SceneSerializerTest, SaveAndLoadMultipleComponents)
{
    Registry reg;
    Entity e = reg.create_entity();
    reg.add_component<Position>(e, Position{10.0f, 0.0f, -5.0f});
    reg.add_component<Scale>(e, Scale{2.0f, 2.0f, 2.0f});
    reg.add_component<Rotation>(e, Rotation{1.5f, 0.5f});

    EXPECT_TRUE(SceneSerializer::save_scene(reg, scene_path));

    Registry reg2;
    EXPECT_TRUE(SceneSerializer::load_scene(reg2, scene_path));

    bool found_pos = false;
    bool found_scale = false;
    bool found_rot = false;
    for (Entity e2 : reg2.view<Position, Scale, Rotation>()) {
        const auto& p = reg2.get_component<Position>(e2);
        const auto& s = reg2.get_component<Scale>(e2);
        const auto& r = reg2.get_component<Rotation>(e2);
        if (p.x == 10.0f && s.x == 2.0f && r.angle == 1.5f) {
            found_pos = true;
            found_scale = true;
            found_rot = true;
            break;
        }
    }
    EXPECT_TRUE(found_pos);
    EXPECT_TRUE(found_scale);
    EXPECT_TRUE(found_rot);
}

TEST_F(SceneSerializerTest, LoadNonExistentFile)
{
    Registry reg;
    EXPECT_FALSE(SceneSerializer::load_scene(reg, "/nonexistent/path.json"));
}

TEST_F(SceneSerializerTest, LoadCorruptedJson)
{
    std::ofstream f(scene_path);
    f << "{ invalid json content }}}";
    f.close();

    Registry reg;
    // Should handle gracefully (return false or skip bad data)
    SceneSerializer::load_scene(reg, scene_path);
    // No crash = pass
}

// ========== PrefabManager tests ==========

class PrefabManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "kenga_testPrefab";
        fs::create_directories(test_dir);
        prefab_path = (test_dir / "test_prefab.json").string();
    }

    void TearDown() override {
        fs::remove_all(test_dir);
    }

    fs::path test_dir;
    std::string prefab_path;
};

TEST_F(PrefabManagerTest, SaveAndLoadPrefab)
{
    Registry reg;
    Entity e = reg.create_entity();
    reg.add_component<Position>(e, Position{5.0f, 1.0f, 0.0f});
    reg.add_component<Scale>(e, Scale{1.0f, 1.0f, 1.0f});

    PrefabManager pm;
    EXPECT_TRUE(pm.save_prefab(reg, e, "cube", prefab_path));
    EXPECT_TRUE(pm.has_prefab("cube"));

    auto names = pm.prefab_names();
    EXPECT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "cube");
}

TEST_F(PrefabManagerTest, InstantiatePrefab)
{
    Registry reg;
    Entity e = reg.create_entity();
    reg.add_component<Position>(e, Position{0.0f, 0.0f, 0.0f});

    PrefabManager pm;
    pm.save_prefab(reg, e, "box", prefab_path);

    // Instantiate at a new position
    Entity e2 = pm.instantiate("box", reg, glm::vec3(10.0f, 5.0f, 0.0f));
    EXPECT_NE(e2, INVALID_ENTITY);
    EXPECT_TRUE(reg.has_component<Position>(e2));

    const auto& p = reg.get_component<Position>(e2);
    EXPECT_FLOAT_EQ(p.x, 10.0f);
    EXPECT_FLOAT_EQ(p.y, 5.0f);
}

TEST_F(PrefabManagerTest, InstantiateNonExistentPrefab)
{
    Registry reg;
    PrefabManager pm;
    Entity e = pm.instantiate("nonexistent", reg, glm::vec3(0.0f));
    EXPECT_EQ(e, INVALID_ENTITY);
}

TEST_F(PrefabManagerTest, LoadPrefabFromFile)
{
    // Create a prefab file manually
    {
        Registry reg;
        Entity e = reg.create_entity();
        reg.add_component<Position>(e, Position{1.0f, 2.0f, 3.0f});
        PrefabManager pm;
        pm.save_prefab(reg, e, "light", prefab_path);
    }

    // Load it in a new PrefabManager
    PrefabManager pm2;
    EXPECT_TRUE(pm2.load_prefab("light", prefab_path));
    EXPECT_TRUE(pm2.has_prefab("light"));
}
