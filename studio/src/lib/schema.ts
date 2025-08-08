import { z } from 'zod'

export const vec3 = z.tuple([z.number(), z.number(), z.number()])
export const vec4 = z.tuple([z.number(), z.number(), z.number(), z.number()])
export const color3 = z.tuple([z.number(), z.number(), z.number()])

export const fpsSceneSchema = z.object({
  meta: z.object({
    schema: z.literal('KengaFPSSceneV0'),
    version: z.string(),
    name: z.string(),
  }),
  render: z.object({
    clearColor: vec4,
  }),
  player: z.object({
    spawn: vec3,
    yaw: z.number(),
    pitch: z.number(),
    move: z.object({
      speed: z.number(),
      run: z.number(),
    }),
  }),
  weapons: z.array(z.object({
    id: z.string(),
    kind: z.enum(['hitscan', 'projectile']),
    damage: z.number(),
    rate: z.number(),
    spread: z.number().optional(),
  })).default([]),
  level: z.object({
    boxes: z.array(z.object({
      pos: vec3,
      size: vec3,
      rotY: z.number(),
      color: color3,
    })).default([])
  }),
  enemies: z.array(z.object({
    kind: z.string(),
    spawn: vec3,
    patrol: z.array(vec3).default([])
  })).default([]),
  triggers: z.array(z.object({
    pos: vec3,
    size: vec3,
    onEnter: z.string(),
  })).default([]),
  goals: z.object({
    type: z.enum(['extract','eliminate','collect']).default('extract'),
    point: vec3
  }).optional()
})

export type KengaFpsScene = z.infer<typeof fpsSceneSchema>
