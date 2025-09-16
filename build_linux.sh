#!/bin/bash

# KengaAI Engine Linux Build Script
# Замена для build.bat

echo "=========================================="
echo "    KENGAIA ENGINE LINUX BUILD SCRIPT"
echo "=========================================="
echo

# Проверка Rust
if ! command -v rustc &> /dev/null; then
    echo "ERROR: Rust не установлен!"
    echo "Установите Rust: curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"
    exit 1
fi

if ! command -v cargo &> /dev/null; then
    echo "ERROR: Cargo не найден!"
    exit 1
fi

echo "✓ Rust найден: $(rustc --version)"
echo "✓ Cargo найден: $(cargo --version)"
echo

# Проверка зависимостей системы
echo "Проверка системных зависимостей..."
MISSING_DEPS=""

if ! pkg-config --exists alsa; then
    MISSING_DEPS="$MISSING_DEPS libalsa-devel"
fi

if ! pkg-config --exists x11; then
    MISSING_DEPS="$MISSING_DEPS libX11-devel"
fi

if [ ! -z "$MISSING_DEPS" ]; then
    echo "WARNING: Возможны проблемы со следующими зависимостями:"
    echo "$MISSING_DEPS"
    echo "Установите их через: sudo apt-get install $MISSING_DEPS"
    echo
fi

# Сборка проекта
echo "Сборка KengaAI Engine..."
echo

# Очистка
echo "Очистка предыдущей сборки..."
cargo clean

# Сборка всех компонентов
echo "Сборка crates..."
cargo build --release

if [ $? -eq 0 ]; then
    echo
    echo "✓ Сборка завершена успешно!"
    echo
    echo "Доступные демо:"
    echo "- ./target/release/minimal-demo assets/levels/minimal.json"
    echo "- ./target/release/kengaquest assets/levels/kengaquest_main.json"
    echo
    echo "Запуск HTML демо:"
    echo "- firefox kengaquest_ultimate.html"
    echo "- firefox kengaquest_demo.html"
else
    echo
    echo "✗ Ошибка сборки!"
    exit 1
fi
