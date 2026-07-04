/**
 * @file GltfMesh.cpp
 * @brief glTF mesh loader using tinygltf
 */

#include "rendering/GltfMesh.h"
#include "core/LogManager.h"
#include "core/LoggerMacros.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <unordered_map>

namespace kenga {

// ---------------------------------------------------------------------------
// helpers: read accessor data from tinygltf buffers
// ---------------------------------------------------------------------------

static const unsigned char* accessor_data(const tinygltf::Model& model,
                                           const tinygltf::Accessor& acc,
                                           size_t& out_stride)
{
    const auto& view = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[view.buffer];
    out_stride = view.byteStride;
    if (out_stride == 0) {
        // Compute tightly-packed stride from component type + type
        int comp_size = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(acc.componentType));
        int num_comp = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(acc.type));
        out_stride = static_cast<size_t>(comp_size * num_comp);
    }
    return buf.data.data() + view.byteOffset + acc.byteOffset;
}

// ---------------------------------------------------------------------------
// glTF node local transform
// ---------------------------------------------------------------------------

static glm::mat4 node_local_transform(const tinygltf::Node& node)
{
    if (!node.matrix.empty()) {
        // Column-major 4x4
        glm::mat4 m;
        for (int i = 0; i < 16; ++i)
            reinterpret_cast<float*>(&m)[i] = static_cast<float>(node.matrix[i]);
        return m;
    }
    glm::mat4 m{1.0f};
    if (!node.translation.empty()) {
        m = glm::translate(m, glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2])));
    }
    if (!node.rotation.empty()) {
        glm::quat q(static_cast<float>(node.rotation[3]),
                     static_cast<float>(node.rotation[0]),
                     static_cast<float>(node.rotation[1]),
                     static_cast<float>(node.rotation[2]));
        m = m * glm::mat4_cast(q);
    }
    if (!node.scale.empty()) {
        m = glm::scale(m, glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2])));
    }
    return m;
}

// ---------------------------------------------------------------------------
// load_from_file
// ---------------------------------------------------------------------------

