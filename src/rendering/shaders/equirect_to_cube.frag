#version 450

layout(location = 0) in vec3 local_pos;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D equirect_map;

const vec2 inv_atan = vec2(0.1591, 0.3183); // 1/(2*PI), 1/PI

vec2 sample_spherical(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= inv_atan;
    uv += 0.5;
    return uv;
}

void main() {
    vec3 dir = normalize(local_pos);
    vec2 uv = sample_spherical(dir);
    vec3 color = texture(equirect_map, uv).rgb;
    out_color = vec4(color, 1.0);
}
