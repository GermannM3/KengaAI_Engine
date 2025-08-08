import { NextRequest } from 'next/server'
import { z } from 'zod'
import { fpsSceneSchema } from '@/lib/schema'

const SUPABASE_EDGE_URL = 'https://yhzyyghmgarnfbmwhxqu.supabase.co/functions/v1/ai-neural-network-api'

const bodySchema = z.object({
  kind: z.literal('fps'),
  prompt: z.string().min(10),
  style: z.string().min(3),
  levelName: z.string().min(3),
  maxBoxes: z.number().min(1).max(512),
  maxEnemies: z.number().min(0).max(128),
})

export async function POST(req: NextRequest) {
  const json = await req.json()
  const parsed = bodySchema.safeParse(json)
  if (!parsed.success) {
    return new Response(`Bad request: ${parsed.error.message}`, { status: 400 })
  }
  const { kind, prompt, style, levelName, maxBoxes, maxEnemies } = parsed.data

  const system = buildFpsSystemPrompt({ style, levelName, maxBoxes, maxEnemies })
  const fullPrompt = `${system}\n\nПожелания пользователя:\n${prompt}`

  // Supabase Edge Function ожидает {message, files, generateImage}
  const payload = {
    message: fullPrompt,
    files: [] as string[],
    generateImage: false,
  }

  const res = await fetch(SUPABASE_EDGE_URL, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  })

  if (!res.ok) {
    const text = await res.text()
    return new Response(`Upstream error ${res.status}: ${text}`, { status: 502 })
  }

  const text = await res.text()

  // ИИ должен вернуть ТОЛЬКО JSON. Валидируем.
  try {
    const data = JSON.parse(text)
    const parsedScene = fpsSceneSchema.parse(data)
    return Response.json({ scene: parsedScene })
  } catch (e: any) {
    return new Response(`Invalid JSON from AI: ${e?.message}\nRaw: ${text}`, { status: 422 })
  }
}

function buildFpsSystemPrompt(opts: {
  style: string
  levelName: string
  maxBoxes: number
  maxEnemies: number
}) {
  const { style, levelName, maxBoxes, maxEnemies } = opts
  return `
Ты — генератор игровых сцен для KengaAI Engine. Верни ТОЛЬКО JSON по схеме "KengaFPSSceneV0" без текста вокруг.

Ограничения:
- До ${maxBoxes} блоков геометрии (оси выровнены, без дыр).
- До ${maxEnemies} врагов.
- Без комментариев и лишних полей, числа — с плавающей точкой.

Стиль уровня: ${style}
Имя уровня: ${levelName}

СХЕМА KengaFPSSceneV0:
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
