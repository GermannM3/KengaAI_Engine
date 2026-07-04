/**
 * @file SceneSerializer.cpp
 * @brief Scene serialization implementation using nlohmann::json
 *
 * PROJECT_RULES.md. Фаза 7: Asset management + save/load.
 */

#include "serialization/SceneSerializer.h"
#include "serialization/JsonHelpers.h"
#include "ecs/Components.h"
#include "physics/PhysicsSystem.h"
#include "core/LoggerMacros.h"

#include <nlohmann/json.hpp>

#include <fstream>

using json = nlohmann::json;

namespace kenga {

// ---------- serialize entity ----------

static json serialize_entity(const Registry& registry, Entity e)
{
    json obj;
    obj["id"] = static_cast<unsigned>(e);

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
    if (registry.has_component<Camera>(e)) {
        const auto& c = registry.get_component<Camera>(e);
        obj["Camera"] = {
            {"position", vec3_to_json(c.position)},
            {"front", vec3_to_json(c.front)},
            {"yaw", c.yaw},
            {"pitch", c.pitch}
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

    if (registry.has_component<Script>(e)) {
        const auto& sc = registry.get_component<Script>(e);
        obj["Script"] = {{"script_path", sc.script_path}};
    }

    if (registry.has_component<SkinnedMesh>(e)) {
        const auto& sm = registry.get_component<SkinnedMesh>(e);
        std::string mesh_path;
        // We don't store the mesh pointer — store a path hint if available
        obj["SkinnedMesh"] = {
            {"current_clip", sm.current_clip},
            {"speed", sm.speed},
            {"playing", sm.playing},
            {"loop", sm.loop}
        };
    }

    if (registry.has_component<LodInfo>(e)) {
        const auto& lod = registry.get_component<LodInfo>(e);
        obj["LodInfo"] = {
            {"lod1_distance", lod.lod1_distance},
            {"lod2_distance", lod.lod2_distance},
            {"cull_at_lod2", lod.cull_at_lod2}
        };
    }

    return obj;
}

// ---------- deserialize entity ----------

static void deserialize_entity(Registry& registry, Entity e, const json& obj,
                               PhysicsSystem* physics)
{
    if (obj.contains("Position")) {
        const auto& j = obj["Position"];
        registry.add_component<Position>(e, Position{j[0], j[1], j[2]});
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
        const auto& j = obj["Orientation"];
        Orientation o;
        o.q = json_to_quat(j);
        registry.add_component<Orientation>(e, std::move(o));
    }
    if (obj.contains("RigidBody")) {
        const auto& j = obj["RigidBody"];
        float mass = j["mass"];
        bool is_static = j["is_static"];

        // Create physics body via PhysicsSystem if available
        if (physics) {
            glm::vec3 pos(0.0f);
            if (registry.has_component<Position>(e)) {
                const auto& p = registry.get_component<Position>(e);
                pos = {p.x, p.y, p.z};
            }
            physics->create_rigid_body(registry, e, mass, is_static, pos);
        } else {
            // Store component without physics body (will be created later)
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
    if (obj.contains("Camera")) {
        const auto& j = obj["Camera"];
        Camera c;
        c.position = json_to_vec3(j["position"]);
        c.front = json_to_vec3(j["front"]);
        c.yaw = j["yaw"];
        c.pitch = j["pitch"];
        registry.add_component<Camera>(e, std::move(c));
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
        // New fields (backward-compatible: use defaults if missing)
        if (j.contains("type"))
            em.type = static_cast<ParticleType>(j["type"].get<int>());
        if (j.contains("use_gpu"))
            em.use_gpu = j["use_gpu"];
        if (j.contains("gravity_scale"))
            em.gravity_scale = j["gravity_scale"];
        registry.add_component<ParticleEmitter>(e, std::move(em));
    }
    if (obj.contains("Script")) {
        const auto& j = obj["Script"];
        Script sc;
        sc.script_path = j["script_path"].get<std::string>();
        registry.add_component<Script>(e, std::move(sc));
    }
    if (obj.contains("LodInfo")) {
        const auto& j = obj["LodInfo"];
        LodInfo lod;
        if (j.contains("lod1_distance")) lod.lod1_distance = j["lod1_distance"];
        if (j.contains("lod2_distance")) lod.lod2_distance = j["lod2_distance"];
        if (j.contains("cull_at_lod2")) lod.cull_at_lod2 = j["cull_at_lod2"];
        registry.add_component<LodInfo>(e, std::move(lod));
    }
    if (obj.contains("SkinnedMesh")) {
        const auto& j = obj["SkinnedMesh"];
        SkinnedMesh sm;
        if (j.contains("current_clip")) sm.current_clip = j["current_clip"];
        if (j.contains("speed")) sm.speed = j["speed"];
        if (j.contains("playing")) sm.playing = j["playing"];
        if (j.contains("loop")) sm.loop = j["loop"];
        // Note: mesh data (shared_ptr) must be re-loaded separately after deserialization
        registry.add_component<SkinnedMesh>(e, std::move(sm));
    }
}

// ---------- public API ----------

bool SceneSerializer::save_scene(const Registry& registry, const std::string& path)
{
    json root;
    root["version"] = 1;
    root["engine"] = "KengaEngine";

    json entities_array = json::array();
    for (const Entity e : registry.all_entities()) {
        entities_array.push_back(serialize_entity(registry, e));
    }
    root["entities"] = entities_array;

    std::ofstream file(path);
    if (!file.is_open()) {
        KNG_ERROR("Failed to open file for saving: {}", path);
        return false;
    }

    file << root.dump(2);
    file.close();

    KNG_INFO("Scene saved to {} ({} entities)", path, entities_array.size());
    return true;
}

bool SceneSerializer::load_scene(Registry& registry, const std::string& path,
                                 PhysicsSystem* physics)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        KNG_ERROR("Failed to open scene file: {}", path);
        return false;
    }

    json root;
    try {
        file >> root;
    } catch (const json::parse_error& e) {
        KNG_ERROR("JSON parse error in {}: {}", path, e.what());
        return false;
    }
    file.close();

    if (!root.contains("entities") || !root["entities"].is_array()) {
        KNG_ERROR("Invalid scene file: missing 'entities' array");
        return false;
    }

    // Clear existing physics bodies before destroying entities
    if (physics) {
        physics->clear_all_bodies(registry);
    }

    // Destroy all existing entities
    const auto existing = registry.all_entities();
    for (const Entity e : existing) {
        registry.destroy_entity(e);
    }

    // Create new entities from JSON
    for (const auto& entity_json : root["entities"]) {
        const Entity e = registry.create_entity();
        deserialize_entity(registry, e, entity_json, physics);
    }

    KNG_INFO("Scene loaded from {} ({} entities)", path, root["entities"].size());
    return true;
}

} // namespace kenga
