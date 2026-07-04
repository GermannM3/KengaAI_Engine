/**
 * @file AnimationSystem.cpp
 * @brief Skeletal animation — keyframe sampling, hierarchy walk, joint matrix computation
 *
 * PROJECT_RULES.md. Phase 9: Animation system.
 */

#include "animation/AnimationSystem.h"
#include "ecs/Registry.h"
#include "ecs/Components.h"
#include "rendering/GltfMesh.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>

namespace kenga {

// ---------------------------------------------------------------------------
// helpers: keyframe interpolation
// ---------------------------------------------------------------------------

/// Find the two keyframes surrounding `time` and return interpolation factor
static std::pair<size_t, float> find_keyframes(const std::vector<AnimKeyframe>& kfs, float time)
{
    if (kfs.empty()) return {0, 0.0f};
    if (kfs.size() == 1 || time <= kfs.front().time) return {0, 0.0f};
    if (time >= kfs.back().time) return {kfs.size() - 1, 0.0f};

    for (size_t i = 0; i + 1 < kfs.size(); ++i) {
        if (time < kfs[i + 1].time) {
            const float span = kfs[i + 1].time - kfs[i].time;
            const float t = (span > 0.0f) ? (time - kfs[i].time) / span : 0.0f;
            return {i, t};
        }
    }
    return {kfs.size() - 1, 0.0f};
}

static glm::vec3 lerp_vec3(const glm::vec4& a, const glm::vec4& b, float t)
{
    return glm::mix(glm::vec3(a), glm::vec3(b), t);
}

static glm::quat lerp_quat(const glm::vec4& a, const glm::vec4& b, float t)
{
    // glTF stores quaternions as (x, y, z, w)
    glm::quat qa(a.w, a.x, a.y, a.z);
    glm::quat qb(b.w, b.x, b.y, b.z);
    return glm::slerp(qa, qb, t);
}

// ---------------------------------------------------------------------------
// fixed_update — advance animation time and compute joint matrices
// ---------------------------------------------------------------------------

void AnimationSystem::fixed_update(Registry& registry, double dt)
{
    for (const Entity e : registry.view<SkinnedMesh>()) {
        auto& sm = registry.get_component<SkinnedMesh>(e);
        if (!sm.mesh || !sm.mesh->has_skeleton || sm.mesh->animations.empty()) continue;
        if (!sm.playing) continue;

        // Clamp clip index
        if (sm.current_clip < 0 || sm.current_clip >= static_cast<int>(sm.mesh->animations.size()))
            sm.current_clip = 0;

        const auto& clip = sm.mesh->animations[sm.current_clip];
        const auto& skel = sm.mesh->skeleton;
        const int joint_count = static_cast<int>(skel.size());

        // Advance time
        sm.current_time += static_cast<float>(dt) * sm.speed;
        if (clip.duration > 0.0f) {
            if (sm.loop) {
                while (sm.current_time > clip.duration)
                    sm.current_time -= clip.duration;
                while (sm.current_time < 0.0f)
                    sm.current_time += clip.duration;
            } else {
                sm.current_time = std::clamp(sm.current_time, 0.0f, clip.duration);
            }
        }

        // Start with rest-pose local transforms
        std::vector<glm::vec3> local_t(joint_count);
        std::vector<glm::quat> local_r(joint_count);
        std::vector<glm::vec3> local_s(joint_count, glm::vec3(1.0f));

        // Decompose rest-pose local transforms (approximate — assumes TRS order)
        for (int j = 0; j < joint_count; ++j) {
            const glm::mat4& m = skel[j].local_transform;
            local_t[j] = glm::vec3(m[3]);
            // Extract scale from column lengths
            local_s[j] = glm::vec3(
                glm::length(glm::vec3(m[0])),
                glm::length(glm::vec3(m[1])),
                glm::length(glm::vec3(m[2])));
            // Extract rotation (remove scale)
            glm::mat3 rot_mat(
                glm::vec3(m[0]) / local_s[j].x,
                glm::vec3(m[1]) / local_s[j].y,
                glm::vec3(m[2]) / local_s[j].z);
            local_r[j] = glm::quat_cast(rot_mat);
        }

        // Apply animation channels (overwrite sampled joints)
        for (const auto& ch : clip.channels) {
            if (ch.joint_index < 0 || ch.joint_index >= joint_count) continue;
            if (ch.keyframes.empty()) continue;

            auto [idx, t] = find_keyframes(ch.keyframes, sm.current_time);

            switch (ch.type) {
            case AnimChannel::Type::translation: {
                if (idx + 1 < ch.keyframes.size()) {
                    local_t[ch.joint_index] = lerp_vec3(ch.keyframes[idx].value,
                                                         ch.keyframes[idx + 1].value, t);
                } else {
                    local_t[ch.joint_index] = glm::vec3(ch.keyframes[idx].value);
                }
                break;
            }
            case AnimChannel::Type::rotation: {
                if (idx + 1 < ch.keyframes.size()) {
                    local_r[ch.joint_index] = lerp_quat(ch.keyframes[idx].value,
                                                         ch.keyframes[idx + 1].value, t);
                } else {
                    const auto& v = ch.keyframes[idx].value;
                    local_r[ch.joint_index] = glm::quat(v.w, v.x, v.y, v.z);
                }
                break;
            }
            case AnimChannel::Type::scale: {
                if (idx + 1 < ch.keyframes.size()) {
                    local_s[ch.joint_index] = lerp_vec3(ch.keyframes[idx].value,
                                                         ch.keyframes[idx + 1].value, t);
                } else {
                    local_s[ch.joint_index] = glm::vec3(ch.keyframes[idx].value);
                }
                break;
            }
            }
        }

        // Compute global transforms (walk hierarchy root-first)
        std::vector<glm::mat4> global(joint_count, glm::mat4(1.0f));
        for (int j = 0; j < joint_count; ++j) {
            glm::mat4 local = glm::translate(glm::mat4(1.0f), local_t[j])
                            * glm::mat4_cast(local_r[j])
                            * glm::scale(glm::mat4(1.0f), local_s[j]);
            if (skel[j].parent >= 0) {
                global[j] = global[skel[j].parent] * local;
            } else {
                global[j] = local;
            }
        }

        // Final joint matrices = global * inverse_bind
        sm.joint_matrices.resize(joint_count);
        for (int j = 0; j < joint_count; ++j) {
            sm.joint_matrices[j] = global[j] * skel[j].inverse_bind_matrix;
        }
    }
}

void AnimationSystem::variable_update(Registry& /*registry*/, double /*dt*/)
{
    // Animation is updated in fixed_update for determinism
}

} // namespace kenga
