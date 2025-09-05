import webbrowser
import time
import pyautogui
import os

# Открываем HTML файл с логотипом в браузере
webbrowser.open('file://' + os.path.realpath('D:/KengaAI_Engine/logo.html'))

# Ждем загрузки страницы
time.sleep(3)

# Делаем скриншот
screenshot = pyautogui.screenshot()

# Обрезаем скриншот до размеров логотипа
logo = screenshot.crop((0, 0, 800, 400))

# Сохраняем логотип
logo.save('D:/KengaAI_Engine/assets/logo.png')

print("Logo screenshot created successfully!")