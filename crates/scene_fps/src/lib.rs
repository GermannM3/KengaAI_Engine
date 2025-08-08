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
    pub enemies: Vec<Enemy>,
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
    pub clearColor: [f32; 4],
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
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BoxDef {
    pub pos: [f32; 3],
    pub size: [f32; 3],
    pub rotY: f32,
    pub color: [f32; 3],
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Enemy {
    pub kind: String,
    pub spawn: [f32; 3],
    #[serde(default)]
    pub patrol: Vec<[f32; 3]>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Trigger {
    pub pos: [f32; 3],
    pub size: [f32; 3],
    pub onEnter: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Goals {
    #[serde(default)]
    pub r#type: String, // "extract" | ...
    pub point: [f32; 3],
}

/// Load scene JSON from path
pub fn load_scene<P: AsRef<Path>>(path: P) -> Result<FpsScene> {
    let s = fs::read_to_string(&path).with_context(|| format!("read {}", path.as_ref().display()))?;
    let scene: FpsScene = serde_json::from_str(&s).with_context(|| "parse scene json")?;
    Ok(scene)
}