bool GltfMesh::load_from_file(const char* path)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;

    // Detect binary vs ASCII
    const std::string path_str(path);
    bool ret = false;
    if (path_str.size() >= 4 && path_str.substr(path_str.size() - 4) == ".glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, path_str);
    } else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, path_str);
    }

    if (!warn.empty()) KNG_WARN("glTF warning: {}", warn);
    if (!err.empty())  KNG_ERROR("glTF error: {}", err);
    if (!ret) { KNG_ERROR("Failed to load glTF: {}", path); return false; }
    if (model.meshes.empty()) { KNG_ERROR("glTF has no meshes: {}", path); return false; }

    vertices.clear();
    indices.clear();
    skeleton.clear();
    animations.clear();
    has_skeleton = false;

    // ---- Build joint-node-index -> skeleton-index map (if skins exist) ----
    std::unordered_map<int, int> node_to_joint; // glTF node index -> skeleton joint index
    if (!model.skins.empty()) {
        const auto& skin = model.skins[0];
        const int joint_count = static_cast<int>(skin.joints.size());
        skeleton.resize(joint_count);

        // Inverse bind matrices
        if (skin.inverseBindMatrices >= 0) {
            const auto& acc = model.accessors[skin.inverseBindMatrices];
            size_t stride = 0;
            const auto* data = accessor_data(model, acc, stride);
            for (int j = 0; j < joint_count; ++j) {
                const float* m = reinterpret_cast<const float*>(data + j * stride);
                glm::mat4 ibm;
                for (int k = 0; k < 16; ++k) reinterpret_cast<float*>(&ibm)[k] = m[k];
                skeleton[j].inverse_bind_matrix = ibm;
            }
        }

        // Build node -> joint mapping and extract local transforms
        for (int j = 0; j < joint_count; ++j) {
            const int node_idx = skin.joints[j];
            node_to_joint[node_idx] = j;
            skeleton[j].name = model.nodes[node_idx].name;
            skeleton[j].local_transform = node_local_transform(model.nodes[node_idx]);
        }

        // Determine parent indices by walking glTF node children
        for (int j = 0; j < joint_count; ++j) {
            skeleton[j].parent = -1; // default: root
        }
        for (int j = 0; j < joint_count; ++j) {
            const int node_idx = skin.joints[j];
            for (int child_node : model.nodes[node_idx].children) {
                auto it = node_to_joint.find(child_node);
                if (it != node_to_joint.end()) {
                    skeleton[it->second].parent = j;
                }
            }
        }

        has_skeleton = true;
        KNG_INFO("glTF skeleton: {} joints", joint_count);
    }

    // ---- Parse mesh geometry ----
    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) continue;

            auto it_pos = primitive.attributes.find("POSITION");
            if (it_pos == primitive.attributes.end()) continue;

            const auto& pos_acc = model.accessors[it_pos->second];
            size_t pos_stride = 0;
            const auto* pos_data = accessor_data(model, pos_acc, pos_stride);
            const size_t vertex_count = pos_acc.count;

            // Optional accessors
            const tinygltf::Accessor* norm_acc = nullptr;
            const tinygltf::Accessor* uv_acc = nullptr;
            const tinygltf::Accessor* joints_acc = nullptr;
            const tinygltf::Accessor* weights_acc = nullptr;

            auto it_norm = primitive.attributes.find("NORMAL");
            auto it_uv = primitive.attributes.find("TEXCOORD_0");
            auto it_joints = primitive.attributes.find("JOINTS_0");
            auto it_weights = primitive.attributes.find("WEIGHTS_0");

            if (it_norm != primitive.attributes.end())
                norm_acc = &model.accessors[it_norm->second];
            if (it_uv != primitive.attributes.end())
                uv_acc = &model.accessors[it_uv->second];
            if (it_joints != primitive.attributes.end())
                joints_acc = &model.accessors[it_joints->second];
            if (it_weights != primitive.attributes.end())
                weights_acc = &model.accessors[it_weights->second];

            size_t norm_stride = 0, uv_stride = 0, j_stride = 0, w_stride = 0;
            const unsigned char* norm_data = norm_acc ? accessor_data(model, *norm_acc, norm_stride) : nullptr;
            const unsigned char* uv_data = uv_acc ? accessor_data(model, *uv_acc, uv_stride) : nullptr;
            const unsigned char* joints_data = joints_acc ? accessor_data(model, *joints_acc, j_stride) : nullptr;
            const unsigned char* weights_data = weights_acc ? accessor_data(model, *weights_acc, w_stride) : nullptr;

            vertices.reserve(vertices.size() + vertex_count);

            for (size_t i = 0; i < vertex_count; ++i) {
                PbrVertex v;
                const float* p = reinterpret_cast<const float*>(pos_data + i * pos_stride);
                v.pos = {p[0], p[1], p[2]};

                if (norm_data) {
                    const float* n = reinterpret_cast<const float*>(norm_data + i * norm_stride);
                    v.normal = {n[0], n[1], n[2]};
                } else {
                    v.normal = {0.0f, 1.0f, 0.0f};
                }

                if (uv_data) {
                    const float* t = reinterpret_cast<const float*>(uv_data + i * uv_stride);
                    v.uv = {t[0], t[1]};
                } else {
                    v.uv = {0.0f, 0.0f};
                }

                // Joint indices (can be unsigned byte or unsigned short)
                if (joints_data && joints_acc) {
                    if (joints_acc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        const uint8_t* jd = joints_data + i * j_stride;
                        v.joints = {jd[0], jd[1], jd[2], jd[3]};
                    } else if (joints_acc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const uint16_t* jd = reinterpret_cast<const uint16_t*>(joints_data + i * j_stride);
                        v.joints = {jd[0], jd[1], jd[2], jd[3]};
                    }
                } else {
                    v.joints = {0, 0, 0, 0};
                }

                // Joint weights (always float)
                if (weights_data) {
                    const float* wd = reinterpret_cast<const float*>(weights_data + i * w_stride);
                    v.weights = {wd[0], wd[1], wd[2], wd[3]};
                } else {
                    v.weights = {1.0f, 0.0f, 0.0f, 0.0f};
                }

                vertices.push_back(v);
            }

            // Indices
            if (primitive.indices >= 0) {
                const auto& idx_acc = model.accessors[primitive.indices];
                size_t idx_stride = 0;
                const auto* idx_data = accessor_data(model, idx_acc, idx_stride);
                const size_t base_vertex = vertices.size() - vertex_count;

                for (size_t i = 0; i < idx_acc.count; ++i) {
                    uint32_t idx = 0;
                    if (idx_acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        idx = *reinterpret_cast<const uint16_t*>(idx_data + i * idx_stride);
                    } else if (idx_acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        idx = *reinterpret_cast<const uint32_t*>(idx_data + i * idx_stride);
                    } else if (idx_acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        idx = *(idx_data + i * idx_stride);
                    }
                    indices.push_back(static_cast<uint32_t>(base_vertex + idx));
                }
            } else {
                const size_t base = vertices.size() - vertex_count;
                for (size_t i = 0; i < vertex_count; ++i)
                    indices.push_back(static_cast<uint32_t>(base + i));
            }
        }
    }

    // ---- Parse animations ----
    for (const auto& anim : model.animations) {
        AnimationClip clip;
        clip.name = anim.name.empty() ? "default" : anim.name;
        clip.duration = 0.0f;

        for (const auto& channel : anim.channels) {
            if (channel.target_node < 0) continue;

            // Map glTF node index to skeleton joint index
            auto it = node_to_joint.find(channel.target_node);
            if (it == node_to_joint.end()) continue;

            AnimChannel ch;
            ch.joint_index = it->second;

            if (channel.target_path == "translation")
                ch.type = AnimChannel::Type::translation;
            else if (channel.target_path == "rotation")
                ch.type = AnimChannel::Type::rotation;
            else if (channel.target_path == "scale")
                ch.type = AnimChannel::Type::scale;
            else
                continue;

            const auto& sampler = anim.samplers[channel.sampler];
            const auto& input_acc = model.accessors[sampler.input];
            const auto& output_acc = model.accessors[sampler.output];

            size_t in_stride = 0, out_stride = 0;
            const auto* in_data = accessor_data(model, input_acc, in_stride);
            const auto* out_data = accessor_data(model, output_acc, out_stride);

            for (size_t k = 0; k < input_acc.count; ++k) {
                AnimKeyframe kf;
                kf.time = *reinterpret_cast<const float*>(in_data + k * in_stride);
                const float* val = reinterpret_cast<const float*>(out_data + k * out_stride);

                if (ch.type == AnimChannel::Type::rotation) {
                    // glTF quaternion: x, y, z, w
                    kf.value = {val[0], val[1], val[2], val[3]};
                } else {
                    // translation or scale: x, y, z
                    kf.value = {val[0], val[1], val[2], 0.0f};
                }

                if (kf.time > clip.duration) clip.duration = kf.time;
                ch.keyframes.push_back(kf);
            }

            clip.channels.push_back(std::move(ch));
        }

        if (!clip.channels.empty()) {
            KNG_INFO("glTF animation '{}': {:.2f}s, {} channels",
                     clip.name, clip.duration, clip.channels.size());
            animations.push_back(std::move(clip));
        }
    }

    KNG_INFO("Loaded glTF: {} verts, {} indices, skeleton={}, anims={}",
             vertices.size(), indices.size(), has_skeleton, animations.size());
    return true;
}

