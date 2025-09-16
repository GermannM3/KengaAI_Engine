import os
import sys
from cairosvg import svg2png

# --- Гарантированное определение базовой директории ---
if getattr(sys, 'frozen', False):
    # Если приложение "заморожено" (например, PyInstaller)
    base_dir = os.path.dirname(sys.executable)
else:
    # Обычный запуск .py скрипта
    script_dir = os.path.dirname(os.path.realpath(__file__))
    base_dir = os.path.dirname(script_dir)

print(f"[*] Базовая директория проекта: {base_dir}")

# Список пар (исходный SVG, целевой PNG)
files_to_convert = [
    (os.path.join(base_dir, "assets", "logo.svg"), os.path.join(base_dir, "assets", "logo.png")),
    (os.path.join(base_dir, "screenshots", "main_menu.svg"), os.path.join(base_dir, "screenshots", "main_menu.png")),
    (os.path.join(base_dir, "screenshots", "lighting_level.svg"), os.path.join(base_dir, "screenshots", "lighting_level.png")),
    (os.path.join(base_dir, "screenshots", "particles_level.svg"), os.path.join(base_dir, "screenshots", "particles_level.png")),
]

# Проверяем наличие cairosvg
try:
    import cairosvg
except ImportError:
    print("Ошибка: cairosvg не установлен.")
    print("Установите его командой: pip3 install cairosvg")
    sys.exit(1)

# Конвертируем файлы
converted_count = 0
for svg_path, png_path in files_to_convert:
    if os.path.exists(svg_path):
        print(f"Конвертируем {svg_path} -> {png_path}...")
        try:
            svg2png(url=svg_path, write_to=png_path, output_width=512, output_height=512)
            print("✅ Готово")
            converted_count += 1
        except Exception as e:
            print(f"❌ Ошибка конвертации: {e}")
    else:
        print(f"Файл не найден: {svg_path}")

if converted_count == 0:
    print("\nНи один файл не был сконвертирован.")
else:
    print(f"\n✅ Успешно сконвертировано {converted_count} файлов.")