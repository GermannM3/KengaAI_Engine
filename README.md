# KengaAI Engine

Модульный движок и настольная студия. Готов для тестов у геймдева.

- Движок (Rust): wgpu, демо FPS (чтение JSON уровня).
- Студия (Tauri): генерация уровней по описанию, экспорт JSON.
- Установщики Studio: MSI/DMG/AppImage собираются GitHub Actions на тэг.

## Запуск демо FPS

\`\`\`bash
cargo run -p kengaai-demo-fps -- assets/levels/example_fps.json
\`\`\`

Управление: WASD + мышь, Shift — бег, Esc — выход.

## Сборка Studio (локально)

\`\`\`bash
cd studio
pnpm install
pnpm build
pnpm tauri:build
\`\`\`

## Релиз Studio (CI)

\`\`\`bash
git tag v0.2.0
git push origin v0.2.0
\`\`\`

См. RELEASE.md.
