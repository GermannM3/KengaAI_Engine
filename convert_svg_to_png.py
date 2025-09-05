import os
import subprocess

# Проверяем, установлен ли Inkscape
def check_inkscape():
    try:
        subprocess.run(['inkscape', '--version'], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False

# Проверяем, установлен ли ImageMagick
def check_imagemagick():
    try:
        subprocess.run(['magick', '--version'], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False

def convert_svg_to_png(svg_path, png_path, width=800, height=600):
    """Конвертирует SVG в PNG"""
    # Пробуем использовать Inkscape
    if check_inkscape():
        cmd = [
            'inkscape', 
            svg_path, 
            f'--export-width={width}',
            f'--export-height={height}',
            f'--export-filename={png_path}'
        ]
        subprocess.run(cmd, check=True)
        return True
    
    # Пробуем использовать ImageMagick
    if check_imagemagick():
        cmd = [
            'magick',
            'convert',
            f'-size', f'{width}x{height}',
            svg_path,
            png_path
        ]
        subprocess.run(cmd, check=True)
        return True
    
    return False

if __name__ == "__main__":
    # Создаем директорию для PNG, если её нет
    os.makedirs('D:/KengaAI_Engine/screenshots/png', exist_ok=True)
    
    # Конвертируем SVG в PNG
    svg_files = [
        'D:/KengaAI_Engine/assets/logo.svg',
        'D:/KengaAI_Engine/screenshots/main_menu.svg',
        'D:/KengaAI_Engine/screenshots/lighting_level.svg',
        'D:/KengaAI_Engine/screenshots/particles_level.svg'
    ]
    
    for svg_file in svg_files:
        if os.path.exists(svg_file):
            filename = os.path.basename(svg_file)
            name_without_ext = os.path.splitext(filename)[0]
            png_file = f'D:/KengaAI_Engine/screenshots/png/{name_without_ext}.png'
            
            try:
                if convert_svg_to_png(svg_file, png_file):
                    print(f"Успешно конвертирован: {filename} -> {name_without_ext}.png")
                else:
                    print(f"Не удалось конвертировать {filename}: нет подходящих инструментов")
            except Exception as e:
                print(f"Ошибка при конвертации {filename}: {e}")
        else:
            print(f"Файл не найден: {svg_file}")
    
    print("\nДля ручной конвертации SVG в PNG вы можете:")
    print("1. Использовать онлайн-конвертеры (например, https://svgtopng.com/)")
    print("2. Установить Inkscape (https://inkscape.org/)")
    print("3. Установить ImageMagick (https://imagemagick.org/)")