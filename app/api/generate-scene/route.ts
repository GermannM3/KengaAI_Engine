import { NextRequest } from 'next/server'
import { z } from 'zod'
import { fpsSceneSchema } from '@/lib/schema'

export const runtime = 'nodejs'

const SUPABASE_EDGE_URL = process.env.NEXT_PUBLIC_SUPABASE_EDGE_URL

const bodySchema = z.object({
  kind: z.literal('fps'),
  prompt: z.string().min(10).max(5000),
  style: z.string().min(3).max(200),
  levelName: z.string().min(3).max(64),
  maxBoxes: z.number().min(1).max(512),
  maxEnemies: z.number().min(0).max(128),
})

export async function POST(req: NextRequest) {
  try {
    // Простейший rate-limit (IP, 20 запросов за 10 мин)
    if (isRateLimited(req)) {
      return new Response('Too Many Requests', { status: 429 })
    }
    const json = await req.json()
    const parsed = bodySchema.safeParse(json)
    if (!parsed.success) {
      return new Response(`Bad request: ${parsed.error.message}`, { status: 400 })
    }
    const { prompt, style, levelName, maxBoxes, maxEnemies } = parsed.data

    // Оффлайн-режим: если не задан внешний эндпойнт — генерируем мок‑сцену локально
    if (!SUPABASE_EDGE_URL || SUPABASE_EDGE_URL.trim() === '' || SUPABASE_EDGE_URL === 'mock') {
      const mock = buildMockScene({ style, levelName, maxBoxes, maxEnemies })
      const parsedScene = fpsSceneSchema.parse(mock)
      return Response.json({ scene: parsedScene })
    }

    const system = buildFpsSystemPrompt({ style, levelName, maxBoxes, maxEnemies })
    const fullPrompt = `${system}\n\nПожелания пользователя:\n${prompt}`

    const payload = {
      message: fullPrompt,
      files: [] as string[],
      generateImage: false,
    }

    const controller = new AbortController()
    const timeout = setTimeout(() => controller.abort(), 90_000)
    let res: Response
    try {
      res = await fetch(SUPABASE_EDGE_URL, {
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
      signal: controller.signal,
      })
    } catch (e: any) {
      clearTimeout(timeout)
      // сеть недоступна/таймаут — используем офлайн-мок
      const fallback = buildMockScene({ style, levelName, maxBoxes, maxEnemies })
      const parsedScene = fpsSceneSchema.parse(fallback)
      return new Response(JSON.stringify({ scene: parsedScene }), {
        status: 200,
        headers: { 'Content-Type': 'application/json', 'X-Fallback': 'mock:network-error' },
      })
    }
    clearTimeout(timeout)

    const raw = await res.text()

    if (!res.ok) {
      // Фолбэк на мок при ошибках апстрима
      const fallback = buildMockScene({ style, levelName, maxBoxes, maxEnemies })
      const parsedScene = fpsSceneSchema.parse(fallback)
      return new Response(JSON.stringify({ scene: parsedScene }), {
        status: 200,
        headers: { 'Content-Type': 'application/json', 'X-Fallback': `mock:upstream-${res.status}` },
      })
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

function buildMockScene(opts: { style: string; levelName: string; maxBoxes: number; maxEnemies: number }) {
  const { levelName, maxBoxes, maxEnemies } = opts
  const boxes = [] as Array<{ pos: [number,number,number]; size: [number,number,number]; rotY: number; color: [number,number,number] }>
  const count = Math.min(10, Math.max(1, Math.floor(maxBoxes/10)))
  for (let i=0;i<count;i++) {
    boxes.push({
      pos: [ (i%5)*2 - 4, 0.0, -2 - Math.floor(i/5)*2 ],
      size: [ 1.0, 1.0, 1.0 ],
      rotY: 0.0,
      color: [ 0.2 + 0.1*(i%3), 0.3, 0.4 ],
    })
  }
  const enemies = [] as Array<{ kind: string; spawn: [number,number,number]; patrol: [number,number,number][] }>
  const enemyCount = Math.min(3, maxEnemies)
  for (let i=0;i<enemyCount;i++) {
    enemies.push({ kind: 'grunt', spawn: [ i*1.5-1.5, 0.5, -6 - i ], patrol: [] })
  }
  return {
    meta: { schema: 'KengaFPSSceneV0', version: '0.1.0', name: levelName || 'level_mock' },
    render: { clearColor: [0.05, 0.07, 0.09, 1.0] as [number,number,number,number] },
    player: { spawn: [0.0, 1.5, 4.0] as [number,number,number], yaw: 3.1415, pitch: 0.0, move: { speed: 4.5, run: 7.5 } },
    weapons: [ { id: 'rifle', kind: 'hitscan', damage: 12.0, rate: 6.0, spread: 1.5 } ],
    level: { boxes },
    enemies,
    triggers: [],
    goals: { type: 'extract', point: [0.0, 0.5, -8.0] as [number,number,number] },
  }
}

// --- простейший in-memory rate limiter (per-process) ---
type Bucket = { count: number; resetAt: number }
const WINDOW_MS = 10 * 60 * 1000
const LIMIT = 20
const buckets = new Map<string, Bucket>()

function isRateLimited(req: NextRequest): boolean {
  const ipRaw = req.headers.get('x-forwarded-for') || req.headers.get('cf-connecting-ip') || req.headers.get('x-real-ip') || 'unknown'
  const ip = ipRaw.split(',')[0].trim()
  const now = Date.now()
  const key = `gen:${ip}`
  const b = buckets.get(key)
  if (!b || now > b.resetAt) {
    buckets.set(key, { count: 1, resetAt: now + WINDOW_MS })
    return false
  }
  if (b.count >= LIMIT) return true
  b.count += 1
  return false
}
