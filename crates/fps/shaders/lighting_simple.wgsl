// 3D boxes with per-instance model, global viewProj, texture support and dynamic lighting

struct Camera {
  viewProj: mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> uCamera: Camera;

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
  return out;
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
  
  // Combine vertex color with texture and lighting
  let col = base * texColor.rgb * light;
  return vec4<f32>(col, texColor.a);
}