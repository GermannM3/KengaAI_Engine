#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdr_scene;
layout(set = 0, binding = 1) uniform sampler2D depth_tex;

layout(push_constant) uniform PushConstants {
    mat4 inv_view_proj;
    mat4 view_proj;
    float step_size;        // ray march step in world units
    float max_distance;     // max ray distance
    float thickness;        // depth comparison threshold
    float intensity;        // reflection blend factor
    float near_plane;
    float far_plane;
    int max_steps;
    float _pad;
} pc;

float linearize_depth(float d) {
    return pc.near_plane * pc.far_plane / (pc.far_plane - d * (pc.far_plane - pc.near_plane));
}

vec3 world_from_uv(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = pc.inv_view_proj * clip;
    return world.xyz / world.w;
}

vec2 project_to_uv(vec3 world_pos) {
    vec4 clip = pc.view_proj * vec4(world_pos, 1.0);
    clip.xyz /= clip.w;
    return clip.xy * 0.5 + 0.5;
}

void main() {
    vec3 scene = texture(hdr_scene, fragUV).rgb;
    float raw_depth = texture(depth_tex, fragUV).r;

    // Skip sky
    if (raw_depth > 0.999) {
        outColor = vec4(scene, 1.0);
        return;
    }

    vec3 world_pos = world_from_uv(fragUV, raw_depth);

    // Approximate normal from depth (screen-space)
    float d_right = texture(depth_tex, fragUV + vec2(1.0/1280.0, 0.0)).r;
    float d_up    = texture(depth_tex, fragUV + vec2(0.0, 1.0/720.0)).r;
    vec3 p_right = world_from_uv(fragUV + vec2(1.0/1280.0, 0.0), d_right);
    vec3 p_up    = world_from_uv(fragUV + vec2(0.0, 1.0/720.0), d_up);
    vec3 normal = normalize(cross(p_right - world_pos, p_up - world_pos));

    // Only reflect surfaces facing somewhat upward (floor-like)
    if (normal.y < 0.3) {
        outColor = vec4(scene, 1.0);
        return;
    }

    // Reflect view direction
    vec3 view_dir = normalize(world_pos); // approximate: camera at origin in view space
    vec3 reflect_dir = reflect(normalize(world_pos - vec3(0.0)), normal);

    // Ray march
    vec3 ray_pos = world_pos + reflect_dir * pc.step_size;
    vec3 reflection = vec3(0.0);
    bool hit = false;

    for (int i = 0; i < pc.max_steps; ++i) {
        vec2 sample_uv = project_to_uv(ray_pos);

        if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0) {
            break;
        }

        float sample_depth = texture(depth_tex, sample_uv).r;
        float sample_linear = linearize_depth(sample_depth);
        float ray_linear = linearize_depth(project_to_uv(ray_pos).x); // approximate

        vec4 ray_clip = pc.view_proj * vec4(ray_pos, 1.0);
        float ray_depth_linear = linearize_depth(ray_clip.z / ray_clip.w);

        if (ray_depth_linear > sample_linear && ray_depth_linear - sample_linear < pc.thickness) {
            reflection = texture(hdr_scene, sample_uv).rgb;
            hit = true;
            break;
        }

        ray_pos += reflect_dir * pc.step_size;
    }

    if (hit) {
        // Fade by distance from screen edge
        float edge_fade = 1.0;
        vec2 sample_uv = project_to_uv(ray_pos);
        vec2 edge = abs(sample_uv - 0.5) * 2.0;
        edge_fade *= 1.0 - smoothstep(0.8, 1.0, max(edge.x, edge.y));

        scene = mix(scene, reflection, pc.intensity * edge_fade);
    }

    outColor = vec4(scene, 1.0);
}
