/**
 * @file PrefabManager.cpp
 * @brief Prefab management implementation
 *
 * PROJECT_RULES.md. Фаза 7: Asset management + save/load.
 */

#include "serialization/PrefabManager.h"
#include "serialization/JsonHelpers.h"
#include "ecs/Components.h"
#include "ecs/Registry.h"
#include "physics/PhysicsSystem.h"
#include "core/LoggerMacros.h"

#include <nlohmann/json.hpp>

#include <fstream>

using json = nlohmann::json;

namespace kenga {

// ---------- serialize single entity to JSON ----------

static json entity_to_json(const Registry& registry, Entity e)
{
    json obj;

    if (registry.has_component<Position>(e)) {
        const auto& p = registry.get_component<Position>(e);
        obj["Position"] = vec3_to_json(p.x, p.y, p.z);
    }
    if (registry.has_component<Rotation>(e)) {
        const auto& r = registry.get_component<Rotation>(e);
        obj["Rotation"] = {{"angle", r.angle}, {"speed", r.speed}};
    }
    if (registry.has_component<Scale>(e)) {
        const auto& s = registry.get_component<Scale>(e);
        obj["Scale"] = vec3_to_json(s.x, s.y, s.z);
    }
    if (registry.has_component<Orientation>(e)) {
        const auto& o = registry.get_component<Orientation>(e);
        obj["Orientation"] = quat_to_json(o.q);
    }
    if (registry.has_component<RigidBody>(e)) {
        const auto& rb = registry.get_component<RigidBody>(e);
        obj["RigidBody"] = {{"mass", rb.mass}, {"is_static", rb.is_static}};
    }
    if (registry.has_component<Light>(e)) {
        const auto& l = registry.get_component<Light>(e);
        obj["Light"] = {
            {"type", static_cast<int>(l.type)},
            {"color", vec3_to_json(l.color)},
            {"intensity", l.intensity},
            {"direction", vec3_to_json(l.direction)},
            {"radius", l.radius},
            {"inner_cutoff", l.inner_cutoff},
            {"outer_cutoff", l.outer_cutoff}
        };
    }
    if (registry.has_component<ParticleEmitter>(e)) {
        const auto& em = registry.get_component<ParticleEmitter>(e);
        obj["ParticleEmitter"] = {
            {"position_offset", vec3_to_json(em.position_offset)},
            {"emit_rate", em.emit_rate},
            {"lifetime_min", em.lifetime_min},
            {"lifetime_max", em.lifetime_max},
            {"velocity_min", vec3_to_json(em.velocity_min)},
            {"velocity_max", vec3_to_json(em.velocity_max)},
            {"color_start", vec4_to_json(em.color_start)},
            {"color_end", vec4_to_json(em.color_end)},
            {"size_start", em.size_start},
            {"size_end", em.size_end},
            {"loop", em.loop},
            {"active", em.active},
            {"type", static_cast<int>(em.type)},
            {"use_gpu", em.use_gpu},
            {"gravity_scale", em.gravity_scale}
        };
    }

    return obj;
}

// ---------- instantiate entity from JSON ----------

static void apply_json_to_entity(Registry& registry, Entity e, const json& obj,
                                 const glm::vec3& position, PhysicsSystem* physics)
{
    // Position: override with spawn position
    if (obj.contains("Position")) {
        registry.add_component<Position>(e, Position{position.x, position.y, position.z});
    }
    if (obj.contains("Rotation")) {
        const auto& j = obj["Rotation"];
        registry.add_component<Rotation>(e, Rotation{j["angle"], j["speed"]});
    }
    if (obj.contains("Scale")) {
        const auto& j = obj["Scale"];
        registry.add_component<Scale>(e, Scale{j[0], j[1], j[2]});
    }
    if (obj.contains("Orientation")) {
        Orientation o;
        o.q = json_to_quat(obj["Orientation"]);
        registry.add_component<Orientation>(e, std::move(o));
    }
    if (obj.contains("RigidBody")) {
        const auto& j = obj["RigidBody"];
        float mass = j["mass"];
        bool is_static = j["is_static"];
        if (physics) {
            physics->create_rigid_body(registry, e, mass, is_static, position);
        } else {
            RigidBody rb;
            rb.mass = mass;
            rb.is_static = is_static;
            registry.add_component<RigidBody>(e, std::move(rb));
        }
    }
    if (obj.contains("Light")) {
        const auto& j = obj["Light"];
        Light l;
        l.type = static_cast<LightType>(j["type"].get<int>());
        l.color = json_to_vec3(j["color"]);
        l.intensity = j["intensity"];
        l.direction = json_to_vec3(j["direction"]);
        l.radius = j["radius"];
        l.inner_cutoff = j["inner_cutoff"];
        l.outer_cutoff = j["outer_cutoff"];
        registry.add_component<Light>(e, std::move(l));
    }
    if (obj.contains("ParticleEmitter")) {
        const auto& j = obj["ParticleEmitter"];
        ParticleEmitter em;
        em.position_offset = json_to_vec3(j["position_offset"]);
        em.emit_rate = j["emit_rate"];
        em.lifetime_min = j["lifetime_min"];
        em.lifetime_max = j["lifetime_max"];
        em.velocity_min = json_to_vec3(j["velocity_min"]);
        em.velocity_max = json_to_vec3(j["velocity_max"]);
        em.color_start = json_to_vec4(j["color_start"]);
        em.color_end = json_to_vec4(j["color_end"]);
        em.size_start = j["size_start"];
        em.size_end = j["size_end"];
        em.loop = j["loop"];
        em.active = j["active"];
        if (j.contains("type"))
            em.type = static_cast<ParticleType>(j["type"].get<int>());
        if (j.contains("use_gpu"))
            em.use_gpu = j["use_gpu"];
        if (j.contains("gravity_scale"))
            em.gravity_scale = j["gravity_scale"];
        registry.add_component<ParticleEmitter>(e, std::move(em));
    }
}

// ---------- public API ----------

bool PrefabManager::load_prefab(const std::string& name, const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        KNG_WARN("Prefab file not found: {} ({})", name, path);
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Validate JSON
    try {
        json::parse(content);
    } catch (const json::parse_error& e) {
        KNG_ERROR("Invalid prefab JSON {}: {}", path, e.what());
        return false;
    }

    m_prefabs[name] = std::move(content);
    KNG_INFO("Loaded prefab '{}' from {}", name, path);
    return true;
}

bool PrefabManager::save_prefab(const Registry& registry, Entity e,
                                const std::string& name, const std::string& path)
{
    json obj = entity_to_json(registry, e);
    obj["prefab_name"] = name;

    std::ofstream file(path);
    if (!file.is_open()) {
        KNG_ERROR("Failed to save prefab to {}", path);
        return false;
    }

    file << obj.dump(2);
    file.close();

    // Also store in memory
    m_prefabs[name] = obj.dump();
    KNG_INFO("Saved prefab '{}' to {}", name, path);
    return true;
}

Entity PrefabManager::instantiate(const std::string& name, Registry& registry,
                                  const glm::vec3& position, PhysicsSystem* physics)
{
    auto it = m_prefabs.find(name);
    if (it == m_prefabs.end()) {
        KNG_WARN("Prefab '{}' not found", name);
        return INVALID_ENTITY;
    }

    json obj = json::parse(it->second);
    Entity e = registry.create_entity();
    apply_json_to_entity(registry, e, obj, position, physics);

    KNG_INFO("Instantiated prefab '{}' as entity {} at ({:.1f}, {:.1f}, {:.1f})",
             name, static_cast<unsigned>(e), position.x, position.y, position.z);
    return e;
}

std::vector<std::string> PrefabManager::prefab_names() const
{
    std::vector<std::string> names;
    names.reserve(m_prefabs.size());
    for (const auto& [name, _] : m_prefabs) {
        names.push_back(name);
    }
    return names;
}

bool PrefabManager::has_prefab(const std::string& name) const
{
    return m_prefabs.count(name) > 0;
}

} // namespace kenga
