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
}

fn cube_vertices() -> Vec<Vertex> {
    // Unit cube centered at origin
    let p = [
        // positions
        [-1.0, -1.0,  1.0], [ 1.0, -1.0,  1.0], [ 1.0,  1.0,  1.0], [-1.0,  1.0,  1.0], // front
        [-1.0, -1.0, -1.0], [ 1.0, -1.0, -1.0], [ 1.0,  1.0, -1.0], [-1.0,  1.0, -1.0], // back
    ];
    let faces: [([usize; 4], [f32; 3]); 6] = [
        ([0,1,2,3], [0.0, 0.0, 1.0]), // front
        ([5,4,7,6], [0.0, 0.0,-1.0]), // back
        ([4,0,3,7], [-1.0, 0.0,0.0]), // left
        ([1,5,6,2], [ 1.0, 0.0,0.0]), // right
        ([3,2,6,7], [ 0.0, 1.0,0.0]), // top
        ([4,5,1,0], [ 0.0,-1.0,0.0]), // bottom
    ];
    let mut v = Vec::with_capacity(36);
    for (idx, n) in faces {
        let i0 = p[idx[0]];
        let i1 = p[idx[1]];
        let i2 = p[idx[2]];
        let i3 = p[idx[3]];
        let tri = [
            Vertex { pos: i0, normal: n },
            Vertex { pos: i1, normal: n },
            Vertex { pos: i2, normal: n },
            Vertex { pos: i0, normal: n },
            Vertex { pos: i2, normal: n },
            Vertex { pos: i3, normal: n },
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
            rot_y: b.rotY,
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

pub struct FpsRenderer {
    surface: wgpu::Surface<'static>,
    device: wgpu::Device,
    queue: wgpu::Queue,
    config: wgpu::SurfaceConfiguration,
    size: winit::dpi::PhysicalSize<u32>,
    color: wgpu::Color,

    depth_tex: wgpu::Texture,
    depth_view: wgpu::TextureView,

    pipeline: wgpu::RenderPipeline,
    vbo: wgpu::Buffer,
    ibo: wgpu::Buffer,
    cam_buf: wgpu::Buffer,
    cam_bind: wgpu::BindGroup,

    instances: Vec<Instance>,
    inst_buf: wgpu::Buffer,

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

impl FpsRenderer {
    pub fn new(window: &Window, scene: &FpsScene) -> Result<Self> {
        pollster::block_on(Self::new_async(window, scene))
    }

    async fn new_async(window: &Window, scene: &FpsScene) -> Result<Self> {
        let size = window.inner_size();
        let instance = wgpu::Instance::default();
        let surface = unsafe { instance.create_surface(window)? };
        let adapter = instance
            .request_adapter(&wgpu::RequestAdapterOptions {
                power_preference: wgpu::PowerPreference::HighPerformance,
                compatible_surface: Some(&surface),
                force_fallback_adapter: false,
            })
            .await
            .expect("No adapter");
        let (device, queue) = adapter
            .request_device(&wgpu::DeviceDescriptor::default(), None)
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
        };
        surface.configure(&device, &config);

        // depth
        let (depth_tex, depth_view) = create_depth(&device, config.width, config.height);

        // pipeline
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("box3d"),
            source: wgpu::ShaderSource::Wgsl(include_str!("../shaders/box3d.wgsl").into()),
        });

        let v_layout = wgpu::VertexBufferLayout {
            array_stride: std::mem::size_of::<Vertex>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &wgpu::vertex_attr_array![0=>Float32x3,1=>Float32x3],
        };

        let i_layout = wgpu::VertexBufferLayout {
            array_stride: std::mem::size_of::<Instance>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Instance,
            attributes: &wgpu::vertex_attr_array![
                2=>Float32x3, // pos
                3=>Float32x3, // scale
                4=>Float32,   // rotY
                5=>Float32x3  // color
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

        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("pipeline-layout"),
            bind_group_layouts: &[&cam_layout],
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
            cache: None,
        });

        // buffers
        let verts = cube_vertices();
        let vbo = device.create_buffer_init(&wgpu::util::BufferInitDescriptor{
            label: Some("vbo"),
            contents: bytemuck::cast_slice(&verts),
            usage: wgpu::BufferUsages::VERTEX
        });

        let instances: Vec<Instance> = scene.level.boxes.iter().map(Instance::from).collect();
        let ibo = device.create_buffer_init(&wgpu::util::BufferInitDescriptor{
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

        info!("FPS renderer ready: {}x{}", config.width, config.height);

        Ok(Self{
            surface,
            device,
            queue,
            config,
            size,
            color: wgpu::Color{
                r: scene.render.clearColor[0] as f64 as f32 as f64 as f32 as f64 as f32, // safe coercion
                g: scene.render.clearColor[1],
                b: scene.render.clearColor[2],
                a: scene.render.clearColor[3],
            },
            depth_tex,
            depth_view,
            pipeline,
            vbo,
            ibo,
            cam_buf,
            cam_bind,
            instances,
            inst_buf: ibo,
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

    pub fn render(&mut self) -> Result<()> {
        let frame = match self.surface.get_current_texture() {
            Ok(f) => f,
            Err(_) => {
                self.surface.configure(&self.device, &self.config);
                self.surface.get_current_texture()?
            }
        };
        let view = frame.texture.create_view(&wgpu::TextureViewDescriptor::default());
        let mut encoder = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor{ label: Some("encoder") });

        {
            let mut rp = encoder.begin_render_pass(&wgpu::RenderPassDescriptor{
                label: Some("main-pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment{
                    view: &view,
                    resolve_target: None,
                    ops: wgpu::Operations{ load: wgpu::LoadOp::Clear(self.color), store: true },
                })],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment{
                    view: &self.depth_view,
                    depth_ops: Some(wgpu::Operations{ load: wgpu::LoadOp::Clear(1.0), store: true }),
                    stencil_ops: None,
                }),
                occlusion_query_set: None,
                timestamp_writes: None,
            });
            rp.set_pipeline(&self.pipeline);
            rp.set_bind_group(0, &self.cam_bind, &[]);
            rp.set_vertex_buffer(0, self.vbo.slice(..));
            rp.set_vertex_buffer(1, self.inst_buf.slice(..));
            rp.draw(0..36, 0..self.instances.len() as u32);
        }

        self.queue.submit([encoder.finish()]);
        frame.present();
        Ok(())
    }

    pub fn set_clear(&mut self, c: [f32;4]) {
        self.color = wgpu::Color{ r: c[0], g: c[1], b: c[2], a: c[3] };
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
