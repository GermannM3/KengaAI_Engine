#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in uvec4 inJoints;
layout(location = 4) in vec4 inWeights;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;

struct GpuLight {
    vec4 position_type;      // xyz = position, w = type (0=dir,1=point,2=spot)
    vec4 direction_cutoff;   // xyz = direction, w = outer cutoff (cos)
    vec4 color_intensity;    // xyz = color, w = intensity
    vec4 attenuation;        // x = radius, y = inner cutoff (cos), zw = reserved
};

layout(set = 0, binding = 0, std140) uniform UniformBufferObject {
    mat4 model;              // offset 0 (not used — push constants override)
    mat4 view;               // offset 64
    mat4 proj;               // offset 128
    mat4 light_mvp;          // offset 192
    GpuLight lights[8];      // offset 256 (each 64 bytes)
    int num_lights;          // offset 768
    int _pad0;
    int _pad1;
    int _pad2;
    vec4 camera_pos;         // offset 784
    float ibl_intensity;     // offset 800
    float _pad3;
    float _pad4;
    float _pad5;
} ubo;

// Joint matrices SSBO for skeletal animation
layout(set = 0, binding = 6, std430) readonly buffer JointBuffer {
    mat4 joint_matrices[128];
};

layout(push_constant) uniform PushConstants {
    mat4 model;              // per-draw model matrix
} pc;

void main() {
    // Compute skin matrix from joint weights
    float w_sum = inWeights.x + inWeights.y + inWeights.z + inWeights.w;
    mat4 skin_matrix;
    if (w_sum > 0.001 && (inJoints.x > 0u || inJoints.y > 0u || inJoints.z > 0u || inJoints.w > 0u || inWeights.y > 0.0 || inWeights.z > 0.0 || inWeights.w > 0.0)) {
        // Skinned vertex: blend joint matrices
        skin_matrix  = inWeights.x * joint_matrices[inJoints.x];
        skin_matrix += inWeights.y * joint_matrices[inJoints.y];
        skin_matrix += inWeights.z * joint_matrices[inJoints.z];
        skin_matrix += inWeights.w * joint_matrices[inJoints.w];
    } else {
        // Non-skinned vertex (cubes, planes): identity
        skin_matrix = mat4(1.0);
    }

    mat4 final_model = pc.model * skin_matrix;
    vec4 world_pos = final_model * vec4(inPosition, 1.0);
    fragWorldPos = world_pos.xyz;
    fragNormal = mat3(transpose(inverse(final_model))) * inNormal;
    fragUV = inUV;
    gl_Position = ubo.proj * ubo.view * world_pos;
}
