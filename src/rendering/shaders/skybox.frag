#version 450

layout(location = 0) in vec3 local_pos;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform samplerCube env_map;

void main() {
    vec3 color = texture(env_map, normalize(local_pos)).rgb;
    out_color = vec4(color, 1.0);
}
