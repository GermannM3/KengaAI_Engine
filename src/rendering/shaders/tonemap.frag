#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdr_scene;
layout(set = 0, binding = 1) uniform sampler2D bloom_tex;

layout(push_constant) uniform PushConstants {
    float exposure;
    float bloom_strength;
} pc;

// ACES filmic tone mapping (Narkowicz 2015 approximation)
vec3 aces_tonemap(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(hdr_scene, fragUV).rgb;
    vec3 bloom = texture(bloom_tex, fragUV).rgb;

    vec3 color = hdr + bloom * pc.bloom_strength;
    color *= pc.exposure;
    color = aces_tonemap(color);

    // Gamma correction (linear -> sRGB)
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
