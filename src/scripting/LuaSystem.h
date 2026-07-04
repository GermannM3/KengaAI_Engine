/**
 * @file LuaSystem.h
 * @brief Lua scripting system for entity behaviour
 *
 * PROJECT_RULES.md. Phase 8: Scripting + gameplay.
 *
 * Each entity with a Script component gets its own Lua environment.
 * The script file must define on_update(entity_id, dt) at minimum.
 * Optional: on_init(entity_id), on_destroy(entity_id).
 */

#pragma once

#include "ecs/Entity.h"
#include "ecs/ISystem.h"

#include <memory>
#include <string>
#include <unordered_map>

// Forward-declare sol types to keep the header lightweight
struct lua_State;

namespace kenga {

class Registry;

/// @brief Per-entity script instance (internal bookkeeping)
struct ScriptInstance {
    bool initialised = false;
    std::string loaded_path;  ///< path that was loaded (to detect changes)
};

/// @brief ECS system that runs Lua scripts attached to entities
class LuaSystem : public ISystem {
public:
    LuaSystem();
    ~LuaSystem() override;

    /// Initialise the Lua VM and register engine bindings
    void init(Registry& registry);

    void fixed_update(Registry& registry, double dt) override;
    void variable_update(Registry& registry, double dt) override;

    /// Force-reload a specific entity's script
    void reload_script(Entity e);

    /// Reload every loaded script (e.g. after hot-reload key)
    void reload_all();

    /// Set the player entity (camera) for demo game — used by engine.get_player_position(), engine.damage_player()
    void set_player_entity(Entity e) { m_player_entity = e; }

private:
    /// Load (or reload) the script file for an entity
    void load_script(Registry& registry, Entity e, const std::string& path);

    /// Remove a script instance
    void unload_script(Entity e);

    struct Impl;                       ///< pimpl to hide sol2 from the header
    std::unique_ptr<Impl> m_impl;

    Entity m_player_entity = INVALID_ENTITY;
    std::unordered_map<uint32_t, ScriptInstance> m_instances;
};

} // namespace kenga
