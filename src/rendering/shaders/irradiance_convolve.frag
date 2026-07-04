#version 450

layout(location = 0) in vec3 local_pos;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform samplerCube env_map;

const float PI = 3.14159265359;

void main() {
    vec3 N = normalize(local_pos);

    // Build tangent frame
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = cross(N, right);

    vec3 irradiance = vec3(0.0);
    float sample_count = 0.0;

    float delta = 0.025;
    for (float phi = 0.0; phi < 2.0 * PI; phi += delta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += delta) {
            // Spherical to cartesian (tangent space)
            vec3 tangent_sample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );
            // Tangent space to world
            vec3 sample_vec = tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * N;

            irradiance += texture(env_map, sample_vec).rgb * cos(theta) * sin(theta);
            sample_count += 1.0;
        }
    }
    irradiance = PI * irradiance / sample_count;

    out_color = vec4(irradiance, 1.0);
}
