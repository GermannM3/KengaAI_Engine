// Шейдер для рендеринга mesh-объектов с PBR

struct Camera {
  viewProj: mat4x4<f32>,
  position: vec3<f32>,
};
@group(0) @binding(0) var<uniform> uCamera: Camera;

struct VSIn {
  @location(0) pos: vec3<f32>,
  @location(1) normal: vec3<f32>,
  @location(2) tex_coords: vec2<f32>,
  @location(3) i_pos: vec3<f32>,
  @location(4) i_scale: vec3<f32>,
  @location(5) i_rotY: f32,
};

struct VSOut {
  @builtin(position) position: vec4<f32>,
  @location(0) worldPos: vec3<f32>,
  @location(1) normal: vec3<f32>,
  @location(2) tex_coords: vec2<f32>,
};

fn rotationY(a: f32) -> mat4x4<f32> {
  let c = cos(a);
  let s = sin(a);
  return mat4x4<f32>(
    vec4<f32>( c, 0.0, s, 0.0),
    vec4<f32>( 0.0, 1.0, 0.0, 0.0),
    vec4<f32>(-s, 0.0, c, 0.0),
    vec4<f32>( 0.0, 0.0, 0.0, 1.0)
  );
}

@vertex
fn vs_main(input: VSIn) -> VSOut {
  var out: VSOut;
  let modelScale = mat4x4<f32>(
    vec4<f32>(input.i_scale.x, 0.0, 0.0, 0.0),
    vec4<f32>(0.0, input.i_scale.y, 0.0, 0.0),
    vec4<f32>(0.0, 0.0, input.i_scale.z, 0.0),
    vec4<f32>(0.0, 0.0, 0.0, 1.0)
  );
  let modelRot = rotationY(input.i_rotY);
  let modelTrans = mat4x4<f32>(
    vec4<f32>(1.0, 0.0, 0.0, 0.0),
    vec4<f32>(0.0, 1.0, 0.0, 0.0),
    vec4<f32>(0.0, 0.0, 1.0, 0.0),
    vec4<f32>(input.i_pos, 1.0)
  );
  let model = modelTrans * modelRot * modelScale;

  let worldPos = (model * vec4<f32>(input.pos, 1.0)).xyz;
  let worldNormal = (modelRot * vec4<f32>(input.normal, 0.0)).xyz;

  out.position = uCamera.viewProj * vec4<f32>(worldPos, 1.0);
  out.worldPos = worldPos;
  out.normal = worldNormal;
  out.tex_coords = input.tex_coords;
  return out;
}

// Textures
@group(1) @binding(0) var baseColorTexture: texture_2d<f32>;
@group(1) @binding(1) var baseColorSampler: sampler;
@group(1) @binding(2) var normalTexture: texture_2d<f32>;
@group(1) @binding(3) var normalSampler: sampler;
@group(1) @binding(4) var metallicTexture: texture_2d<f32>;
@group(1) @binding(5) var metallicSampler: sampler;
@group(1) @binding(6) var roughnessTexture: texture_2d<f32>;
@group(1) @binding(7) var roughnessSampler: sampler;

// Material properties
struct Material {
  baseColorFactor: vec4<f32>,
  metallicFactor: f32,
  roughnessFactor: f32,
};
@group(2) @binding(0) var<uniform> uMaterial: Material;

// Lighting
struct Light {
  position: vec3<f32>,
  color: vec3<f32>,
  intensity: f32,
  kind: u32, // 0 = point, 1 = directional
};

struct Lights {
  count: u32,
  lights: array<Light, 16>,
};
@group(3) @binding(0) var<uniform> uLights: Lights;

// PBR functions
fn fresnelSchlick(cosTheta: f32, F0: vec3<f32>) -> vec3<f32> {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

fn DistributionGGX(N: vec3<f32>, H: vec3<f32>, roughness: f32) -> f32 {
  let a = roughness * roughness;
  let a2 = a * a;
  let NdotH = max(dot(N, H), 0.0);
  let NdotH2 = NdotH * NdotH;
  
  let nom = a2;
  let denom = (NdotH2 * (a2 - 1.0) + 1.0);
  let denom2 = denom * denom;
  
  return nom / (3.14159265359 * denom2);
}

fn GeometrySchlickGGX(NdotV: f32, roughness: f32) -> f32 {
  let r = (roughness + 1.0);
  let k = (r * r) / 8.0;
  
  let nom = NdotV;
  let denom = NdotV * (1.0 - k) + k;
  
  return nom / denom;
}

fn GeometrySmith(N: vec3<f32>, V: vec3<f32>, L: vec3<f32>, roughness: f32) -> f32 {
  let NdotV = max(dot(N, V), 0.0);
  let NdotL = max(dot(N, L), 0.0);
  let ggx2 = GeometrySchlickGGX(NdotV, roughness);
  let ggx1 = GeometrySchlickGGX(NdotL, roughness);
  
  return ggx1 * ggx2;
}

fn calculatePBR(N: vec3<f32>, V: vec3<f32>, L: vec3<f32>, lightColor: vec3<f32>, lightIntensity: f32, albedo: vec3<f32>, metallic: f32, roughness: f32) -> vec3<f32> {
  let H = normalize(V + L);
  
  let F0 = mix(vec3<f32>(0.04), albedo, metallic);
  
  // Расчет компонентов PBR
  let NDF = DistributionGGX(N, H, roughness);
  let G = GeometrySmith(N, V, L, roughness);
  let F = fresnelSchlick(max(dot(H, V), 0.0), F0);
  
  let kS = F;
  let kD = vec3<f32>(1.0) - kS;
  kD = kD * (1.0 - metallic);
  
  let numerator = NDF * G * F;
  let denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
  let specular = numerator / max(denominator, 0.001);
  
  let NdotL = max(dot(N, L), 0.0);
  let radiance = lightColor * lightIntensity;
  
  return (kD * albedo / 3.14159265359 + specular) * radiance * NdotL;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
  let N = normalize(in.normal);
  let V = normalize(uCamera.position - in.worldPos);
  
  // Sample textures
  let baseColor = textureSample(baseColorTexture, baseColorSampler, in.tex_coords) * uMaterial.baseColorFactor;
  let normalMap = textureSample(normalTexture, normalSampler, in.tex_coords);
  let metallic = textureSample(metallicTexture, metallicSampler, in.tex_coords).r * uMaterial.metallicFactor;
  let roughness = textureSample(roughnessTexture, roughnessSampler, in.tex_coords).g * uMaterial.roughnessFactor;
  
  let albedo = baseColor.rgb;
  
  // Ambient lighting
  var Lo = albedo * 0.03;
  
  // Process lights
  for (var i: u32 = 0u; i < uLights.count && i < 16u; i = i + 1u) {
    let light = uLights.lights[i];
    
    if (light.kind == 0u) { // Point light
      let lightDir = light.position - in.worldPos;
      let distance = length(lightDir);
      let L = normalize(lightDir);
      
      let attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
      let lightIntensity = light.intensity * attenuation;
      
      Lo += calculatePBR(N, V, L, light.color, lightIntensity, albedo, metallic, roughness);
    } else if (light.kind == 1u) { // Directional light
      let L = normalize(-light.position);
      Lo += calculatePBR(N, V, L, light.color, light.intensity, albedo, metallic, roughness);
    }
  }
  
  // HDR tonemapping
  Lo = Lo / (Lo + vec3<f32>(1.0));
  
  // Gamma correction
  Lo = pow(Lo, vec3<f32>(1.0 / 2.2));
  
  return vec4<f32>(Lo, baseColor.a);
}