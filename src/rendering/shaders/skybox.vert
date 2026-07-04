#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;   // unused, but matches PbrVertex layout
layout(location = 2) in vec2 inUV;       // unused

layout(location = 0) out vec3 local_pos;

layout(push_constant) uniform PushConstants {
    mat4 view_proj; // proj * mat4(mat3(view)) — no translation
} pc;

void main() {
    local_pos = inPosition;
    vec4 clip_pos = pc.view_proj * vec4(inPosition, 1.0);
    // Set z = w so skybox is always at max depth (behind everything)
    gl_Position = clip_pos.xyww;
}
