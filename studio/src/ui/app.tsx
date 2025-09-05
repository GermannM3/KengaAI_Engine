import { useMemo, useState } from 'react'
import { Download, Wand2, FileDown, Info } from 'lucide-react'
import { fpsSceneSchema, type KengaFpsScene } from '@/lib/schema'
import { buildFpsSystemPrompt } from '@/lib/prompt'
import { downloadJson } from '@/lib/download'

const SUPABASE_EDGE_URL = import.meta.env.VITE_SUPABASE_EDGE_URL || 'https://yhzyyghmgarnfbmwhxqu.supabase.co/functions/v1/ai-neural-network-api'

export default function App() {
  const [prompt, setPrompt] = useState<string>(examplePrompt)
  const [style, setStyle] = useState<string>('Doom x Destiny, технокоридоры, тревожная подсветка')
  const [levelName, setLevelName] = useState<string>('level_fps_m1')
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [scene, setScene] = useState<KengaFpsScene | null>(null)

  const canGenerate = useMemo(() => prompt.trim().length > 10, [prompt])

  async function onGenerate() {
    setLoading(true); setError(null)
    try {
      const sys = buildFpsSystemPrompt({ style, levelName, maxBoxes: 128, maxEnemies: 16 })
      const msg = `${sys}\n\nПожелания пользователя:\n${prompt}`
      const controller = new AbortController()
      const timeout = setTimeout(() => controller.abort(), 45_000)
      const target = SUPABASE_EDGE_URL || 'mock'
      const res = await fetch(target === 'mock' ? '/api/generate-scene' : SUPABASE_EDGE_URL, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: target === 'mock'
          ? JSON.stringify({ kind: 'fps', prompt, style, levelName, maxBoxes: 128, maxEnemies: 16 })
          : JSON.stringify({ message: msg, files: [], generateImage: false }),
        signal: controller.signal,
      })
      clearTimeout(timeout)
      if (!res.ok) throw new Error(`API ${res.status}`)
      const text = await res.text()
      const json = JSON.parse(text)
      const parsed = fpsSceneSchema.parse(json)
      setScene(parsed)
    } catch (e: any) {
      setError(e?.message ?? 'Ошибка')
    } finally {
      setLoading(false)
    }
  }

  function onDownload() {
    if (!scene) return
    downloadJson(scene, `${levelName || 'kenga_fps_level'}.json`)
  }

  return (
    <div className="max-w-6xl mx-auto p-6 space-y-6">
      <header className="space-y-2">
        <h1 className="text-2xl font-semibold">KengaAI Studio</h1>
        <p className="text-sm text-white/70">Опиши уровень — получишь готовый JSON для KengaAI Engine (FPS, MVP).</p>
      </header>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <div className="card space-y-4">
          <div className="space-y-2">
            <label className="label">Художественный стиль</label>
            <input className="input" value={style} onChange={(e)=>setStyle(e.target.value)} placeholder="Doom x Destiny, sci‑fi" />
          </div>
          <div className="space-y-2">
            <label className="label">Имя уровня</label>
            <input className="input" value={levelName} onChange={(e)=>setLevelName(e.target.value)} placeholder="level_fps_m1" />
          </div>
          <div className="space-y-2">
            <label className="label">Описание уровня</label>
            <textarea className="textarea min-h-[180px]" value={prompt} onChange={(e)=>setPrompt(e.target.value)} />
          </div>
          <div className="flex gap-3">
            <button className="btn-primary" disabled={!canGenerate || loading} onClick={onGenerate}>
              <Wand2 className="w-4 h-4" />
              {loading ? 'Генерация…' : 'Сгенерировать'}
            </button>
            <button className="btn-secondary" disabled={!scene} onClick={onDownload}>
              <FileDown className="w-4 h-4" />
              Скачать JSON
            </button>
          </div>
          {error && <div className="text-sm text-red-400">Ошибка: {error}</div>}
        </div>

        <div className="card space-y-3">
          <h3 className="font-medium">Предпросмотр</h3>
          {scene ? (
            <Preview scene={scene} />
          ) : (
            <div className="text-sm text-white/70">Нажмите “Сгенерировать”.</div>
          )}
        </div>
      </div>

      <div className="card space-y-3">
        <h3 className="font-medium flex items-center gap-2">
          <Info className="w-4 h-4 text-kenga-green" />
          Как запустить в движке
        </h3>
        <ol className="text-sm list-decimal pl-5 space-y-1 text-white/80">
          <li>Сохраните файл в assets/levels/ вашего репозитория.</li>
          <li>Запустите: <code className="bg-black/40 px-1 py-0.5 rounded">cargo run -p kengaai-demo-fps -- assets/levels/&lt;имя&gt;.json</code></li>
          <li>Управление: WASD + мышь, Shift — бег.</li>
        </ol>
      </div>
    </div>
  )
}

function Preview({ scene }: { scene: KengaFpsScene }) {
  const boxes = scene.level?.boxes?.length ?? 0
  const enemies = scene.enemies?.length ?? 0
  return (
    <div className="text-sm space-y-2">
      <div className="flex items-center justify-between">
        <div>Уровень: <span className="font-medium">{scene.meta.name}</span></div>
        <div className="flex items-center gap-2">
          <span className="text-white/60">Фон:</span>
          <span className="inline-block w-4 h-4 rounded border" style={{ backgroundColor: rgbaCss(scene.render.clearColor) }} />
        </div>
      </div>
      <div>Блоков: <span className="font-medium">{boxes}</span></div>
      <div>Враги: <span className="font-medium">{enemies}</span></div>
    </div>
  )
}

const examplePrompt = `Сделай один уровень: стартовая комната, коридор, арена; несколько колонн на арене; игрок начинает со штурмовой винтовкой.`
function rgbaCss([r,g,b,a]: [number,number,number,number]) {
  return `rgba(${Math.round(r*255)}, ${Math.round(g*255)}, ${Math.round(b*255)}, ${a})`
}
