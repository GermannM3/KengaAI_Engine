#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdr_input;

layout(push_constant) uniform PushConstants {
    float threshold;
    float soft_knee;
} pc;

void main() {
    vec3 color = texture(hdr_input, fragUV).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float knee = pc.threshold * pc.soft_knee;
    float soft = brightness - pc.threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);
    float contribution = max(soft, brightness - pc.threshold);
    contribution /= max(brightness, 0.00001);
    outColor = vec4(color * contribution, 1.0);
}
