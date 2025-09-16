#!/bin/bash

# KengaAI Engine Linux Run Script
# Замена для start_engine.bat

echo "=========================================="
echo "    KENGAIA ENGINE LINUX RUN MENU"
echo "=========================================="
echo

if [ $# -eq 0 ]; then
    echo "Использование: $0 <command> [args...]"
    echo
    echo "Команды:"
    echo "  build          - Собрать проект"
    echo "  minimal        - Запустить минимальное демо"
    echo "  kengaquest     - Запустить игру KengaQuest"
    echo "  html-demo      - Открыть HTML демо в браузере"
    echo "  check          - Проверить систему"
    echo "  clean          - Очистить сборку"
    echo
    echo "Примеры:"
    echo "  $0 minimal"
    echo "  $0 kengaquest"
    echo "  $0 html-demo"
    exit 1
fi

COMMAND=$1
shift

case $COMMAND in
    "build")
        echo "Сборка проекта..."
        ./build_linux.sh
        ;;

    "minimal")
        LEVEL=${1:-"assets/levels/minimal.json"}
        echo "Запуск минимального демо с уровнем: $LEVEL"
        if [ -f "./target/release/minimal-demo" ]; then
            ./target/release/minimal-demo "$LEVEL"
        else
            echo "Демо не собрано! Сначала выполните: $0 build"
            exit 1
        fi
        ;;

    "kengaquest")
        LEVEL=${1:-"assets/levels/kengaquest_main.json"}
        echo "Запуск KengaQuest с уровнем: $LEVEL"
        if [ -f "./target/release/kengaquest" ]; then
            ./target/release/kengaquest "$LEVEL"
        else
            echo "Игра не собрана! Сначала выполните: $0 build"
            exit 1
        fi
        ;;

    "html-demo")
        echo "Открытие HTML демо..."
        if command -v firefox &> /dev/null; then
            firefox kengaquest_ultimate.html &
        elif command -v chromium &> /dev/null; then
            chromium kengaquest_ultimate.html &
        elif command -v google-chrome &> /dev/null; then
            google-chrome kengaquest_ultimate.html &
        else
            echo "Браузер не найден. Откройте вручную: kengaquest_ultimate.html"
        fi
        ;;

    "check")
        echo "Проверка системы..."
        echo "Rust: $(rustc --version)"
        echo "Cargo: $(cargo --version)"
        echo "OS: $(uname -a)"
        echo "Display: $DISPLAY"

        if [ -n "$DISPLAY" ]; then
            echo "✓ Графическое окружение доступно"
        else
            echo "✗ Графическое окружение не доступно"
        fi

        if pkg-config --exists alsa; then
            echo "✓ ALSA (звук) найден"
        else
            echo "✗ ALSA (звук) не найден"
        fi
        ;;

    "clean")
        echo "Очистка сборки..."
        cargo clean
        echo "✓ Сборка очищена"
        ;;

    *)
        echo "Неизвестная команда: $COMMAND"
        echo "Используйте: $0 без параметров для справки"
        exit 1
        ;;
esac
