use anyhow::{bail, Result};
use kengaai_fps::{FpsController, FpsRenderer};
use kengaai_scene_fps::load_scene;
use log::{error, info};
use std::env;
use std::time::Instant;
use winit::{
    event::{DeviceEvent, ElementState, Event, MouseButton, WindowEvent},
    event_loop::EventLoop,
    keyboard::{KeyCode, PhysicalKey},
};

fn main() -> Result<()> {
    env_logger::init();
    let args: Vec<String> = env::args().collect();
    let path = args.get(1).cloned().unwrap_or_else(|| "assets/levels/example_fps.json".to_string());
    info!("Loading level: {}", path);
    let scene = load_scene(&path)?;

    let event_loop = EventLoop::new().expect("event loop");
    let window = winit::window::WindowBuilder::new()
        .with_title(format!("KengaAI FPS — {}", scene.meta.name))
        .with_inner_size(winit::dpi::LogicalSize::new(1280.0, 720.0))
        .build(&event_loop)
        .expect("window");
    window.set_cursor_visible(false);
    window.set_cursor_grab(winit::window::CursorGrabMode::Confined).ok();

    let mut renderer = FpsRenderer::new(&window, &scene)?;
    let mut ctrl = FpsController::new(scene.player.move_.speed, scene.player.move_.run);
    renderer.set_clear(scene.render.clearColor);

    let start = Instant::now();
    let mut last = start;

    event_loop.run(move |event, target| {
        match event {
            Event::WindowEvent { event, window_id } if window_id == window.id() => {
                match event {
                    WindowEvent::CloseRequested => target.exit(),
                    WindowEvent::Resized(ns) => renderer.resize(ns),
                    WindowEvent::ScaleFactorChanged{ new_inner_size, .. } => renderer.resize(*new_inner_size),
                    WindowEvent::MouseInput { state, button, .. } => {
                        if button == MouseButton::Left && state.is_pressed() {
                            // MVP: здесь можно добавить хитскан луч для попаданий
                            // пока — без стрельбы
                        }
                    }
                    WindowEvent::KeyboardInput { event, .. } => {
                        let pressed = event.state == ElementState::Pressed;
                        match event.physical_key {
                            PhysicalKey::Code(KeyCode::Escape) if pressed => target.exit(),
                            PhysicalKey::Code(KeyCode::KeyW) => ctrl.forward = pressed,
                            PhysicalKey::Code(KeyCode::KeyS) => ctrl.back = pressed,
                            PhysicalKey::Code(KeyCode::KeyA) => ctrl.left = pressed,
                            PhysicalKey::Code(KeyCode::KeyD) => ctrl.right = pressed,
                            PhysicalKey::Code(KeyCode::ShiftLeft) => ctrl.running = pressed,
                            _ => {}
                        }
                    }
                    _ => {}
                }
            }
            Event::DeviceEvent { event, .. } => {
                if let DeviceEvent::MouseMotion { delta } = event {
                    ctrl.mouse_delta.x += delta.0 as f32;
                    ctrl.mouse_delta.y += delta.1 as f32;
                }
            }
            Event::AboutToWait => {
                window.request_redraw();
            }
            Event::RedrawRequested(_) => {
                let now = Instant::now();
                let dt = (now - last).as_secs_f32();
                last = now;

                ctrl.step(&mut renderer.camera, dt);
                renderer.update_camera();

                if let Err(e) = renderer.render() {
                    error!("render: {e:?}");
                }
            }
            _ => {}
        }
    });
}
