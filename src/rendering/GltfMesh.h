/**
 * @file GltfMesh.h
 * @brief glTF mesh data and loader (geometry + skeleton + animations)
 *
 * PROJECT_RULES.md. Phase 9: Animation system.
 */

#pragma once

#include "rendering/PbrVertex.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

namespace kenga {

/// @brief Single joint in a skeleton hierarchy
struct JointInfo {
    int parent = -1;                         ///< parent index (-1 for root)
    glm::mat4 inverse_bind_matrix{1.0f};     ///< inverse bind pose
    glm::mat4 local_transform{1.0f};         ///< rest-pose local transform
    std::string name;
};

/// @brief A single keyframe value (vec4 stores T/S as xyz or quaternion as xyzw)
struct AnimKeyframe {
    float time = 0.0f;
    glm::vec4 value{0.0f};
};

/// @brief One animation channel targeting a specific joint property
struct AnimChannel {
    int joint_index = 0;
    enum class Type { translation, rotation, scale } type = Type::translation;
    std::vector<AnimKeyframe> keyframes;
};

/// @brief A named animation clip with duration and channels
struct AnimationClip {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimChannel> channels;
};

/// @brief glTF mesh data: geometry, skeleton, and animation clips
struct GltfMesh {
    std::vector<PbrVertex> vertices;
    std::vector<uint32_t> indices;

    // Skeleton data (populated when glTF has skins)
    std::vector<JointInfo> skeleton;
    std::vector<AnimationClip> animations;
    bool has_skeleton = false;

    /// Load geometry + skeleton + animations from a glTF/glb file
    bool load_from_file(const char* path);

    /// Procedural primitives (no skeleton)
    void create_cube();
    void create_plane(float size_x, float size_z);
};

} // namespace kenga
