/**
 * @file LuaSystem.cpp
 * @brief Lua scripting system implementation (sol2)
 *
 * PROJECT_RULES.md. Phase 8: Scripting + gameplay.
 */

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "scripting/LuaSystem.h"
#include "ecs/Registry.h"
#include "ecs/Components.h"
#include "ecs/Entity.h"

#include <btBulletDynamicsCommon.h>
#include "rendering/GltfMesh.h"
#include "core/LoggerMacros.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace kenga {

// ---------------------------------------------------------------------------
// pimpl — hides sol::state from the header
// ---------------------------------------------------------------------------

struct LuaSystem::Impl {
    sol::state lua;
    Registry* registry = nullptr;
};

// ---------------------------------------------------------------------------
// ctor / dtor
// ---------------------------------------------------------------------------

LuaSystem::LuaSystem()
    : m_impl(std::make_unique<Impl>())
{
}

LuaSystem::~LuaSystem() = default;

// ---------------------------------------------------------------------------
// init — open libs, register engine types
// ---------------------------------------------------------------------------

void LuaSystem::init(Registry& registry)
{
    m_impl->registry = &registry;
    sol::state& lua = m_impl->lua;

    // Standard libs (no io/os for sandboxing)
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                       sol::lib::table, sol::lib::package);

    // ---- glm::vec3 ----
    lua.new_usertype<glm::vec3>("vec3",
        sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
        "x", &glm::vec3::x,
        "y", &glm::vec3::y,
        "z", &glm::vec3::z,
        sol::meta_function::addition,
            [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
        sol::meta_function::subtraction,
            [](const glm::vec3& a, const glm::vec3& b) { return a - b; },
        sol::meta_function::multiplication,
            sol::overload(
                [](const glm::vec3& a, float s) { return a * s; },
                [](float s, const glm::vec3& a) { return s * a; }
            ),
        sol::meta_function::to_string,
            [](const glm::vec3& v) {
                std::ostringstream ss;
                ss << "vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
                return ss.str();
            }
    );

    // ---- Position ----
    lua.new_usertype<Position>("Position",
        "x", &Position::x,
        "y", &Position::y,
        "z", &Position::z
    );

    // ---- Rotation ----
    lua.new_usertype<Rotation>("Rotation",
        "angle", &Rotation::angle,
        "speed", &Rotation::speed
    );

    // ---- Scale ----
    lua.new_usertype<Scale>("Scale",
        "x", &Scale::x,
        "y", &Scale::y,
        "z", &Scale::z
    );

    // ---- Light ----
    lua.new_usertype<Light>("Light",
        "intensity", &Light::intensity,
        "color", &Light::color,
        "direction", &Light::direction,
        "radius", &Light::radius
    );

    // ---- Engine API table ----
    sol::table engine = lua.create_named_table("engine");

    // engine.get_position(entity_id) -> Position or nil
    engine.set_function("get_position", [this](uint32_t eid) -> sol::object {
        Entity e = static_cast<Entity>(eid);
        if (m_impl->registry->has_component<Position>(e)) {
            return sol::make_object(m_impl->lua,
                                    &m_impl->registry->get_component<Position>(e));
        }
        return sol::nil;
    });

    // engine.get_rotation(entity_id) -> Rotation or nil
    engine.set_function("get_rotation", [this](uint32_t eid) -> sol::object {
        Entity e = static_cast<Entity>(eid);
        if (m_impl->registry->has_component<Rotation>(e)) {
            return sol::make_object(m_impl->lua,
                                    &m_impl->registry->get_component<Rotation>(e));
        }
        return sol::nil;
    });

    // engine.get_scale(entity_id) -> Scale or nil
    engine.set_function("get_scale", [this](uint32_t eid) -> sol::object {
        Entity e = static_cast<Entity>(eid);
        if (m_impl->registry->has_component<Scale>(e)) {
            return sol::make_object(m_impl->lua,
                                    &m_impl->registry->get_component<Scale>(e));
        }
        return sol::nil;
    });

    // engine.get_light(entity_id) -> Light or nil
    engine.set_function("get_light", [this](uint32_t eid) -> sol::object {
        Entity e = static_cast<Entity>(eid);
        if (m_impl->registry->has_component<Light>(e)) {
            return sol::make_object(m_impl->lua,
                                    &m_impl->registry->get_component<Light>(e));
        }
        return sol::nil;
    });

    // engine.log(message)
    engine.set_function("log", [](const std::string& msg) {
        KNG_INFO("[Lua] {}", msg);
    });

    // engine.warn(message)
    engine.set_function("warn", [](const std::string& msg) {
        KNG_WARN("[Lua] {}", msg);
    });

    // engine.time() -> seconds since start (os.clock)
    engine.set_function("time", []() -> double {
        return static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
    });

    // engine.play_animation(entity_id, clip_name) — set animation clip by name
    engine.set_function("play_animation", [this](uint32_t eid, const std::string& clip_name) {
        Entity e = static_cast<Entity>(eid);
        if (!m_impl->registry->has_component<SkinnedMesh>(e)) return;
        auto& sm = m_impl->registry->get_component<SkinnedMesh>(e);
        if (!sm.mesh) return;
        for (int i = 0; i < static_cast<int>(sm.mesh->animations.size()); ++i) {
            if (sm.mesh->animations[i].name == clip_name) {
                sm.current_clip = i;
                sm.current_time = 0.0f;
                sm.playing = true;
                return;
            }
        }
        KNG_WARN("[Lua] animation clip '{}' not found", clip_name);
    });

    // engine.set_animation_speed(entity_id, speed)
    engine.set_function("set_animation_speed", [this](uint32_t eid, float speed) {
        Entity e = static_cast<Entity>(eid);
        if (!m_impl->registry->has_component<SkinnedMesh>(e)) return;
        m_impl->registry->get_component<SkinnedMesh>(e).speed = speed;
    });

    // engine.get_player_position() -> vec3 or nil (for demo game AI)
    engine.set_function("get_player_position", [this]() -> sol::object {
        if (m_player_entity == INVALID_ENTITY) return sol::nil;
        if (!m_impl->registry->is_valid(m_player_entity)) return sol::nil;
        if (!m_impl->registry->has_component<Camera>(m_player_entity)) return sol::nil;
        const glm::vec3& pos = m_impl->registry->get_component<Camera>(m_player_entity).position;
        return sol::make_object(m_impl->lua, pos);
    });

    // engine.damage_player(amount)
    engine.set_function("damage_player", [this](float amount) {
        if (m_player_entity == INVALID_ENTITY) return;
        if (!m_impl->registry->is_valid(m_player_entity)) return;
        if (!m_impl->registry->has_component<Player>(m_player_entity)) return;
        auto& player = m_impl->registry->get_component<Player>(m_player_entity);
        player.health = (player.health > amount) ? player.health - amount : 0.0f;
    });

    // engine.set_rigid_body_velocity(entity_id, vx, vy, vz) — for AI movement
    engine.set_function("set_rigid_body_velocity", [this](uint32_t eid, float vx, float vy, float vz) {
        Entity e = static_cast<Entity>(eid);
        if (!m_impl->registry->has_component<RigidBody>(e)) return;
        const auto& rb = m_impl->registry->get_component<RigidBody>(e);
        if (!rb.body || rb.body->getInvMass() == 0.0f) return;
        rb.body->setLinearVelocity(btVector3(vx, vy, vz));
        rb.body->activate(true);
    });

    KNG_INFO("LuaSystem initialised (sol2 + Lua 5.4)");
}

