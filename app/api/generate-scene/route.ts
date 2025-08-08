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
  try {
    const json = await req.json()
    const parsed = bodySchema.safeParse(json)
    if (!parsed.success) {
      return new Response(`Bad request: ${parsed.error.message}`, { status: 400 })
    }
    const { prompt, style, levelName, maxBoxes, maxEnemies } = parsed.data

    const system = buildFpsSystemPrompt({ style, levelName, maxBoxes, maxEnemies })
    const fullPrompt = `${system}\n\nПожелания пользователя:\n${prompt}`

    const payload = {
      message: fullPrompt,
      files: [] as string[],
      generateImage: false,
    }

    const res = await fetch(SUPABASE_EDGE_URL, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json, text/plain, */*',
        'User-Agent': 'KengaAI-Studio/1.0 (+vercel)',
        'X-Request-Source': 'kenga-studio-vercel',
      },
      redirect: 'follow',
      cache: 'no-store',
      body: JSON.stringify(payload),
    })

    const raw = await res.text()

    if (!res.ok) {
      // Пробрасываем сырое тело для диагностики
      console.error('Upstream error', res.status, raw)
      return new Response(`Upstream ${res.status}: ${raw}`, { status: 502 })
    }

    // Пытаемся распарсить JSON от функции
    let data: unknown
    try {
      data = JSON.parse(raw)
    } catch (e) {
      console.error('Invalid JSON from upstream:', raw)
      return new Response(`Invalid JSON from upstream. Raw: ${raw}`, { status: 422 })
    }

    const parsedScene = fpsSceneSchema.parse(data)
    return Response.json({ scene: parsedScene })
  } catch (e: any) {
    console.error('Route error:', e)
    return new Response(`Route error: ${e?.message ?? 'unknown'}`, { status: 500 })
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
  - До ${maxBoxes} блоков геометрии.
  - До ${maxEnemies} врагов.
  - Без комментариев и лишних полей. Числа — с плавающей точкой.

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
    "level": { "boxes": [ { "pos": [x,y,z], "size": [sx,sy,sz], "rotY": 0.0, "color": [r,g,b] } ] },
    "enemies": [ { "kind": "grunt", "spawn": [x,y,z], "patrol": [[x,y,z],[x,y,z]] } ],
    "triggers": [ { "pos": [x,y,z], "size": [sx,sy,sz], "onEnter": "spawn_wave:grunt:3" } ],
    "goals": { "type": "extract", "point": [x,y,z] }
  }
  `.trim()
}
