#!/bin/bash

# Скрипт загрузки модели GigaChat-20B-A3B-base для KengaAI Studio
# Замена зависимостей на ALT Linux совместимые

echo "=========================================="
echo " ЗАГРУЗКА GIGACHAT-20B-A3B-BASE"
echo "=========================================="
echo

MODEL_DIR="models/GigaChat-20B-A3B-base"
MODEL_REPO="ai-sage/GigaChat-20B-A3B-base"

# Проверка Python
if ! command -v python3 &> /dev/null; then
    echo "❌ Python3 не найден!"
    echo "Установите: sudo apt-get install python3"
    exit 1
fi

echo "✓ Python3 найден: $(python3 --version)"

# Проверка pip
if ! command -v pip3 &> /dev/null; then
    echo "❌ pip3 не найден!"
    echo "Установите: sudo apt-get install python3-pip"
    exit 1
fi

echo "✓ pip3 найден"

# Создание директории для модели
mkdir -p "$MODEL_DIR"

# Установка необходимых Python пакетов
echo
echo "Установка Python зависимостей..."
echo "---------------------------------"

REQUIRED_PACKAGES=(
    "torch"
    "transformers>=4.47"
    "accelerate"
    "huggingface_hub"
    "sentencepiece"
    "protobuf"
    "numpy"
)

for package in "${REQUIRED_PACKAGES[@]}"; do
    echo "Установка $package..."
    if pip3 install "$package" --user; then
        echo "✓ $package установлен"
    else
        echo "⚠ Ошибка установки $package"
    fi
done

# Проверка установки
echo
echo "Проверка установки зависимостей..."
echo "-----------------------------------"

python3 -c "
import sys
packages = ['torch', 'transformers', 'huggingface_hub', 'sentencepiece']
missing = []

for pkg in packages:
    try:
        __import__(pkg)
        print(f'✓ {pkg}')
    except ImportError:
        missing.append(pkg)
        print(f'✗ {pkg}')

if missing:
    print(f'\\n❌ Недостающие пакеты: {missing}')
    sys.exit(1)
else:
    print('\\n✅ Все зависимости установлены')
" || exit 1

# Загрузка модели
echo
echo "Загрузка модели..."
echo "------------------"

python3 -c "
from huggingface_hub import snapshot_download
import os

model_path = '$MODEL_DIR'
repo_id = '$MODEL_REPO'

print(f'Загрузка модели {repo_id}...')
print(f'Путь сохранения: {model_path}')

try:
    snapshot_download(
        repo_id=repo_id,
        local_dir=model_path,
        local_dir_use_symlinks=False,
        resume_download=True
    )
    print('✅ Модель успешно загружена!')
except Exception as e:
    print(f'❌ Ошибка загрузки: {e}')
    exit(1)
"

if [ $? -eq 0 ]; then
    echo
    echo "=========================================="
    echo " ✅ МОДЕЛЬ УСПЕШНО ЗАГРУЖЕНА!"
    echo "=========================================="
    echo
    echo "Модель сохранена в: $MODEL_DIR"
    echo
    echo "Для использования в студии:"
    echo "1. Запустите студию: npm run tauri:dev"
    echo "2. AI помощник будет автоматически использовать модель"
    echo
    echo "Размер модели: ~40GB"
    echo "Требования к памяти для работы:"
    echo "- CPU: минимум 8GB RAM"
    echo "- GPU: минимум 8GB VRAM (рекомендуется)"
else
    echo
    echo "=========================================="
    echo " ❌ ОШИБКА ЗАГРУЗКИ МОДЕЛИ"
    echo "=========================================="
    echo
    echo "Возможные причины:"
    echo "1. Недостаточно места на диске"
    echo "2. Проблемы с интернет соединением"
    echo "3. Ограничения прав доступа"
    echo
    echo "Попробуйте:"
    echo "1. Проверить свободное место: df -h"
    echo "2. Проверить интернет: ping huggingface.co"
    echo "3. Запустить с правами sudo"
    exit 1
fi
