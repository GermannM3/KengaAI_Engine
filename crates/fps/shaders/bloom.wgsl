// Шейдер для эффекта Bloom

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) tex_coords: vec2<f32>,
};

@vertex
fn vertex_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
  var out: VertexOutput;
  // Создаем полноэкранный квадрат
  let pos = array<vec2<f32>, 6>(
    vec2<f32>(-1.0, -1.0),
    vec2<f32>( 1.0, -1.0),
    vec2<f32>(-1.0,  1.0),
    vec2<f32>( 1.0, -1.0),
    vec2<f32>( 1.0,  1.0),
    vec2<f32>(-1.0,  1.0)
  );
  
  let uv = array<vec2<f32>, 6>(
    vec2<f32>(0.0, 0.0),
    vec2<f32>(1.0, 0.0),
    vec2<f32>(0.0, 1.0),
    vec2<f32>(1.0, 0.0),
    vec2<f32>(1.0, 1.0),
    vec2<f32>(0.0, 1.0)
  );
  
  out.position = vec4<f32>(pos[vertex_index], 0.0, 1.0);
  out.tex_coords = uv[vertex_index];
  return out;
}

// Текстура входного изображения
@group(0) @binding(0) var input_texture: texture_2d<f32>;
@group(0) @binding(1) var input_sampler: sampler;

// Параметры bloom
@group(0) @binding(2) var<uniform> bloom_params: vec3<f32>; // x: threshold, y: intensity, z: unused

fn luminance(color: vec3<f32>) -> f32 {
  return dot(color, vec3<f32>(0.2126, 0.7152, 0.0722));
}

@fragment
fn fragment_main(in: VertexOutput) -> @location(0) vec4<f32> {
  let color = textureSample(input_texture, input_sampler, in.tex_coords).rgb;
  
  // Применяем порог яркости для выделения самых ярких частей
  let brightness = max(luminance(color) - bloom_params.x, 0.0);
  let contribution = brightness / (brightness + 1.0);
  
  // Создаем bloom эффект
  let bloom = color * contribution * bloom_params.y;
  
  // Комбинируем оригинальное изображение с bloom
  let final_color = color + bloom;
  
  return vec4<f32>(final_color, 1.0);
}