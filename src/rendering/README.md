# Rendering — Vulkan backend

Фаза 3.1: базовый Vulkan (clear color, swapchain, validation layers).

## Модули

- **VulkanContext** — instance, physical/logical device, queues, surface, debug messenger
- **Swapchain** — B8G8R8A8_SRGB, recreation on resize
- **RenderPass** — single subpass, clear color (0.1, 0.1, 0.2)
- **Renderer** — VulkanRenderer: command pool/buffers, fences, semaphores

## Запуск

```powershell
.\build\Debug\KengaEngine.exe
```

Окно 1280x720, тёмно-синий экран. ESC — выход.
