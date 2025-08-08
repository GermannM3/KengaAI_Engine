# KengaAI No‑Code: Формат сцены

JSON (MVP) — минимальный, читаемый, легко генерируемый ИИ.

Оси и единицы измерения:
- 2D, координаты и размеры в NDC (‑1..1) по X и Y
- Повороты в радианах

Схема:
```json
{
  "background": [0.08, 0.10, 0.12, 1.0],
  "entities": [
    {
      "transform": { "position": [x, y], "rotation": 0.0, "scale": [1.0, 1.0] },
      "sprite":    { "size": [w, h], "color": [r, g, b] },
      "behavior":  { "type": "Move", "params": { "velocity": [vx, vy] } }
    }
  ]
}
