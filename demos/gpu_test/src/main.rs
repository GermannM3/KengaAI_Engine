use anyhow::Result;
use kengaai_fps::{FpsController, FpsRenderer};
use kengaai_scene_fps::load_scene;
use log::{error, info, warn};
use std::env;
use std::time::Instant;
use winit::{
    event::{DeviceEvent, ElementState, Event, MouseButton, WindowEvent},
    event_loop::EventLoop,
    keyboard::{KeyCode, PhysicalKey},
};

/// Тест совместимости GPU с движком KengaAI
/// Проверяет минимальные требования к оборудованию
fn main() -> Result<()> {
    env_logger::init();
    
    info!("=== KENGAIA ENGINE GPU COMPATIBILITY TEST ===");
    info!("System: Windows 10/11");
    info!("Architecture: x86_64");
    info!("===============================================");
    
    // Проверка наличия необходимых компонентов
    check_system_requirements()?;
    
    // Создание минимального тестового уровня
    let scene = create_test_scene();
    
    info!("Creating test window...");
    let event_loop = EventLoop::new().expect("event loop");
    let window = winit::window::WindowBuilder::new()
        .with_title("KengaAI Engine - GPU Compatibility Test")
        .with_inner_size(winit::dpi::LogicalSize::new(800.0, 600.0))
        .build(&event_loop)
        .expect("window");
    
    window.set_cursor_visible(false);
    window.set_cursor_grab(winit::window::CursorGrabMode::Confined).ok();
    
    let window: &'static winit::window::Window = Box::leak(Box::new(window));
    
    info!("Initializing renderer...");
    let mut renderer = match FpsRenderer::new(window, &scene) {
        Ok(r) => {
            info!("✓ Renderer initialized successfully");
            r
        },
        Err(e) => {
            error!("✗ Failed to initialize renderer: {:?}", e);
            show_hardware_recommendations();
            return Err(e.into());
        }
    };
    
    info!("Setting up test controller...");
    let mut ctrl = FpsController::new(4.5, 7.5);
    renderer.set_clear([0.1, 0.2, 0.3, 1.0]);
    
    let start = Instant::now();
    let mut last = start;
    
    info!("=== STARTING RENDER LOOP ===");
    info!("If you see a colored screen with a cube, GPU is compatible");
    info!("If screen stays white or app crashes, GPU needs upgrade");
    info!("Press ESC to exit");
    
    let win_id = window.id();
    event_loop.run(move |event, target| {
        match event {
            Event::WindowEvent { event, window_id } if window_id == win_id => {
                match event {
                    WindowEvent::CloseRequested => {
                        info!("Close requested");
                        target.exit();
                    },
                    WindowEvent::Resized(ns) => {
                        info!("Window resized to {}x{}", ns.width, ns.height);
                        renderer.resize(ns);
                    },
                    WindowEvent::ScaleFactorChanged { .. } => { /* handled via Resized */ },
                    WindowEvent::MouseInput { state, button, .. } => {
                        if button == MouseButton::Left && state.is_pressed() {
                            info!("Left mouse button pressed");
                        }
                    }
                    WindowEvent::KeyboardInput { event, .. } => {
                        let pressed = event.state == ElementState::Pressed;
                        match event.physical_key {
                            PhysicalKey::Code(KeyCode::Escape) if pressed => {
                                info!("ESC pressed - exiting test");
                                target.exit();
                            },
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
                
                match renderer.render() {
                    Ok(()) => {
                        // Success - GPU can handle rendering
                    },
                    Err(e) => {
                        error!("Render error: {:?}", e);
                        show_hardware_recommendations();
                        target.exit();
                    }
                }
            }
            _ => {}
        }
    })?;
    
    Ok(())
}

/// Проверяет минимальные системные требования
fn check_system_requirements() -> Result<()> {
    info!("Checking system requirements...");
    
    // Проверка Rust
    let rust_version = std::process::Command::new("rustc")
        .arg("--version")
        .output()
        .map(|output| String::from_utf8_lossy(&output.stdout).to_string())
        .unwrap_or_else(|_| "Not found".to_string());
    
    if rust_version.contains("Not found") {
        error!("Rust compiler not found!");
        return Err(anyhow::anyhow!("Rust is required to run this test"));
    }
    
    info!("Rust: {}", rust_version.trim());
    
    // Проверка Cargo
    let cargo_version = std::process::Command::new("cargo")
        .arg("--version")
        .output()
        .map(|output| String::from_utf8_lossy(&output.stdout).to_string())
        .unwrap_or_else(|_| "Not found".to_string());
    
    if cargo_version.contains("Not found") {
        error!("Cargo not found!");
        return Err(anyhow::anyhow!("Cargo is required to run this test"));
    }
    
    info!("Cargo: {}", cargo_version.trim());
    
    Ok(())
}

/// Создает минимальную тестовую сцену
fn create_test_scene() -> kengaai_scene_fps::FpsScene {
    use kengaai_scene_fps::*;
    
    FpsScene {
        meta: Meta {
            schema: "KengaFPSSceneV0".to_string(),
            version: "0.1.0".to_string(),
            name: "gpu_test".to_string(),
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
                    size: [5.0, 0.5, 5.0],
                    rot_y: 0.0,
                    color: [0.3, 0.3, 0.4],
                    texture: None,
                },
                BoxDef {
                    pos: [0.0, 1.0, 0.0],
                    size: [1.0, 1.0, 1.0],
                    rot_y: 0.0,
                    color: [1.0, 0.2, 0.2],
                    texture: None,
                }
            ],
            meshes: vec![],
        },
        lights: vec![],
        particles: vec![],
        enemies: vec![],
        sounds: vec![],
        triggers: vec![],
        goals: None,
    }
}

/// Показывает рекомендации по апгрейду оборудования
fn show_hardware_recommendations() {
    error!("===================================================");
    error!("HARDWARE COMPATIBILITY ISSUE DETECTED");
    error!("===================================================");
    error!("");
    error!("Your GPU may not support required features:");
    error!("- Intel HD Graphics 4000 (2012) has limited OpenGL support");
    error!("- Modern shaders require OpenGL 4.3+ features");
    error!("- Driver version may be outdated");
    error!("");
    error!("Recommendations:");
    error!("1. Update Intel HD Graphics drivers from intel.com");
    error!("2. Consider using dedicated GPU (NVIDIA/AMD)");
    error!("3. Reduce graphics quality in engine settings");
    error!("4. Use software rendering mode (if available)");
    error!("");
    error!("For development, consider upgrading hardware:");
    error!("- NVIDIA GTX 1050 or AMD RX 550 or better");
    error!("- Updated drivers");
    error!("===================================================");
}