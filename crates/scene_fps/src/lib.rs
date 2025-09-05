use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::Path;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FpsScene {
    pub meta: Meta,
    pub render: Render,
    pub player: Player,
    #[serde(default)]
    pub weapons: Vec<Weapon>,
    pub level: Level,
    #[serde(default)]
    pub lights: Vec<Light>,
    #[serde(default)]
    pub particles: Vec<ParticleSystem>,
    #[serde(default)]
    pub enemies: Vec<Enemy>,
    #[serde(default)]
    pub sounds: Vec<SoundSource>,
    #[serde(default)]
    pub triggers: Vec<Trigger>,
    #[serde(default)]
    pub goals: Option<Goals>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Meta {
    pub schema: String,   // "KengaFPSSceneV0"
    pub version: String,  // "0.1.0"
    pub name: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Render {
    #[serde(rename = "clearColor")]
    pub clear_color: [f32; 4],
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Player {
    pub spawn: [f32; 3],
    pub yaw: f32,
    pub pitch: f32,
    #[serde(rename = "move")]
    pub r#move: Move,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Move {
    pub speed: f32,
    pub run: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Weapon {
    pub id: String,
    pub kind: String, // "hitscan" | "projectile"
    pub damage: f32,
    pub rate: f32,
    #[serde(default)]
    pub spread: Option<f32>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Level {
    #[serde(default)]
    pub boxes: Vec<BoxDef>,
    #[serde(default)]
    pub meshes: Vec<MeshDef>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BoxDef {
    pub pos: [f32; 3],
    pub size: [f32; 3],
    #[serde(rename = "rotY")]
    pub rot_y: f32,
    #[serde(default)]
    pub color: [f32; 3],
    #[serde(default)]
    pub texture: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MeshDef {
    pub pos: [f32; 3],
    pub scale: [f32; 3],
    #[serde(rename = "rotY")]
    pub rot_y: f32,
    pub file: String,
    #[serde(default)]
    pub material: Option<Material>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Material {
    #[serde(default)]
    pub color: [f32; 3],
    #[serde(default)]
    pub texture: Option<String>,
    #[serde(default)]
    pub metallic: f32,
    #[serde(default)]
    pub roughness: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Enemy {
    pub kind: String,
    pub spawn: [f32; 3],
    #[serde(default)]
    pub patrol: Vec<[f32; 3]>,
    #[serde(default)]
    pub physics: Option<PhysicsProperties>,
    #[serde(default)]
    pub behavior: Option<Behavior>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Behavior {
    #[serde(rename = "type")]
    pub type_: String, // "patrol", "chase", "flee", "idle"
    #[serde(default)]
    pub parameters: std::collections::HashMap<String, serde_json::Value>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhysicsProperties {
    #[serde(rename = "type")]
    pub type_: String, // "dynamic", "static", "kinematic"
    #[serde(default)]
    pub mass: f32,
    #[serde(default)]
    pub friction: f32,
    #[serde(default)]
    pub restitution: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SoundSource {
    pub position: [f32; 3],
    pub file: String,
    #[serde(default)]
    pub volume: f32,
    #[serde(default)]
    pub looping: bool,
    #[serde(default)]
    pub spatial: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Trigger {
    pub pos: [f32; 3],
    pub size: [f32; 3],
    #[serde(rename = "onEnter")]
    pub on_enter: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Goals {
    #[serde(default)]
    pub r#type: String, // "extract" | ...
    pub point: [f32; 3],
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Light {
    pub kind: String, // "point" | "directional"
    pub position: [f32; 3],
    pub color: [f32; 3],
    #[serde(default)]
    pub intensity: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ParticleSystem {
    pub position: [f32; 3],
    pub color: [f32; 3],
    pub count: u32,
    #[serde(default)]
    pub lifetime: f32,
    #[serde(default)]
    pub speed: f32,
    #[serde(default)]
    pub spread: f32,
}

/// Load scene JSON from path
pub fn load_scene<P: AsRef<Path>>(path: P) -> Result<FpsScene> {
    let s = fs::read_to_string(&path).with_context(|| format!("read {}", path.as_ref().display()))?;
    let scene: FpsScene = serde_json::from_str(&s).with_context(|| "parse scene json")?;
    Ok(scene)
}
