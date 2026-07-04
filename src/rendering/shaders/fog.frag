#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdr_scene;
layout(set = 0, binding = 1) uniform sampler2D depth_tex;

layout(push_constant) uniform PushConstants {
    vec3 fog_color;
    float fog_density;      // 0.01 - 0.5
    float fog_start;        // near distance where fog begins
    float fog_end;          // distance where fog is fully opaque
    float fog_height;       // height falloff center
    float fog_height_falloff; // how fast fog fades with height (0 = no height fog)
    float near_plane;
    float far_plane;
    float camera_y;         // camera world Y for height fog
    float _pad;
} pc;

float linearize_depth(float d) {
    return pc.near_plane * pc.far_plane / (pc.far_plane - d * (pc.far_plane - pc.near_plane));
}

void main() {
    vec3 scene = texture(hdr_scene, fragUV).rgb;
    float raw_depth = texture(depth_tex, fragUV).r;

    // Skip sky (depth = 1.0 or very close)
    if (raw_depth > 0.999) {
        outColor = vec4(scene, 1.0);
        return;
    }

    float linear_d = linearize_depth(raw_depth);

    // Distance fog: exponential
    float dist_factor = 1.0 - exp(-pc.fog_density * max(linear_d - pc.fog_start, 0.0));
    dist_factor = clamp(dist_factor, 0.0, 1.0);

    // Height fog: reduce fog above fog_height
    float height_factor = 1.0;
    if (pc.fog_height_falloff > 0.0) {
        // Approximate: use camera Y + depth to estimate world height
        // This is a screen-space approximation
        float approx_y = pc.camera_y - (fragUV.y - 0.5) * linear_d * 0.5;
        height_factor = exp(-pc.fog_height_falloff * max(approx_y - pc.fog_height, 0.0));
        height_factor = clamp(height_factor, 0.0, 1.0);
    }

    float fog = dist_factor * height_factor;
    vec3 result = mix(scene, pc.fog_color, fog);

    outColor = vec4(result, 1.0);
}
