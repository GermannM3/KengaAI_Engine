use anyhow::Result;
use bytemuck::{Pod, Zeroable};
use glam::{Mat4, Vec2, Vec3};
use kengaai_scene_fps::{BoxDef, FpsScene};
use log::info;
use wgpu::util::DeviceExt;
use winit::window::Window;

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct Vertex {
    pos: [f32; 3],
    normal: [f32; 3],
    tex_coords: [f32; 2],
}

fn cube_vertices() -> Vec<Vertex> {
    // Unit cube centered at origin
    let p = [
        // positions
        [-1.0, -1.0,  1.0], [ 1.0, -1.0,  1.0], [ 1.0,  1.0,  1.0], [-1.0,  1.0,  1.0], // front
        [-1.0, -1.0, -1.0], [ 1.0, -1.0, -1.0], [ 1.0,  1.0, -1.0], [-1.0,  1.0, -1.0], // back
    ];
    // texture coordinates
    let uv = [
        [0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0], // front
        [0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0], // back
    ];
    let faces: [([usize; 4], [f32; 3], [usize; 4]); 6] = [
        ([0,1,2,3], [0.0, 0.0, 1.0], [0,1,2,3]), // front
        ([5,4,7,6], [0.0, 0.0,-1.0], [4,5,6,7]), // back
        ([4,0,3,7], [-1.0, 0.0,0.0], [8,9,10,11]), // left: uv indices 8-11
        ([1,5,6,2], [ 1.0, 0.0,0.0], [12,13,14,15]), // right: uv indices 12-15
        ([3,2,6,7], [ 0.0, 1.0,0.0], [16,17,18,19]), // top: uv indices 16-19
        ([4,5,1,0], [ 0.0,-1.0,0.0], [20,21,22,23]), // bottom: uv indices 20-23
    ];
    let mut v = Vec::with_capacity(36);
    for (idx, n, uv_idx) in faces {
        let i0 = p[idx[0]];
        let i1 = p[idx[1]];
        let i2 = p[idx[2]];
        let i3 = p[idx[3]];
        let uv0 = uv[uv_idx[0] % 8]; // Ensure we don't go out of bounds
        let uv1 = uv[uv_idx[1] % 8]; // Ensure we don't go out of bounds
        let uv2 = uv[uv_idx[2] % 8]; // Ensure we don't go out of bounds
        let uv3 = uv[uv_idx[3] % 8]; // Ensure we don't go out of bounds
        let tri = [
            Vertex { pos: i0, normal: n, tex_coords: uv0 },
            Vertex { pos: i1, normal: n, tex_coords: uv1 },
            Vertex { pos: i2, normal: n, tex_coords: uv2 },
            Vertex { pos: i0, normal: n, tex_coords: uv0 },
            Vertex { pos: i2, normal: n, tex_coords: uv2 },
            Vertex { pos: i3, normal: n, tex_coords: uv3 },
        ];
        v.extend_from_slice(&tri);
    }
    v
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct Instance {
    pos: [f32; 3],
    scale: [f32; 3],
    rot_y: f32,
    color: [f32; 3],
    _pad: f32,
}

impl From<&BoxDef> for Instance {
    fn from(b: &BoxDef) -> Self {
        Self {
            pos: b.pos,
            scale: b.size,
            rot_y: b.rot_y,
            color: b.color,
            _pad: 0.0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct CameraUBO {
    view_proj: [[f32; 4]; 4],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct LightRaw {
    position: [f32; 3],
    _pad0: f32,
    color: [f32; 3],
    intensity: f32,
    kind: u32,
    _pad1: [u32; 3],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct LightsUBO {
    count: u32,
    _pad: [u32; 3],
    lights: [LightRaw; 16], // Максимум 16 источников света
}

pub struct FpsRenderer<'w> {
    surface: wgpu::Surface<'w>,
    device: wgpu::Device,
    queue: wgpu::Queue,
    config: wgpu::SurfaceConfiguration,
    size: winit::dpi::PhysicalSize<u32>,
    color: wgpu::Color,

    depth_tex: wgpu::Texture,
    depth_view: wgpu::TextureView,

    pipeline: wgpu::RenderPipeline,
    vbo: wgpu::Buffer,
    cam_buf: wgpu::Buffer,
    cam_bind: wgpu::BindGroup,
    lights_buf: wgpu::Buffer,
    lights_bind: wgpu::BindGroup,

    instances: Vec<Instance>,
    inst_buf: wgpu::Buffer,

    // Texture support
    texture_bind_group_layout: wgpu::BindGroupLayout,
    textures: std::collections::HashMap<String, (wgpu::Texture, wgpu::BindGroup)>,
    texture_sampler: wgpu::Sampler,

    pub camera: Camera,
}

pub struct Camera {
    pub pos: Vec3,
    pub yaw: f32,
    pub pitch: f32,
    pub fov_y: f32,
    pub z_near: f32,
    pub z_far: f32,
}

impl Camera {
    pub fn view(&self) -> Mat4 {
        let dir = Self::dir(self.yaw, self.pitch);
        Mat4::look_to_rh(self.pos, dir, Vec3::Y)
    }

    pub fn proj(&self, aspect: f32) -> Mat4 {
        Mat4::perspective_rh_gl(self.fov_y.to_radians(), aspect, self.z_near, self.z_far)
    }

    pub fn dir(yaw: f32, pitch: f32) -> Vec3 {
        let (sy, cy) = yaw.sin_cos();
        let (sp, cp) = pitch.sin_cos();
        Vec3::new(cy * cp, sp, sy * cp)
    }
}

impl<'w> FpsRenderer<'w> {
    pub fn new(window: &'w Window, scene: &FpsScene) -> Result<Self> {
        pollster::block_on(Self::new_async(window, scene))
    }

    async fn new_async(window: &'w Window, scene: &FpsScene) -> Result<Self> {
        let size = window.inner_size();
        let instance = wgpu::Instance::default();
        let surface = instance.create_surface(window)?;
        let adapter = instance
            .request_adapter(&wgpu::RequestAdapterOptions {
                power_preference: wgpu::PowerPreference::HighPerformance,
                compatible_surface: Some(&surface),
                force_fallback_adapter: false,
            })
            .await
            .expect("No adapter");
        let required_limits = adapter.limits();
        let (device, queue) = adapter
            .request_device(
                &wgpu::DeviceDescriptor {
                    label: Some("device"),
                    required_features: wgpu::Features::empty(),
                    required_limits,
                },
                None,
            )
            .await?;
        let caps = surface.get_capabilities(&adapter);
        let format = caps.formats.iter().copied().find(|f| f.is_srgb()).unwrap_or(caps.formats[0]);
        let present_mode = wgpu::PresentMode::Fifo;
        let alpha_mode = caps.alpha_modes[0];

        let mut config = wgpu::SurfaceConfiguration {
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            format,
            width: size.width.max(1),
            height: size.height.max(1),
            present_mode,
            alpha_mode,
            view_formats: vec![],
            desired_maximum_frame_latency: 2,
        };
        surface.configure(&device, &config);

        // depth
        let (depth_tex, depth_view) = create_depth(&device, config.width, config.height);

        // pipeline
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("lighting"),
            source: wgpu::ShaderSource::Wgsl(include_str!("../shaders/lighting_simple.wgsl").into()),
        });

        let v_layout = wgpu::VertexBufferLayout {
            array_stride: std::mem::size_of::<Vertex>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &wgpu::vertex_attr_array![0=>Float32x3,1=>Float32x3,2=>Float32x2],
        };

        let i_layout = wgpu::VertexBufferLayout {
            array_stride: std::mem::size_of::<Instance>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Instance,
            attributes: &wgpu::vertex_attr_array![
                3=>Float32x3, // pos
                4=>Float32x3, // scale
                5=>Float32,   // rotY
                6=>Float32x3  // color
            ],
        };

        let cam_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("cam-layout"),
            entries: &[wgpu::BindGroupLayoutEntry{
                binding: 0,
                visibility: wgpu::ShaderStages::VERTEX,
                ty: wgpu::BindingType::Buffer{
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None
                },
                count: None
            }],
        });

        // Create texture bind group layout
        let texture_bind_group_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("texture_bind_group_layout"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                    count: None,
                },
            ],
        });

        // Create texture sampler
        let texture_sampler = device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("texture_sampler"),
            address_mode_u: wgpu::AddressMode::ClampToEdge,
            address_mode_v: wgpu::AddressMode::ClampToEdge,
            address_mode_w: wgpu::AddressMode::ClampToEdge,
            mag_filter: wgpu::FilterMode::Linear,
            min_filter: wgpu::FilterMode::Linear,
            mipmap_filter: wgpu::FilterMode::Linear,
            ..Default::default()
        });

        // Create lights bind group layout
        let lights_bind_group_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("lights-bind-group-layout"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
            ],
        });

        // Create pipeline layout with camera, texture and lights bind group layouts
        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("pipeline-layout"),
            bind_group_layouts: &[&cam_layout, &texture_bind_group_layout, &lights_bind_group_layout],
            push_constant_ranges: &[],
        });

        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: "vs_main",
                buffers: &[v_layout, i_layout],
                compilation_options: wgpu::PipelineCompilationOptions::default(),
            },
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: "fs_main",
                targets: &[Some(wgpu::ColorTargetState{
                    format,
                    blend: Some(wgpu::BlendState::REPLACE),
                    write_mask: wgpu::ColorWrites::ALL
                })],
                compilation_options: wgpu::PipelineCompilationOptions::default(),
            }),
            primitive: wgpu::PrimitiveState::default(),
            depth_stencil: Some(wgpu::DepthStencilState{
                format: wgpu::TextureFormat::Depth24Plus,
                depth_write_enabled: true,
                depth_compare: wgpu::CompareFunction::Less,
                stencil: wgpu::StencilState::default(),
                bias: wgpu::DepthBiasState::default(),
            }),
            multisample: wgpu::MultisampleState::default(),
            multiview: None,
        });

        // buffers
        let verts = cube_vertices();
        let vbo = device.create_buffer_init(&wgpu::util::BufferInitDescriptor{
            label: Some("vbo"),
            contents: bytemuck::cast_slice(&verts),
            usage: wgpu::BufferUsages::VERTEX
        });

        let instances: Vec<Instance> = scene.level.boxes.iter().map(Instance::from).collect();
        let inst_buf = device.create_buffer_init(&wgpu::util::BufferInitDescriptor{
            label: Some("inst"),
            contents: bytemuck::cast_slice(&instances),
            usage: wgpu::BufferUsages::VERTEX | wgpu::BufferUsages::COPY_DST
        });

        // camera
        let camera = Camera {
            pos: Vec3::from(scene.player.spawn),
            yaw: scene.player.yaw,
            pitch: scene.player.pitch,
            fov_y: 70.0,
            z_near: 0.1,
            z_far: 200.0,
        };
        let vp = camera.proj(config.width as f32 / config.height as f32) * camera.view();
        let cam_buf = device.create_buffer_init(&wgpu::util::BufferInitDescriptor{
            label: Some("cam-ubo"),
            contents: bytemuck::bytes_of(&CameraUBO{ view_proj: vp.to_cols_array_2d() }),
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST
        });
        let cam_bind = device.create_bind_group(&wgpu::BindGroupDescriptor{
            label: Some("cam-bind"),
            layout: &cam_layout,
            entries: &[wgpu::BindGroupEntry{ binding:0, resource: cam_buf.as_entire_binding() }],
        });

        // lights
        let mut lights_raw = LightsUBO {
            count: 0,
            _pad: [0; 3],
            lights: [LightRaw {
                position: [0.0; 3],
                _pad0: 0.0,
                color: [0.0; 3],
                intensity: 0.0,
                kind: 0,
                _pad1: [0; 3],
            }; 16],
        };
        
        // Fill lights from scene
        for (i, light) in scene.lights.iter().enumerate() {
            if i >= 16 {
                break;
            }
            lights_raw.count += 1;
            lights_raw.lights[i] = LightRaw {
                position: light.position,
                _pad0: 0.0,
                color: light.color,
                intensity: light.intensity,
                kind: if light.kind == "point" { 0 } else { 1 }, // 0 = point, 1 = directional
                _pad1: [0; 3],
            };
        }
        
        let lights_buf = device.create_buffer_init(&wgpu::util::BufferInitDescriptor{
            label: Some("lights-ubo"),
            contents: bytemuck::bytes_of(&lights_raw),
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST
        });
        let lights_bind = device.create_bind_group(&wgpu::BindGroupDescriptor{
            label: Some("lights-bind"),
            layout: &lights_bind_group_layout,
            entries: &[wgpu::BindGroupEntry{ binding:0, resource: lights_buf.as_entire_binding() }],
        });

        // Initialize textures map
        let textures = std::collections::HashMap::new();

        info!("FPS renderer ready: {}x{}", config.width, config.height);

        Ok(Self{
            surface,
            device,
            queue,
            config,
            size,
            color: wgpu::Color{
                r: scene.render.clear_color[0] as f64,
                g: scene.render.clear_color[1] as f64,
                b: scene.render.clear_color[2] as f64,
                a: scene.render.clear_color[3] as f64,
            },
            depth_tex,
            depth_view,
            pipeline,
            vbo,
            cam_buf,
            cam_bind,
            lights_buf,
            lights_bind,
            instances,
            inst_buf,
            texture_bind_group_layout,
            textures,
            texture_sampler,
            camera,
        })
    }

    pub fn resize(&mut self, new_size: winit::dpi::PhysicalSize<u32>) {
        if new_size.width == 0 || new_size.height == 0 { return; }
        self.size = new_size;
        self.config.width = new_size.width;
        self.config.height = new_size.height;
        self.surface.configure(&self.device, &self.config);
        let (dt, view) = create_depth(&self.device, self.config.width, self.config.height);
        self.depth_tex = dt;
        self.depth_view = view;
    }

    pub fn update_camera(&mut self) {
        let vp = self.camera.proj(self.config.width as f32 / self.config.height as f32) * self.camera.view();
        self.queue.write_buffer(&self.cam_buf, 0, bytemuck::bytes_of(&CameraUBO{ view_proj: vp.to_cols_array_2d() }));
    }

    pub fn update_lights(&mut self, lights: &[kengaai_scene_fps::Light]) {
        let mut lights_raw = LightsUBO {
            count: 0,
            _pad: [0; 3],
            lights: [LightRaw {
                position: [0.0; 3],
                _pad0: 0.0,
                color: [0.0; 3],
                intensity: 0.0,
                kind: 0,
                _pad1: [0; 3],
            }; 16],
        };
        
        // Fill lights from scene
        for (i, light) in lights.iter().enumerate() {
            if i >= 16 {
                break;
            }
            lights_raw.count += 1;
            lights_raw.lights[i] = LightRaw {
                position: light.position,
                _pad0: 0.0,
                color: light.color,
                intensity: light.intensity,
                kind: if light.kind == "point" { 0 } else { 1 }, // 0 = point, 1 = directional
                _pad1: [0; 3],
            };
        }
        
        self.queue.write_buffer(&self.lights_buf, 0, bytemuck::bytes_of(&lights_raw));
    }

    pub fn render(&mut self) -> Result<()> {
        info!("Начало отрисовки кадра");
        
        let frame = match self.surface.get_current_texture() {
            Ok(f) => f,
            Err(_) => {
                self.surface.configure(&self.device, &self.config);
                self.surface.get_current_texture()?
            }
        };
        
        info!("Получен кадровый буфер");
        let view = frame.texture.create_view(&wgpu::TextureViewDescriptor::default());
        let mut encoder = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor{ label: Some("encoder") });

        {
            info!("Начало рендер-пасса");
            let mut rp = encoder.begin_render_pass(&wgpu::RenderPassDescriptor{
                label: Some("main-pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment{
                    view: &view,
                    resolve_target: None,
                    ops: wgpu::Operations{ load: wgpu::LoadOp::Clear(self.color), store: wgpu::StoreOp::Store },
                })],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment{
                    view: &self.depth_view,
                    depth_ops: Some(wgpu::Operations{ load: wgpu::LoadOp::Clear(1.0), store: wgpu::StoreOp::Store }),
                    stencil_ops: None,
                }),
                occlusion_query_set: None,
                timestamp_writes: None,
            });
            
            info!("Установка pipeline и bind groups");
            rp.set_pipeline(&self.pipeline);
            rp.set_bind_group(0, &self.cam_bind, &[]);
            // Set default texture bind group (white texture) if no textures are loaded
            if let Some((_, ref bind_group)) = self.textures.values().next() {
                rp.set_bind_group(1, bind_group, &[]);
            }
            rp.set_bind_group(2, &self.lights_bind, &[]);
            
            info!("Отрисовка геометрии: {} инстансов", self.instances.len());
            rp.set_vertex_buffer(0, self.vbo.slice(..));
            rp.set_vertex_buffer(1, self.inst_buf.slice(..));
            rp.draw(0..36, 0..self.instances.len() as u32);
        }

        info!("Отправка команд и представление кадра");
        self.queue.submit([encoder.finish()]);
        frame.present();
        
        info!("Кадр отрисован успешно");
        Ok(())
    }

    pub fn set_clear(&mut self, c: [f32;4]) {
        self.color = wgpu::Color{ r: c[0] as f64, g: c[1] as f64, b: c[2] as f64, a: c[3] as f64 };
    }

    /// Load a texture from raw RGBA data
    pub fn load_texture(&mut self, name: String, width: u32, height: u32, rgba_data: &[u8]) -> Result<()> {
        // Check if texture already exists
        if self.textures.contains_key(&name) {
            return Ok(());
        }

        // Create texture
        let texture_size = wgpu::Extent3d {
            width,
            height,
            depth_or_array_layers: 1,
        };
        let texture = self.device.create_texture(&wgpu::TextureDescriptor {
            label: Some(&format!("texture_{}", name)),
            size: texture_size,
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8UnormSrgb,
            usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
            view_formats: &[],
        });

        // Upload texture data
        self.queue.write_texture(
            wgpu::ImageCopyTexture {
                texture: &texture,
                mip_level: 0,
                origin: wgpu::Origin3d::ZERO,
                aspect: wgpu::TextureAspect::All,
            },
            rgba_data,
            wgpu::ImageDataLayout {
                offset: 0,
                bytes_per_row: Some(4 * width),
                rows_per_image: Some(height),
            },
            texture_size,
        );

        // Create texture view
        let texture_view = texture.create_view(&wgpu::TextureViewDescriptor::default());

        // Create bind group
        let bind_group = self.device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some(&format!("bind_group_{}", name)),
            layout: &self.texture_bind_group_layout,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: wgpu::BindingResource::TextureView(&texture_view),
                },
                wgpu::BindGroupEntry {
                    binding: 1,
                    resource: wgpu::BindingResource::Sampler(&self.texture_sampler),
                },
            ],
        });

        // Store texture and bind group
        self.textures.insert(name, (texture, bind_group));
        Ok(())
    }

    /// Load a texture from an image file
    pub fn load_texture_from_file<P: AsRef<std::path::Path>>(&mut self, name: String, path: P) -> Result<()> {
        // Check if texture already exists
        if self.textures.contains_key(&name) {
            return Ok(());
        }

        // Load image
        let img = image::open(path)?.to_rgba8();
        let dimensions = img.dimensions();

        // Load texture
        self.load_texture(name, dimensions.0, dimensions.1, &img)
    }
}

