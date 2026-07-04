#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D input_tex;

layout(push_constant) uniform PushConstants {
    vec2 direction; // (1/w, 0) for horizontal, (0, 1/h) for vertical
} pc;

// 9-tap Gaussian weights (sigma ~= 4)
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec3 result = texture(input_tex, fragUV).rgb * weights[0];
    for (int i = 1; i < 5; ++i) {
        vec2 offset = pc.direction * float(i);
        result += texture(input_tex, fragUV + offset).rgb * weights[i];
        result += texture(input_tex, fragUV - offset).rgb * weights[i];
    }
    outColor = vec4(result, 1.0);
}
