// Шейдер для рендеринга частиц

struct Camera {
  viewProj: mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> uCamera: Camera;

struct VSIn {
  @location(0) pos: vec2<f32>, // Billboard quad vertex
  @location(1) particlePos: vec3<f32>, // Particle position
  @location(2) particleColor: vec3<f32>, // Particle color
  @location(3) particleLifetime: f32, // Particle lifetime
};

struct VSOut {
  @builtin(position) position: vec4<f32>,
  @location(0) color: vec3<f32>,
  @location(1) tex_coords: vec2<f32>,
};

@vertex
fn vs_main(input: VSIn) -> VSOut {
  var out: VSOut;
  // Преобразуем вершину билборда в мировые координаты
  let worldPos = input.particlePos + vec3<f32>(input.pos.x, input.pos.y, 0.0);
  out.position = uCamera.viewProj * vec4<f32>(worldPos, 1.0);
  out.color = input.particleColor;
  out.tex_coords = input.pos * 0.5 + 0.5; // Преобразуем из [-1,1] в [0,1]
  return out;
}

// Текстура частицы (градиент круга)
@group(1) @binding(0) var tex: texture_2d<f32>;
@group(1) @binding(1) var samp: sampler;

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
  let texColor = textureSample(tex, samp, in.tex_coords);
  return vec4<f32>(in.color * texColor.rgb, texColor.a);
}