# Релиз и установщики

Чтобы собрать и опубликовать установщики Studio (MSI/DMG/AppImage) автоматически:

1. Убедитесь, что проект собирается локально:
   - Движок (Rust):
     \`\`\`
     rustup default stable
     cargo build --release
     \`\`\`
   - Studio:
     \`\`\`
     cd studio
     pnpm install
     pnpm build
     \`\`\`
2. Создайте тэг версии и отправьте в GitHub:
   \`\`\`
   git tag v0.2.0
   git push origin v0.2.0
   \`\`\`
3. GitHub Actions соберёт инсталляторы под Windows/macOS/Linux и приложит их к релизу.
   - Workflow: .github/workflows/release-studio.yml
   - Артефакты: MSI/DMG/AppImage/DEB в разделе Releases.

Примечания:
- Для локальной сборки Studio установите зависимости Tauri (см. studio/README.md).
- CI не требует секретов — сборка выполняется без приватных ключей.
