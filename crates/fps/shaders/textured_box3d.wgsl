// MVP 3D boxes with per-instance model, global viewProj, and texture support

struct Camera {
  viewProj: mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> uCamera: Camera;

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
  @location(0) color: vec3<f32>,
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
  out.color = input.i_color;
  out.normal = worldNormal;
  out.tex_coords = input.tex_coords;
  return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
  // simple lambert with fixed light
  let lightDir = normalize(vec3<f32>(0.4, 1.0, 0.3));
  let n = normalize(in.normal);
  let diff = max(dot(n, lightDir), 0.0);
  let base = in.color;
  
  // Sample texture
  let texColor = textureSample(tex, samp, in.tex_coords);
  
  // Combine vertex color with texture
  let col = base * texColor.rgb * (0.25 + 0.75 * diff);
  return vec4<f32>(col, texColor.a);
}