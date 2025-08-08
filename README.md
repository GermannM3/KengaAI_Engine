# KengaAI Engine

Модульный движок с no‑code студией.

- Движок (Rust): рендер wgpu, 2D/3D, FPS‑runtime (MVP).
- Студия (Tauri): генерация уровней FPS по текстовому описанию, валидация и экспорт JSON.
- Цветовая тема студии: синий, зелёный, чёрный, белый.

## Быстрый старт — Движок

\`\`\`bash
rustup default stable
cargo build --release

# FPS демо:
cargo run -p kengaai-demo-fps -- assets/levels/example_fps.json
\`\`\`

Управление: WASD + мышь, Shift — бег, Esc — выход.

## Быстрый старт — Студия

\`\`\`bash
cd studio
pnpm install
pnpm dev           # во время разработки
pnpm build && pnpm tauri:build
\`\`\`

- Windows MSI / macOS DMG / Linux AppImage будут в `studio/src-tauri/target/release/bundle/`.

## Генерация уровней

Откройте Studio, опишите уровень, скачайте JSON и поместите в `assets/levels/`. Запустите:
\`\`\`bash
cargo run -p kengaai-demo-fps -- assets/levels/<your>.json
\`\`\`

## Структура
- crates/fps — FPS 3D рендер и контроллер
- crates/scene_fps — схема сцен FPS (JSON)
- demos/fps — запуск уровня из JSON
- studio — настольное приложение (Tauri) с установщиками
- assets/levels — примеры уровней

Лицензия: MIT
