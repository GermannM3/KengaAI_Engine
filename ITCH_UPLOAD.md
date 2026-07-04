# Загрузка на itch.io

## Подготовка

1. Установи [butler](https://itch.io/docs/butler/) (CLI для itch.io):
```powershell
# Windows (choco)
choco install butler

# Или скачай с https://github.com/itchio/butler/releases
```

2. авторизуйся:
```powershell
butler login
```

## Создание игры на itch.io

1. Зайди на https://itch.io/game/new
2. Заполни:
   - **Title:** Arena One — Kenga Engine Demo
   - **URL:** arena-one (или другой уникальный slug)
   - **Classification:** Game > Action > FPS
   - **Description:** См. itch-page.md
   - **Uploads:** Загрузи `KengaEngine-Demo.zip`
   - **Platform:** Windows
   - **System requirements:** Windows 10/11, Vulkan GPU, 4GB RAM
   - **Visibility:** Public (или как хочешь)

## Загрузка через butler

```powershell
# Из корня проекта
butler push KengaEngine-Demo.zip your-username/arena-one:windows
```

## Обновление версии

```powershell
# При обновлении
butler push KengaEngine-Demo.zip your-username/arena-one:windows --userversion 0.2.0
```
