export async function GET() {
  const url = 'https://yhzyyghmgarnfbmwhxqu.supabase.co/functions/v1/ai-neural-network-api'
  const payload = {
    message: 'Привет, как дела?',
    files: [] as string[],
    generateImage: false,
  }

  try {
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
    })

    const text = await res.text()
    return new Response(
      JSON.stringify({ status: res.status, ok: res.ok, body: text }),
      { status: 200, headers: { 'Content-Type': 'application/json' } },
    )
  } catch (e: any) {
    return new Response(`Fetch error: ${e?.message ?? 'unknown'}`, { status: 500 })
  }
}
