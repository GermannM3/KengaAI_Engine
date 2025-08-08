'use client'

import { useState, useMemo } from 'react'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Label } from '@/components/ui/label'
import { Textarea } from '@/components/ui/textarea'
import { Input } from '@/components/ui/input'
import { Tabs, TabsList, TabsTrigger, TabsContent } from '@/components/ui/tabs'
import { Separator } from '@/components/ui/separator'
import { Download, Play, Wand2, FileDown, Info } from 'lucide-react'
import { ScenePreview } from '@/components/scene-preview'
import { type KengaFpsScene } from '@/lib/schema'
import { examplePrompt, fpsSchemaGuide } from '@/lib/prompt-templates'
import { downloadJson } from '@/lib/download'

export default function HomePage() {
  const [prompt, setPrompt] = useState<string>(examplePrompt)
  const [style, setStyle] = useState<string>('Doom x Destiny, мрачные коридоры мегаструктуры')
  const [levelName, setLevelName] = useState<string>('level_fps_m1')
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [scene, setScene] = useState<KengaFpsScene | null>(null)

  const canGenerate = useMemo(() => prompt.trim().length > 10, [prompt])

  async function onGenerate() {
    setLoading(true)
    setError(null)
    try {
      const res = await fetch('/api/generate-scene', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          kind: 'fps',
          prompt,
          style,
          levelName,
          maxBoxes: 128,
          maxEnemies: 16,
        }),
      })
      if (!res.ok) {
        const txt = await res.text()
        throw new Error(`API error: ${res.status} ${txt}`)
      }
      const data = (await res.json()) as { scene: KengaFpsScene }
      setScene(data.scene)
    } catch (e: any) {
      setError(e?.message ?? 'Неизвестная ошибка')
    } finally {
      setLoading(false)
    }
  }

  function onDownload() {
    if (!scene) return
    downloadJson(scene, `${levelName || 'kenga_fps_level'}.json`)
  }

  return (
    <main className="mx-auto max-w-6xl p-6 space-y-6">
      <header className="flex flex-col gap-2">
        <h1 className="text-2xl font-semibold">KengaAI Studio — No‑Code генератор уровней</h1>
        <p className="text-sm text-muted-foreground">
          Опиши игру словами — агент соберёт JSON‑уровень для KengaAI Engine. Поддержка: шутер от первого лица (MVP).
        </p>
      </header>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <Card>
          <CardHeader>
            <CardTitle>Описание уровня</CardTitle>
            <CardDescription>Жанр: FPS. Агент создаст сцену по схеме KengaFPSScene.</CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="space-y-2">
              <Label htmlFor="style">Художественный стиль</Label>
              <Input
                id="style"
                placeholder="Напр.: Doom x Destiny, sci‑fi, неон"
                value={style}
                onChange={(e) => setStyle(e.target.value)}
              />
            </div>
            <div className="space-y-2">
              <Label htmlFor="levelName">Имя уровня (файл)</Label>
              <Input
                id="levelName"
                placeholder="level_fps_m1"
                value={levelName}
                onChange={(e) => setLevelName(e.target.value)}
              />
            </div>
            <div className="space-y-2">
              <Label htmlFor="prompt">ТЗ для агента</Label>
              <Textarea
                id="prompt"
                className="min-h-[200px]"
                value={prompt}
                onChange={(e) => setPrompt(e.target.value)}
              />
            </div>
            <div className="flex gap-3">
              <Button disabled={!canGenerate || loading} onClick={onGenerate} className="gap-2">
                <Wand2 className="h-4 w-4" />
                {loading ? 'Генерация…' : 'Сгенерировать уровень'}
              </Button>
              <Button variant="secondary" disabled={!scene} onClick={onDownload} className="gap-2">
                <FileDown className="h-4 w-4" />
                Скачать JSON
              </Button>
            </div>
            {error && (
              <p className="text-sm text-red-600">Ошибка: {error}</p>
            )}
          </CardContent>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle>Предпросмотр</CardTitle>
            <CardDescription>Краткая сводка по сгенерированной сцене</CardDescription>
          </CardHeader>
          <CardContent>
            {scene ? (
              <ScenePreview scene={scene} />
            ) : (
              <div className="text-sm text-muted-foreground">
                Нет данных. Нажми “Сгенерировать уровень”, чтобы получить сцену.
              </div>
            )}
          </CardContent>
        </Card>
      </div>

      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Info className="h-4 w-4" />
            Как запустить в движке
          </CardTitle>
          <CardDescription>
            Скачай JSON и положи его в <code className="font-mono">assets/levels/</code> движка.
          </CardDescription>
        </CardHeader>
        <CardContent className="space-y-4">
          <ol className="list-decimal pl-4 space-y-1 text-sm">
            <li>Скачай <strong>{(levelName || 'kenga_fps_level') + '.json'}</strong> и помести в <code className="font-mono">assets/levels/</code> репозитория KengaAI Engine.</li>
            <li>Запусти демо FPS загрузчика (в движке): <code className="font-mono">cargo run -p kengaai-demo-fps -- assets/levels/{(levelName || 'kenga_fps_level') + '.json'}</code></li>
            <li>Играй: WASD + мышь, ЛКМ — выстрел. (MVP — один уровень, базовые враги и оружие).</li>
          </ol>
          <Separator />
          <Tabs defaultValue="schema">
            <TabsList>
              <TabsTrigger value="schema">Схема KengaFPSScene</TabsTrigger>
              <TabsTrigger value="prompt">Пример запроса</TabsTrigger>
            </TabsList>
            <TabsContent value="schema">
              <pre className="text-xs overflow-auto bg-muted p-3 rounded-md">{fpsSchemaGuide}</pre>
            </TabsContent>
            <TabsContent value="prompt">
              <pre className="text-xs overflow-auto bg-muted p-3 rounded-md">{examplePrompt}</pre>
            </TabsContent>
          </Tabs>
        </CardContent>
      </Card>
    </main>
  )
}
