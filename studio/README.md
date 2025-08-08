# KengaAI Studio (Desktop)

Настольное приложение для генерации FPS‑уровней (JSON) под KengaAI Engine.

Цвета: синий (#1E88E5), зелёный (#2ECC71), чёрный (#0A0E12), белый (#FFFFFF).

## Локальная сборка

Требуется: Rust stable, Node.js 18+, PNPM 9, инструменты платформы.

- Windows: Visual Studio Build Tools + WiX Toolset (для MSI)
- macOS: Xcode CLT
- Linux: зависимости GTK/WebKit (см. tauri.app)

\`\`\`bash
cd studio
pnpm install
pnpm build
pnpm tauri:build
\`\`\`

Результат:
- Windows MSI: studio/src-tauri/target/release/bundle/msi/
- macOS DMG: studio/src-tauri/target/release/bundle/dmg/
- Linux AppImage/DEB: studio/src-tauri/target/release/bundle/
