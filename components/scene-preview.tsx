'use client'

import { Card, CardContent } from '@/components/ui/card'
import { type KengaFpsScene } from '@/lib/schema'

export function ScenePreview({ scene }: { scene: KengaFpsScene }) {
  const boxes = scene.level?.boxes?.length ?? 0
  const enemies = scene.enemies?.length ?? 0
  const weapons = scene.weapons?.length ?? 0
  const color = scene.render.clearColor

  return (
    <div className="space-y-4">
      <Card>
        <CardContent className="p-4 text-sm">
          <div className="grid grid-cols-2 gap-3">
            <div>
              <div className="text-muted-foreground">Уровень</div>
              <div className="font-medium">{scene.meta?.name || '(без имени)'}</div>
            </div>
            <div>
              <div className="text-muted-foreground">Структура</div>
              <div className="font-medium">{boxes} блоков</div>
            </div>
            <div>
              <div className="text-muted-foreground">Враги</div>
              <div className="font-medium">{enemies}</div>
            </div>
            <div>
              <div className="text-muted-foreground">Оружие</div>
              <div className="font-medium">{weapons}</div>
            </div>
            <div className="col-span-2">
              <div className="text-muted-foreground">Фон</div>
              <div className="flex items-center gap-2">
                <div
                  className="h-5 w-5 rounded border"
                  style={{ backgroundColor: rgbaCss(color) }}
                  aria-label="clear color"
                  title={`rgba(${color.map((c) => Math.round(c * 255)).join(', ')})`}
                />
                <span className="text-xs">{`[${color.map((c) => c.toFixed(2)).join(', ')}]`}</span>
              </div>
            </div>
          </div>
        </CardContent>
      </Card>

      <div className="text-xs text-muted-foreground">
        Подсказка: скачай JSON и положи в assets/levels движка. В MVP‑версии движок строит уровень из прямоугольных блоков (оси X/Z), использует FPS‑камеру и хитскан‑оружие.
      </div>
    </div>
  )
}

function rgbaCss(c: [number, number, number, number]) {
  const [r, g, b, a] = c
  return `rgba(${Math.round(r * 255)}, ${Math.round(g * 255)}, ${Math.round(b * 255)}, ${a})`
}
