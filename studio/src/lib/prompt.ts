export function buildFpsSystemPrompt(opts: {
  style: string
  levelName: string
  maxBoxes: number
  maxEnemies: number
}) {
  const { style, levelName, maxBoxes, maxEnemies } = opts
  return `
Ты — генератор игровых сцен. Верни ТОЛЬКО JSON по схеме "KengaFPSSceneV0" без текста вокруг.

Ограничения:
- До ${maxBoxes} блоков уровня.
- До ${maxEnemies} врагов.
- Числа с плавающей точкой. Без комментариев и лишних полей.

Стиль уровня: ${style}
Имя уровня: ${levelName}

СХЕМА:
{
  "meta": { "schema": "KengaFPSSceneV0", "version": "0.1.0", "name": "level_name" },
  "render": { "clearColor": [r,g,b,a] },
  "player": { "spawn": [x,y,z], "yaw": 0.0, "pitch": 0.0, "move": { "speed": 4.5, "run": 7.5 } },
  "weapons": [
    { "id": "rifle", "kind": "hitscan", "damage": 12.0, "rate": 6.0, "spread": 1.5 }
  ],
  "level": {
    "boxes": [
      { "pos": [x,y,z], "size": [sx,sy,sz], "rotY": 0.0, "color": [r,g,b] }
    ]
  },
  "enemies": [
    { "kind": "grunt", "spawn": [x,y,z], "patrol": [[x,y,z],[x,y,z]] }
  ],
  "triggers": [
    { "pos": [x,y,z], "size": [sx,sy,sz], "onEnter": "spawn_wave:grunt:3" }
  ],
  "goals": { "type": "extract", "point": [x,y,z] }
}
`.trim()
}
