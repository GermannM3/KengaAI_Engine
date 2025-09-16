#!/bin/bash

# KengaAI Engine Linux Diagnostic Script
# Замена для run_diagnostic.bat

echo "=========================================="
echo " KENGAIA ENGINE LINUX DIAGNOSTIC TOOL"
echo "=========================================="
echo

LOG_FILE="diagnostic_$(date +%Y%m%d_%H%M%S).log"
echo "Лог диагностики: $LOG_FILE"
echo

# Функция логирования
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $*" | tee -a "$LOG_FILE"
}

log "[1/6] Проверка системных требований..."
echo "----------------------------------------"

# Проверка Rust
if command -v rustc &> /dev/null; then
    log "✓ Rust установлен: $(rustc --version)"
else
    log "✗ Rust НЕ установлен!"
    log "Установите: curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"
fi

# Проверка Cargo
if command -v cargo &> /dev/null; then
    log "✓ Cargo доступен: $(cargo --version)"
else
    log "✗ Cargo НЕ найден!"
fi

# Проверка системных зависимостей
log ""
log "[2/6] Проверка системных зависимостей..."
echo "-----------------------------------------"

DEPS_OK=true

if pkg-config --exists alsa; then
    log "✓ ALSA (звук) найден"
else
    log "✗ ALSA (звук) НЕ найден"
    DEPS_OK=false
fi

if pkg-config --exists x11; then
    log "✓ X11 (графика) найден"
else
    log "✗ X11 (графика) НЕ найден"
    DEPS_OK=false
fi

if pkg-config --exists xkbcommon; then
    log "✓ XKB Common найден"
else
    log "✗ XKB Common НЕ найден"
    DEPS_OK=false
fi

if pkg-config --exists wayland-client; then
    log "✓ Wayland найден (опционально)"
else
    log "⚠ Wayland НЕ найден (работа через X11)"
fi

# Проверка графического окружения
log ""
log "[3/6] Проверка графического окружения..."
echo "-----------------------------------------"

if [ -n "$DISPLAY" ]; then
    log "✓ X11 дисплей доступен: $DISPLAY"
else
    log "✗ Графическое окружение НЕ доступно"
    DEPS_OK=false
fi

# Проверка Vulkan/OpenGL
log ""
log "[4/6] Проверка графических драйверов..."
echo "-----------------------------------------"

if command -v vulkaninfo &> /dev/null; then
    log "✓ Vulkan утилиты доступны"
    vulkaninfo --summary | grep -E "(deviceName|driverName|driverVersion)" | head -3 >> "$LOG_FILE" 2>/dev/null
else
    log "⚠ Vulkan утилиты НЕ найдены (опционально)"
fi

if command -v glxinfo &> /dev/null; then
    log "✓ OpenGL утилиты доступны"
    glxinfo | grep -E "(OpenGL version|renderer|vendor)" | head -3 >> "$LOG_FILE" 2>/dev/null
else
    log "⚠ OpenGL утилиты НЕ найдены"
fi

# Тест сборки
log ""
log "[5/6] Тест сборки проекта..."
echo "------------------------------"

if cargo check -p minimal-demo &>> "$LOG_FILE"; then
    log "✓ Сборка проекта успешна"
else
    log "✗ Ошибки сборки! Проверьте лог: $LOG_FILE"
    DEPS_OK=false
fi

# Финальный отчет
log ""
log "[6/6] Финальный отчет..."
echo "------------------------"

if [ "$DEPS_OK" = true ]; then
    log "✓ СИСТЕМА ГОТОВА К РАБОТЕ!"
    log ""
    log "Рекомендуемые команды:"
    log "  ./run_linux.sh build          - Собрать проект"
    log "  ./run_linux.sh minimal        - Запустить минимальное демо"
    log "  ./run_linux.sh kengaquest     - Запустить игру"
    log "  ./run_linux.sh html-demo      - Открыть HTML демо"
else
    log "⚠ ОБНАРУЖЕНЫ ПРОБЛЕМЫ!"
    log ""
    log "Рекомендации:"
    log "1. Установите недостающие зависимости"
    log "2. Проверьте логи в файле: $LOG_FILE"
    log "3. Для Ubuntu/Debian: sudo apt-get install libasound2-dev libx11-dev libxkbcommon-dev"
    log "4. Для ALT Linux: sudo apt-get install libalsa-devel libX11-devel libxkbcommon-devel"
fi

log ""
log "=========================================="
log "    ДИАГНОСТИКА ЗАВЕРШЕНА"
log "=========================================="
log ""
log "Подробный лог сохранен в: $LOG_FILE"

echo
echo "=========================================="
echo "    ДИАГНОСТИКА ЗАВЕРШЕНА"
echo "=========================================="
echo
echo "Подробный лог: $LOG_FILE"
