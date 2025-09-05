use anyhow::Result;
use kengaai_fps::{FpsController, FpsRenderer};
use kengaai_scene_fps::{FpsScene, Render, Player, Move, Level, BoxDef, MeshDef};
use log::info;
use std::time::Instant;
use winit::{
    event::{DeviceEvent, ElementState, Event, MouseButton, WindowEvent},
    event_loop::EventLoop,
    keyboard::{KeyCode, PhysicalKey},
};

fn main() -> Result<()> {
    env_logger::init();
    
    // Создаем минимальную сцену для тестирования
    let scene = FpsScene {
        meta: kengaai_scene_fps::Meta {
            schema: "KengaFPSSceneV0".to_string(),
            version: "0.1.0".to_string(),
            name: "test_scene".to_string(),
        },
        render: Render {
            clear_color: [0.1, 0.2, 0.3, 1.0],
        },
        player: Player {
            spawn: [0.0, 1.5, 4.0],
            yaw: 3.1415,
            pitch: 0.0,
            r#move: Move {
                speed: 4.5,
                run: 7.5,
            },
        },
        weapons: vec![],
        level: Level {
            boxes: vec![
                BoxDef {
                    pos: [0.0, 0.0, 0.0],
                    size: [10.0, 0.5, 10.0],
                    rot_y: 0.0,
                    color: [0.5, 0.5, 0.5],
                    texture: None,
                },
                BoxDef {
                    pos: [0.0, 1.0, -2.0],
                    size: [1.0, 1.0, 1.0],
                    rot_y: 0.0,
                    color: [1.0, 0.0, 0.0],
                    texture: None,
                },
            ],
            meshes: vec![],
        },
        lights: vec![],
        particles: vec![],
        enemies: vec![],
        sounds: vec![],
        triggers: vec![],
        goals: None,
    };
    
    info!("Создана тестовая сцена");
    
    let event_loop = EventLoop::new().expect("event loop");
    let window = winit::window::WindowBuilder::new()
        .with_title("KengaAI Engine - Test")
        .with_inner_size(winit::dpi::LogicalSize::new(1280.0, 720.0))
        .build(&event_loop)
        .expect("window");
    
    window.set_cursor_visible(false);
    window.set_cursor_grab(winit::window::CursorGrabMode::Confined).ok();
    
    // Ликуем окно до 'static для совместимости с wgpu surface lifetime
    let window: &'static winit::window::Window = Box::leak(Box::new(window));
    let mut renderer = FpsRenderer::new(window, &scene)?;
    
    let mut ctrl = FpsController::new(scene.player.r#move.speed, scene.player.r#move.run);
    renderer.set_clear(scene.render.clear_color);
    
    let start = Instant::now();
    let mut last = start;
    
    let win_id = window.id();
    event_loop.run(move |event, target| {
        match event {
            Event::WindowEvent { event, window_id } if window_id == win_id => {
                match event {
                    WindowEvent::CloseRequested => target.exit(),
                    WindowEvent::Resized(ns) => renderer.resize(ns),
                    WindowEvent::ScaleFactorChanged { .. } => { /* handled via Resized */ },
                    WindowEvent::MouseInput { state, button, .. } => {
                        if button == MouseButton::Left && state.is_pressed() {
                            info!("Выстрел!");
                        }
                    }
                    WindowEvent::KeyboardInput { event, .. } => {
                        let pressed = event.state == ElementState::Pressed;
                        match event.physical_key {
                            PhysicalKey::Code(KeyCode::Escape) if pressed => target.exit(),
                            PhysicalKey::Code(KeyCode::KeyW) => { ctrl.forward = pressed },
                            PhysicalKey::Code(KeyCode::KeyS) => { ctrl.back = pressed },
                            PhysicalKey::Code(KeyCode::KeyA) => { ctrl.left = pressed },
                            PhysicalKey::Code(KeyCode::KeyD) => { ctrl.right = pressed },
                            PhysicalKey::Code(KeyCode::ShiftLeft) => { ctrl.running = pressed },
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
            Event::WindowEvent { event: WindowEvent::RedrawRequested, window_id } if window_id == win_id => {
                let now = Instant::now();
                let dt = (now - last).as_secs_f32();
                last = now;
                
                ctrl.step(&mut renderer.camera, dt);
                renderer.update_camera();
                
                if let Err(e) = renderer.render() {
                    log::error!("render: {e:?}");
                }
            }
            _ => {}
        }
    })?;
    Ok(())
}