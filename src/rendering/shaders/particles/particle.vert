#version 450

// Per-vertex data (quad corners)
layout(location = 0) in vec2 inQuadOffset; // [-0.5, 0.5] quad corner

// Per-instance data from SSBO (read via gl_InstanceIndex)
struct GpuParticle {
    vec4 position_lifetime; // xyz = world position, w = total lifetime
    vec4 velocity_age;      // xyz = velocity, w = current age
    vec4 color_start;
    vec4 color_end;
    vec4 size_flags;        // x = size_start, y = size_end, z = alive (1.0/0.0), w = gravity_scale
};

layout(std430, set = 0, binding = 0) readonly buffer ParticleBuffer {
    GpuParticle particles[];
};

layout(push_constant) uniform PushConstants {
    mat4 view_proj;
    vec4 camera_right; // xyz = camera right vector
    vec4 camera_up;    // xyz = camera up vector
} pc;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;

void main() {
    GpuParticle p = particles[gl_InstanceIndex];

    // Skip dead particles (move off-screen)
    if (p.size_flags.z < 0.5) {
        gl_Position = vec4(0.0, 0.0, -2.0, 1.0);
        fragColor = vec4(0.0);
        fragUV = vec2(0.0);
        return;
    }

    float t = p.velocity_age.w / max(p.position_lifetime.w, 0.001);
    t = clamp(t, 0.0, 1.0);

    // Interpolate color and size
    fragColor = mix(p.color_start, p.color_end, t);
    float size = mix(p.size_flags.x, p.size_flags.y, t);

    // Billboard: offset quad corners in camera space
    vec3 world_pos = p.position_lifetime.xyz
                   + pc.camera_right.xyz * inQuadOffset.x * size
                   + pc.camera_up.xyz * inQuadOffset.y * size;

    gl_Position = pc.view_proj * vec4(world_pos, 1.0);
    fragUV = inQuadOffset + 0.5; // [0,1] range
}
