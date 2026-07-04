#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
    // Soft circle: distance from center of quad
    float dist = length(fragUV - vec2(0.5));
    float alpha = 1.0 - smoothstep(0.3, 0.5, dist);

    outColor = vec4(fragColor.rgb, fragColor.a * alpha);

    // Discard fully transparent fragments
    if (outColor.a < 0.01) discard;
}
