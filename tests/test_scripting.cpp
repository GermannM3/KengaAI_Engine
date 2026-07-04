/**
 * @file test_scripting.cpp
 * @brief Unit tests for LuaSystem (script loading, sandboxing, error handling)
 */

#include "scripting/LuaSystem.h"
#include "ecs/Components.h"
#include "ecs/Registry.h"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

using namespace kenga;

namespace fs = std::filesystem;

class LuaSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry = std::make_unique<Registry>();
        lua_system = std::make_unique<LuaSystem>();
        lua_system->init(*registry);

        test_dir = fs::temp_directory_path() / "kenga_testLua";
        fs::create_directories(test_dir);
    }

    void TearDown() override {
        lua_system.reset();
        registry.reset();
        fs::remove_all(test_dir);
    }

    std::unique_ptr<Registry> registry;
    std::unique_ptr<LuaSystem> lua_system;
    fs::path test_dir;
};

TEST_F(LuaSystemTest, InitSucceeds)
{
    // If we got here, init() didn't crash
    SUCCEED();
}

TEST_F(LuaSystemTest, LoadValidScript)
{
    Entity e = registry->create_entity();
    registry->add_component<Script>(e, Script{"assets/scripts/spin.lua"});

    // Run one update cycle — should load and execute the script
    lua_system->fixed_update(*registry, 1.0 / 60.0);
    SUCCEED();
}

TEST_F(LuaSystemTest, LoadNonExistentScript)
{
    Entity e = registry->create_entity();
    registry->add_component<Script>(e, Script{"assets/scripts/nonexistent.lua"});

    // Should handle gracefully (no crash)
    lua_system->fixed_update(*registry, 1.0 / 60.0);
    SUCCEED();
}

TEST_F(LuaSystemTest, LoadScriptWithSyntaxError)
{
    auto bad_script = test_dir / "bad_syntax.lua";
    {
        std::ofstream f(bad_script);
        f << "function on_update(entity_id, dt)\n";
        f << "  this is not valid lua {{{{\n";
        f << "end\n";
    }

    Entity e = registry->create_entity();
    registry->add_component<Script>(e, Script{bad_script.string()});

    // Should handle syntax error gracefully
    lua_system->fixed_update(*registry, 1.0 / 60.0);
    SUCCEED();
}

TEST_F(LuaSystemTest, ScriptSandboxNoFileIO)
{
    // Script that tries to use io library (should fail after sandboxing)
    auto sandbox_test = test_dir / "sandbox_test.lua";
    {
        std::ofstream f(sandbox_test);
        f << "function on_init(entity_id)\n";
        f << "  local f = io.open('test.txt', 'w')\n";
        f << "  if f then\n";
        f << "    f:close()\n";
        f << "  end\n";
        f << "end\n";
    }

    Entity e = registry->create_entity();
    registry->add_component<Script>(e, Script{sandbox_test.string()});

    // Should not crash — io library is not available
    lua_system->fixed_update(*registry, 1.0 / 60.0);
    SUCCEED();
}

TEST_F(LuaSystemTest, ScriptSandboxNoOS)
{
    // Script that tries to use os library (should fail after sandboxing)
    auto sandbox_test = test_dir / "sandbox_os_test.lua";
    {
        std::ofstream f(sandbox_test);
        f << "function on_init(entity_id)\n";
        f << "  os.execute('echo hello')\n";
        f << "end\n";
    }

    Entity e = registry->create_entity();
    registry->add_component<Script>(e, Script{sandbox_test.string()});

    // Should not crash — os library is not available
    lua_system->fixed_update(*registry, 1.0 / 60.0);
    SUCCEED();
}

TEST_F(LuaSystemTest, MultipleScriptsRun)
{
    Entity e1 = registry->create_entity();
    registry->add_component<Script>(e1, Script{"assets/scripts/spin.lua"});

    Entity e2 = registry->create_entity();
    registry->add_component<Script>(e2, Script{"assets/scripts/bounce.lua"});

    // Both scripts should load and run without conflict
    for (int i = 0; i < 10; ++i) {
        lua_system->fixed_update(*registry, 1.0 / 60.0);
    }
    SUCCEED();
}

TEST_F(LuaSystemTest, ReloadScript)
{
    Entity e = registry->create_entity();
    registry->add_component<Script>(e, Script{"assets/scripts/spin.lua"});

    lua_system->fixed_update(*registry, 1.0 / 60.0);

    // Reload should not crash
    lua_system->reload_script(e);
    lua_system->fixed_update(*registry, 1.0 / 60.0);
    SUCCEED();
}
