// 3D boxes with per-instance model, global viewProj, texture support, dynamic lighting and shadows

struct Camera {
  viewProj: mat4x4<f32>,
  view: mat4x4<f32>,
  proj: mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> uCamera: Camera;

// Light view-projection matrix for shadow mapping
@group(0) @binding(1) var<uniform> uLightViewProj: mat4x4<f32>;

// Shadow map texture
@group(0) @binding(2) var shadowMap: texture_depth_2d;
@group(0) @binding(3) var shadowSampler: sampler;

// Lighting data
struct Light {
  position: vec3<f32>,
  color: vec3<f32>,
  intensity: f32,
  kind: u32, // 0 = point, 1 = directional
};

struct Lights {
  count: u32,
  lights: array<Light, 16>, // Максимум 16 источников света
};

@group(2) @binding(0) var<uniform> uLights: Lights;

// Texture support
@group(1) @binding(0) var tex: texture_2d<f32>;
@group(1) @binding(1) var samp: sampler;

struct VSIn {
  @location(0) pos: vec3<f32>,
  @location(1) normal: vec3<f32>,
  @location(2) tex_coords: vec2<f32>,
  @location(3) i_pos: vec3<f32>,
  @location(4) i_scale: vec3<f32>,
  @location(5) i_rotY: f32,
  @location(6) i_color: vec3<f32>,
};

struct VSOut {
  @builtin(position) position: vec4<f32>,
  @location(0) worldPos: vec3<f32>,
  @location(1) color: vec3<f32>,
  @location(2) normal: vec3<f32>,
  @location(3) tex_coords: vec2<f32>,
  @location(4) lightSpacePos: vec4<f32>,
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
  out.color = input.i_color;
  out.normal = worldNormal;
  out.tex_coords = input.tex_coords;
  out.lightSpacePos = uLightViewProj * vec4<f32>(worldPos, 1.0);
  return out;
}

fn calculateShadow(lightSpacePos: vec4<f32>) -> f32 {
  // Преобразуем координаты в пространство текстуры
  let projCoords = lightSpacePos.xyz / lightSpacePos.w;
  // Преобразуем [-1, 1] в [0, 1]
  let uv = projCoords.xy * 0.5 + 0.5;
  
  // Получаем ближайшую глубину из теневой карты
  let closestDepth = textureSample(shadowMap, shadowSampler, uv);
  // Получаем текущую глубину фрагмента
  let currentDepth = projCoords.z;
  
  // Проверяем, находится ли фрагмент в тени
  let bias = 0.005;
  if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0) {
    if (currentDepth - bias > closestDepth) {
      return 0.5; // В тени
    }
  }
  return 1.0; // Не в тени
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
  let n = normalize(in.normal);
  let base = in.color;
  
  // Sample texture
  let texColor = textureSample(tex, samp, in.tex_coords);
  
  // Ambient light
  var light = vec3<f32>(0.1, 0.1, 0.1);
  
  // Process lights
  for (var i: u32 = 0u; i < uLights.count && i < 16u; i = i + 1u) {
    let lightData = uLights.lights[i];
    
    if (lightData.kind == 0u) { // Point light
      let lightDir = lightData.position - in.worldPos;
      let distance = length(lightDir);
      let normalizedDir = lightDir / distance;
      
      let diff = max(dot(n, normalizedDir), 0.0);
      let attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
      
      light += lightData.color * diff * lightData.intensity * attenuation;
    } else if (lightData.kind == 1u) { // Directional light
      let lightDir = normalize(-lightData.position);
      let diff = max(dot(n, lightDir), 0.0);
      light += lightData.color * diff * lightData.intensity;
    }
  }
  
  // Apply shadows
  let shadow = calculateShadow(in.lightSpacePos);
  
  // Combine vertex color with texture and lighting
  let col = base * texColor.rgb * light * shadow;
  return vec4<f32>(col, texColor.a);
}