/**
 * @file JsonHelpers.h
 * @brief Shared JSON serialization helpers for glm types
 */

#pragma once

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <stdexcept>

namespace kenga {

using json = nlohmann::json;

inline json vec3_to_json(float x, float y, float z)
{
    return json::array({x, y, z});
}

inline json vec3_to_json(const glm::vec3& v)
{
    return json::array({v.x, v.y, v.z});
}

inline json vec4_to_json(const glm::vec4& v)
{
    return json::array({v.x, v.y, v.z, v.w});
}

inline json quat_to_json(const glm::quat& q)
{
    return json::array({q.w, q.x, q.y, q.z});
}

inline glm::vec3 json_to_vec3(const json& j)
{
    if (!j.is_array() || j.size() < 3) {
        throw std::runtime_error("Expected JSON array of size >= 3 for vec3");
    }
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

inline glm::vec4 json_to_vec4(const json& j)
{
    if (!j.is_array() || j.size() < 4) {
        throw std::runtime_error("Expected JSON array of size >= 4 for vec4");
    }
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
}

inline glm::quat json_to_quat(const json& j)
{
    if (!j.is_array() || j.size() < 4) {
        throw std::runtime_error("Expected JSON array of size >= 4 for quat");
    }
    return glm::quat(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
}

} // namespace kenga
