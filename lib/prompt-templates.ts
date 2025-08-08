export const fpsSchemaGuide = `
KengaFPSSceneV0
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

export const examplePrompt = `Сделай уровень шутера от первого лица:
- Сеттинг: научно‑фантастический бункер (Doom x Destiny), красная тревога, мерцающее освещение.
- План: стартовая комната -> узкий коридор -> арена с колоннами -> лифт к эвакуации.
- Игрок: спавнится в стартовой, скорость средняя, есть винтовка (хитскан), урон умеренный.
- Враги: 6–8 "grunt", патрулируют коридоры и встречают на арене.`