void GltfMesh::create_cube()
{
    const float s = 0.5f;
    // Each face: 2 triangles, 6 vertices with UV mapping [0,1]x[0,1]
    vertices = {
        // -X face
        {{-s, -s, -s}, {-1, 0, 0}, {0, 1}}, {{-s, -s,  s}, {-1, 0, 0}, {1, 1}}, {{-s,  s,  s}, {-1, 0, 0}, {1, 0}},
        {{-s, -s, -s}, {-1, 0, 0}, {0, 1}}, {{-s,  s,  s}, {-1, 0, 0}, {1, 0}}, {{-s,  s, -s}, {-1, 0, 0}, {0, 0}},
        // +X face
        {{ s, -s,  s}, { 1, 0, 0}, {0, 1}}, {{ s, -s, -s}, { 1, 0, 0}, {1, 1}}, {{ s,  s, -s}, { 1, 0, 0}, {1, 0}},
        {{ s, -s,  s}, { 1, 0, 0}, {0, 1}}, {{ s,  s, -s}, { 1, 0, 0}, {1, 0}}, {{ s,  s,  s}, { 1, 0, 0}, {0, 0}},
        // -Y face
        {{-s, -s, -s}, {0, -1, 0}, {0, 0}}, {{ s, -s, -s}, {0, -1, 0}, {1, 0}}, {{ s, -s,  s}, {0, -1, 0}, {1, 1}},
        {{-s, -s, -s}, {0, -1, 0}, {0, 0}}, {{ s, -s,  s}, {0, -1, 0}, {1, 1}}, {{-s, -s,  s}, {0, -1, 0}, {0, 1}},
        // +Y face
        {{-s,  s,  s}, {0,  1, 0}, {0, 1}}, {{ s,  s,  s}, {0,  1, 0}, {1, 1}}, {{ s,  s, -s}, {0,  1, 0}, {1, 0}},
        {{-s,  s,  s}, {0,  1, 0}, {0, 1}}, {{ s,  s, -s}, {0,  1, 0}, {1, 0}}, {{-s,  s, -s}, {0,  1, 0}, {0, 0}},
        // +Z face
        {{-s, -s,  s}, {0, 0,  1}, {0, 1}}, {{ s, -s,  s}, {0, 0,  1}, {1, 1}}, {{ s,  s,  s}, {0, 0,  1}, {1, 0}},
        {{-s, -s,  s}, {0, 0,  1}, {0, 1}}, {{ s,  s,  s}, {0, 0,  1}, {1, 0}}, {{-s,  s,  s}, {0, 0,  1}, {0, 0}},
        // -Z face
        {{ s, -s, -s}, {0, 0, -1}, {0, 1}}, {{-s, -s, -s}, {0, 0, -1}, {1, 1}}, {{-s,  s, -s}, {0, 0, -1}, {1, 0}},
        {{ s, -s, -s}, {0, 0, -1}, {0, 1}}, {{-s,  s, -s}, {0, 0, -1}, {1, 0}}, {{ s,  s, -s}, {0, 0, -1}, {0, 0}},
    };
    indices.resize(36);
    for (uint32_t i = 0; i < 36; ++i) {
        indices[i] = i;
    }
    // Non-skinned: identity joint
    for (auto& v : vertices) {
        v.joints = {0, 0, 0, 0};
        v.weights = {1.0f, 0.0f, 0.0f, 0.0f};
    }
}

void GltfMesh::create_plane(float size_x, float size_z)
{
    const float hx = size_x * 0.5f;
    const float hz = size_z * 0.5f;
    vertices = {
        {{-hx, 0.0f, -hz}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ hx, 0.0f, -hz}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ hx, 0.0f,  hz}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-hx, 0.0f,  hz}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
    };
    indices = {0, 1, 2, 0, 2, 3};
    // Non-skinned: identity joint
    for (auto& v : vertices) {
        v.joints = {0, 0, 0, 0};
        v.weights = {1.0f, 0.0f, 0.0f, 0.0f};
    }
}

} // namespace kenga
