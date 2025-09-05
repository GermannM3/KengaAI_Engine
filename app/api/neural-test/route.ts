export const runtime = 'edge'

export async function GET() {
  const url = process.env.NEXT_PUBLIC_SUPABASE_EDGE_URL || 'http://127.0.0.1:8787/functions/v1/ai-neural-network-api'
  const payload = {
    message: 'Привет, как дела?',
    files: [] as string[],
    generateImage: false,
  }

  try {
    const controller = new AbortController()
    const t0 = Date.now()
    const timeout = setTimeout(() => controller.abort(), 30_000)
    const res = await fetch(url, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json, text/plain, */*',
        'User-Agent': 'KengaAI-Studio/diagnostic',
        'X-Request-Source': 'diagnostic',
      },
      cache: 'no-store',
      body: JSON.stringify(payload),
      signal: controller.signal,
    })
    clearTimeout(timeout)

    const text = await res.text()
    const ms = Date.now() - t0
    return new Response(JSON.stringify({ status: res.status, ok: res.ok, ms, body: text }), {
      status: 200,
      headers: { 'Content-Type': 'application/json' },
    })
  } catch (e: any) {
    return new Response(`Fetch error: ${e?.message ?? 'unknown'}`, { status: 500 })
  }
}
