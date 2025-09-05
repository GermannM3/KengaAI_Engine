#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
fn main() {
  let ctx = tauri::generate_context!();
  tauri::Builder::default()
    .run(ctx)
    .expect("error while running KengaAI Studio");
}
