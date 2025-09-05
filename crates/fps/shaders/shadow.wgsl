// Шейдер для генерации теневой карты

struct Camera {
  viewProj: mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> uCamera: Camera;

struct VSIn {
  @location(0) pos: vec3<f32>,
  @location(3) i_pos: vec3<f32>,
  @location(4) i_scale: vec3<f32>,
  @location(5) i_rotY: f32,
};

struct VSOut {
  @builtin(position) position: vec4<f32>,
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

  out.position = uCamera.viewProj * vec4<f32>(worldPos, 1.0);
  return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) f32 {
  // Возвращаем глубину для теневой карты
  return in.position.z / in.position.w;
}