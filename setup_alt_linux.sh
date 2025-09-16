#!/bin/bash

# KengaAI Engine ALT Linux Setup Script
# Автоматическая установка всех зависимостей

echo "=========================================="
echo " KENGAIA ENGINE ALT LINUX SETUP"
echo "=========================================="
echo

set -e  # Остановить при ошибке

# Функция логирования
log() {
    echo "[$(date +%H:%M:%S)] $*"
}

log "Обновление списка пакетов..."
sudo apt-get update

# Проверка и установка Rust
log "Проверка Rust..."
if ! command -v rustc &> /dev/null; then
    log "Установка Rust..."
    sudo apt-get install -y rust rust-cargo
    log "✓ Rust установлен"
else
    log "✓ Rust уже установлен: $(rustc --version)"
fi

# Системные зависимости
log "Установка системных зависимостей..."
DEPENDENCIES=(
    "libalsa-devel"
    "libX11-devel"
    "libXcursor-devel"
    "libXi-devel"
    "libXrandr-devel"
    "libxkbcommon-devel"
    "libxcb-devel"
    "libXau-devel"
    "libXext-devel"
    "libXfixes-devel"
    "libXrender-devel"
    "xorg-proto-devel"
)

for dep in "${DEPENDENCIES[@]}"; do
    if apt-cache show "$dep" &> /dev/null; then
        log "Установка $dep..."
        sudo apt-get install -y "$dep"
    else
        log "⚠ Пакет $dep не найден в репозиториях"
    fi
done

# Опциональные зависимости
log "Установка опциональных зависимостей..."
OPTIONAL_DEPS=(
    "vulkan-tools"      # Для диагностики Vulkan
    "mesa-vulkan-drivers"  # Vulkan драйверы
    "llvm"              # Для оптимизаций
    "clang"             # Альтернативный компилятор
)

for dep in "${OPTIONAL_DEPS[@]}"; do
    if apt-cache show "$dep" &> /dev/null; then
        log "Установка $dep..."
        sudo apt-get install -y "$dep" || log "⚠ Не удалось установить $dep"
    fi
done

# Проверка установки
log "Проверка установки..."

errors=0

if ! command -v rustc &> /dev/null; then
    log "✗ Rust не установлен!"
    ((errors++))
fi

if ! pkg-config --exists alsa; then
    log "✗ ALSA не найден!"
    ((errors++))
fi

if ! pkg-config --exists x11; then
    log "✗ X11 не найден!"
    ((errors++))
fi

if ! pkg-config --exists xkbcommon; then
    log "✗ XKB Common не найден!"
    ((errors++))
fi

# Финальный отчет
echo
echo "=========================================="
if [ $errors -eq 0 ]; then
    echo "✓ УСТАНОВКА ЗАВЕРШЕНА УСПЕШНО!"
    echo
    echo "Следующие шаги:"
    echo "1. Перезагрузите систему (рекомендуется)"
    echo "2. Запустите диагностику: ./diagnostic_linux.sh"
    echo "3. Соберите проект: ./build_linux.sh"
    echo "4. Запустите демо: ./run_linux.sh minimal"
else
    echo "⚠ ОБНАРУЖЕНЫ ПРОБЛЕМЫ ($errors ошибок)"
    echo
    echo "Рекомендации:"
    echo "1. Проверьте интернет соединение"
    echo "2. Попробуйте: sudo apt-get update && sudo apt-get upgrade"
    echo "3. Свяжитесь с поддержкой ALT Linux"
fi
echo "=========================================="

exit $errors