fn create_depth(device: &wgpu::Device, width: u32, height: u32) -> (wgpu::Texture, wgpu::TextureView) {
    let tex = device.create_texture(&wgpu::TextureDescriptor{
        label: Some("depth"),
        size: wgpu::Extent3d{ width, height, depth_or_array_layers:1 },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format: wgpu::TextureFormat::Depth24Plus,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
        view_formats: &[],
    });
    let view = tex.create_view(&wgpu::TextureViewDescriptor::default());
    (tex, view)
}

/// Basic FPS controller state and update helpers
pub struct FpsController {
    pub move_speed: f32,
    pub run_speed: f32,
    pub mouse_sensitivity: f32,
    pub velocity: Vec3,
    pub mouse_delta: Vec2,
    pub running: bool,
    pub forward: bool,
    pub back: bool,
    pub left: bool,
    pub right: bool,
}

impl FpsController {
    pub fn new(move_speed: f32, run_speed: f32) -> Self {
        Self {
            move_speed,
            run_speed,
            mouse_sensitivity: 0.12,
            velocity: Vec3::ZERO,
            mouse_delta: Vec2::ZERO,
            running: false,
            forward: false, back: false, left: false, right: false,
        }
    }

    pub fn step(&mut self, cam: &mut Camera, dt: f32) {
        // look
        cam.yaw += self.mouse_delta.x * self.mouse_sensitivity * dt;
        cam.pitch += -self.mouse_delta.y * self.mouse_sensitivity * dt;
        cam.pitch = cam.pitch.clamp(-1.5, 1.5);
        self.mouse_delta = Vec2::ZERO;

        // move
        let speed = if self.running { self.run_speed } else { self.move_speed };
        let dir = Camera::dir(cam.yaw, cam.pitch);
        let forward = Vec3::new(dir.x, 0.0, dir.z).normalize_or_zero();
        let right = forward.cross(Vec3::Y).normalize_or_zero();

        let mut wish = Vec3::ZERO;
        if self.forward { wish += forward; }
        if self.back    { wish -= forward; }
        if self.left    { wish -= right; }
        if self.right   { wish += right; }
        if wish.length_squared() > 0.0 {
            wish = wish.normalize() * speed;
        }
        cam.pos += wish * dt;
    }
}
