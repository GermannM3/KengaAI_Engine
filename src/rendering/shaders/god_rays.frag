#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdr_scene;

layout(push_constant) uniform PushConstants {
    vec2 light_screen_pos;  // light position in screen space [0..1]
    float density;          // ray density (0.5 - 2.0)
    float weight;           // contribution weight (0.01 - 0.1)
    float decay;            // per-sample decay (0.9 - 1.0)
    float exposure_rays;    // final exposure multiplier
    int num_samples;        // number of ray samples (30 - 100)
    float _pad;
} pc;

void main() {
    vec2 uv = fragUV;
    vec2 delta = (uv - pc.light_screen_pos) * (1.0 / float(pc.num_samples)) * pc.density;

    vec3 accum = vec3(0.0);
    float illumination_decay = 1.0;

    vec2 sample_uv = uv;
    for (int i = 0; i < pc.num_samples; ++i) {
        sample_uv -= delta;
        vec3 s = texture(hdr_scene, clamp(sample_uv, 0.0, 1.0)).rgb;
        s *= illumination_decay * pc.weight;
        accum += s;
        illumination_decay *= pc.decay;
    }

    vec3 scene = texture(hdr_scene, uv).rgb;
    outColor = vec4(scene + accum * pc.exposure_rays, 1.0);
}
