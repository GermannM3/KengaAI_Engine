# KengaAI Studio

Настольное приложение для генерации FPS‑уровней (JSON) под KengaAI Engine.

Цвета интерфейса: синий, зелёный, чёрный, белый.

## Установка и сборка

1) Установите Rust (stable), Node.js 18+, и Tauri Prerequisites:
- Windows: Visual Studio Build Tools + WiX Toolset (для MSI)
- macOS: Xcode Command Line Tools
- Linux: зависимости GTK/WebKit (см. https://tauri.app)

2) Сборка:
\`\`\`bash
cd studio
pnpm install # или npm/yarn
pnpm build
pnpm tauri:build
\`\`\`

Итоги:
- Windows: MSI в `studio/src-tauri/target/release/bundle/msi/`
- macOS: DMG в `.../bundle/dmg/`
- Linux: AppImage и DEB в `.../bundle/`
