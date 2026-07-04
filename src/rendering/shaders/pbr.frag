#version 450
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

struct GpuLight {
    vec4 position_type;      // xyz = position, w = type (0=dir,1=point,2=spot)
    vec4 direction_cutoff;   // xyz = direction, w = outer cutoff (cos)
    vec4 color_intensity;    // xyz = color, w = intensity
    vec4 attenuation;        // x = radius, y = inner cutoff (cos), zw = reserved
};

layout(set = 0, binding = 0, std140) uniform UniformBufferObject {
    mat4 model;              // offset 0
    mat4 view;               // offset 64
    mat4 proj;               // offset 128
    mat4 light_mvp;          // offset 192
    GpuLight lights[8];      // offset 256 (each 64 bytes)
    // offset 768:
    int num_lights;          // 4 bytes
    int _pad0;               // 4 bytes
    int _pad1;               // 4 bytes
    int _pad2;               // 4 bytes
    // offset 784:
    vec4 camera_pos;         // 16 bytes (xyz = camera world position)
    // offset 800:
    float ibl_intensity;     // 4 bytes
    float _pad3;             // 4 bytes
    float _pad4;             // 4 bytes
    float _pad5;             // 4 bytes
    // total: 816 bytes
} ubo;

layout(set = 0, binding = 1) uniform sampler2DShadow shadow_sampler;
layout(set = 0, binding = 2) uniform sampler2D albedo_sampler;
layout(set = 0, binding = 3) uniform samplerCube irradiance_map;
layout(set = 0, binding = 4) uniform samplerCube prefiltered_map;
layout(set = 0, binding = 5) uniform sampler2D brdf_lut;

layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 baseColor_metallic_roughness; // rgb = base color, a = packed metallic/roughness
} pc;

float calc_attenuation(float distance, float radius) {
    float att = clamp(1.0 - (distance * distance) / (radius * radius), 0.0, 1.0);
    return att * att;
}

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

vec3 fresnel_schlick_roughness(float cos_theta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.camera_pos.xyz - fragWorldPos);

    vec3 albedo_tex = texture(albedo_sampler, fragUV).rgb;
    vec3 base_color = pc.baseColor_metallic_roughness.rgb * albedo_tex;

    // Unpack metallic and roughness from push constant alpha
    // a encodes: integer part = metallic*10, fractional = roughness
    // Simple encoding: metallic = floor(a) / 10, roughness = fract(a)
    // For now use fixed values derived from the push constant
    float metallic = 0.0;
    float roughness = clamp(pc.baseColor_metallic_roughness.a, 0.01, 1.0);

    vec3 F0 = mix(vec3(0.04), base_color, metallic);

    // Shadow from first directional light (light_mvp)
    // Shadow pass renders with GLM ortho (Y-up) into Vulkan depth (Y-down viewport).
    // UBO light_mvp = light_proj * light_view (no Y-flip, no model).
    // fragWorldPos is already in world space.
    vec4 shadow_clip = ubo.light_mvp * vec4(fragWorldPos, 1.0);
    vec3 shadow_ndc = shadow_clip.xyz / shadow_clip.w;
    // NDC to UV: x [-1,1] -> [0,1], y [-1,1] -> [0,1]
    // Vulkan renders shadow map with Y-down viewport, so shadow_ndc.y is inverted
    // relative to texture UV. Flip Y to compensate.
    vec2 shadow_uv = vec2(shadow_ndc.x * 0.5 + 0.5, 1.0 - (shadow_ndc.y * 0.5 + 0.5));

    float shadow = 0.0;
    if (shadow_ndc.z >= 0.0 && shadow_ndc.z <= 1.0 &&
        shadow_uv.x >= 0.0 && shadow_uv.x <= 1.0 &&
        shadow_uv.y >= 0.0 && shadow_uv.y <= 1.0) {
        float texel = 1.0 / 2048.0;
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                shadow += texture(shadow_sampler,
                                 vec3(shadow_uv + vec2(x, y) * texel, shadow_ndc.z));
            }
        }
        shadow /= 9.0;
    } else {
        shadow = 1.0;
    }

    // Accumulate direct lighting
    vec3 total_diffuse = vec3(0.0);
    for (int i = 0; i < ubo.num_lights; ++i) {
        GpuLight light = ubo.lights[i];
        int light_type = int(light.position_type.w);
        vec3 light_color = light.color_intensity.xyz;
        float intensity = light.color_intensity.w;
        vec3 L;
        float attenuation = 1.0;

        if (light_type == 0) {
            L = -normalize(light.direction_cutoff.xyz);
            if (i == 0) {
                attenuation = mix(0.4, 1.0, shadow); // min 40% even in full shadow
            }
        } else if (light_type == 1) {
            vec3 to_light = light.position_type.xyz - fragWorldPos;
            float dist = length(to_light);
            L = to_light / max(dist, 0.001);
            attenuation = calc_attenuation(dist, light.attenuation.x);
        } else {
            vec3 to_light = light.position_type.xyz - fragWorldPos;
            float dist = length(to_light);
            L = to_light / max(dist, 0.001);
            attenuation = calc_attenuation(dist, light.attenuation.x);

            float theta = dot(L, -normalize(light.direction_cutoff.xyz));
            float inner = light.attenuation.y;
            float outer = light.direction_cutoff.w;
            float epsilon = inner - outer;
            float spot_factor = clamp((theta - outer) / max(epsilon, 0.001), 0.0, 1.0);
            attenuation *= spot_factor;
        }

        float NdotL = max(dot(N, L), 0.0);
        total_diffuse += base_color * light_color * intensity * NdotL * attenuation;
    }

    // IBL ambient
    float NdotV = max(dot(N, V), 0.0);
    vec3 F = fresnel_schlick_roughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - F) * (1.0 - metallic);

    vec3 irradiance = texture(irradiance_map, N).rgb;
    vec3 diffuse_ibl = irradiance * base_color;

    vec3 R = reflect(-V, N);
    const float max_lod = 4.0;
    vec3 prefiltered = textureLod(prefiltered_map, R, roughness * max_lod).rgb;
    vec2 env_brdf = texture(brdf_lut, vec2(NdotV, roughness)).rg;
    vec3 specular_ibl = prefiltered * (F * env_brdf.x + env_brdf.y);

    vec3 ambient = (kD * diffuse_ibl + specular_ibl) * ubo.ibl_intensity;

    // Minimum ambient so objects are never completely black
    vec3 min_ambient = base_color * 0.08;
    vec3 color = max(ambient, min_ambient) + total_diffuse;

    outColor = vec4(color, 1.0);
}
