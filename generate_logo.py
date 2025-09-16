import os

# Определяем базовую директорию проекта
# Скрипт должен запускаться из корня проекта
base_dir = os.getcwd() 
icon_dir = os.path.join(base_dir, "studio", "src-tauri", "icons")
icon_path = os.path.join(icon_dir, "icon.png")

# Создаем директорию, если она не существует
os.makedirs(icon_dir, exist_ok=True)

# Создаем простое изображение 512x512 с прозрачным фоном
from PIL import Image
img = Image.new('RGBA', (512, 512), (0, 0, 0, 0))

# Здесь можно добавить код для рисования (например, логотипа),
# но для решения проблемы с RGBA достаточно прозрачного фона.

# Сохраняем иконку напрямую в нужное место
try:
    img.save(icon_path, 'PNG')
    print(f"✅ Успешно создана иконка: {icon_path}")
except Exception as e:
    print(f"❌ Не удалось сохранить иконку: {e}")