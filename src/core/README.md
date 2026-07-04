# Core — ядро Kenga Engine

Фаза 2.1: Game loop, Time, Input, LogManager.

## Модули

- **Application** — основной цикл (fixed timestep 60 Hz), init/shutdown
- **Time** — get_time, delta_time, fixed_delta_time
- **Input** — is_key_pressed, GLFW callbacks (ESC → close)
- **LogManager** — spdlog wrapper, макросы KNG_TRACE..KNG_CRITICAL

## Использование

```cpp
#include "core/LogManager.h"
#include "core/LoggerMacros.h"
#include "core/Application.h"

int main() {
    kenga::LogManager::init();
    kenga::Application app;
    app.run();
    return 0;
}
```