// ---------------------------------------------------------------------------
// load / unload script
// ---------------------------------------------------------------------------

void LuaSystem::load_script(Registry& registry, Entity e, const std::string& path)
{
    sol::state& lua = m_impl->lua;
    const uint32_t eid = static_cast<uint32_t>(e);

    // Try several candidate paths
    std::string resolved;
    {
        const std::string candidates[] = {
            path,
            "Debug/" + path,
            "Release/" + path,
            "../" + path,
            "../../" + path,
        };
        for (const auto& c : candidates) {
            if (std::filesystem::exists(c)) {
                resolved = c;
                break;
            }
        }
    }

    if (resolved.empty()) {
        KNG_WARN("Lua script not found: {}", path);
        return;
    }

    // Read file
    std::ifstream file(resolved);
    if (!file.is_open()) {
        KNG_ERROR("Failed to open Lua script: {}", resolved);
        return;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    const std::string source = ss.str();
    file.close();

    // Execute the script in a protected call
    sol::protected_function_result result = lua.safe_script(source, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        KNG_ERROR("Lua load error ({}): {}", resolved, err.what());
        return;
    }

    // Call on_init(entity_id) if defined
    sol::protected_function on_init = lua["on_init"];
    if (on_init.valid()) {
        sol::protected_function_result init_result = on_init(eid);
        if (!init_result.valid()) {
            sol::error err = init_result;
            KNG_ERROR("Lua on_init error ({}): {}", resolved, err.what());
        }
    }

    ScriptInstance inst;
    inst.initialised = true;
    inst.loaded_path = path;
    m_instances[eid] = std::move(inst);

    // Mark component as loaded
    if (registry.has_component<Script>(e)) {
        auto& sc = registry.get_component<Script>(e);
        sc.loaded = true;
        sc.needs_reload = false;
    }

    KNG_INFO("Lua script loaded: {} (entity {})", resolved, eid);
}

void LuaSystem::unload_script(Entity e)
{
    const uint32_t eid = static_cast<uint32_t>(e);

    // Call on_destroy if defined
    sol::protected_function on_destroy = m_impl->lua["on_destroy"];
    if (on_destroy.valid()) {
        on_destroy(eid);
    }

    m_instances.erase(eid);
}

// ---------------------------------------------------------------------------
// reload
// ---------------------------------------------------------------------------

void LuaSystem::reload_script(Entity e)
{
    if (!m_impl->registry) return;
    if (!m_impl->registry->has_component<Script>(e)) return;

    unload_script(e);
    const auto& sc = m_impl->registry->get_component<Script>(e);
    load_script(*m_impl->registry, e, sc.script_path);
}

void LuaSystem::reload_all()
{
    if (!m_impl->registry) return;
    for (const Entity e : m_impl->registry->view<Script>()) {
        reload_script(e);
    }
    KNG_INFO("All Lua scripts reloaded");
}

// ---------------------------------------------------------------------------
// fixed_update — called every physics tick
// ---------------------------------------------------------------------------

void LuaSystem::fixed_update(Registry& registry, double dt)
{
    m_impl->registry = &registry;

    for (const Entity e : registry.view<Script>()) {
        auto& sc = registry.get_component<Script>(e);
        const uint32_t eid = static_cast<uint32_t>(e);

        // Load script if not yet loaded or reload requested
        auto it = m_instances.find(eid);
        if (it == m_instances.end() || sc.needs_reload) {
            if (!sc.script_path.empty()) {
                load_script(registry, e, sc.script_path);
            }
            continue; // skip update on load frame
        }

        // Call on_update(entity_id, dt)
        sol::protected_function on_update = m_impl->lua["on_update"];
        if (on_update.valid()) {
            sol::protected_function_result result = on_update(eid, dt);
            if (!result.valid()) {
                sol::error err = result;
                KNG_ERROR("Lua on_update error (entity {}): {}", eid, err.what());
                // Disable to avoid spam
                sc.loaded = false;
                m_instances.erase(eid);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// variable_update — called every render frame
// ---------------------------------------------------------------------------

void LuaSystem::variable_update(Registry& registry, double dt)
{
    m_impl->registry = &registry;

    for (const Entity e : registry.view<Script>()) {
        const auto& sc = registry.get_component<Script>(e);
        const uint32_t eid = static_cast<uint32_t>(e);

        if (!sc.loaded) continue;
        auto it = m_instances.find(eid);
        if (it == m_instances.end()) continue;

        // Call on_render(entity_id, dt) if defined
        sol::protected_function on_render = m_impl->lua["on_render"];
        if (on_render.valid()) {
            sol::protected_function_result result = on_render(eid, static_cast<float>(dt));
            if (!result.valid()) {
                sol::error err = result;
                KNG_ERROR("Lua on_render error (entity {}): {}", eid, err.what());
            }
        }
    }
}

} // namespace kenga
