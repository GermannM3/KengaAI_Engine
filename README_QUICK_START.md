# KengaAI Engine - Quick Start Guide

Welcome to KengaAI Engine! This guide will help you get started with the engine.

## Prerequisites

Before starting, ensure you have:
- Windows 10/11 (64-bit)
- Rust toolchain installed (https://rustup.rs/)
- Visual Studio Build Tools
- Python 3.x (for diagnostics)

## Quick Start

### 1. Run the Quick Start Menu
Double-click `start_engine.bat` to open the quick start menu with options:

- **Run diagnostics** - Full system diagnostics
- **Check system compatibility** - GPU and system checks
- **Run minimal demo** - Simple demo to test engine
- **Run GPU compatibility test** - Detailed GPU testing
- **View documentation** - Access all guides

### 2. Manual Launch Options

#### Run Minimal Demo:
```batch
cd D:\KengaAI_Engine
set RUST_LOG=info
cargo run --target x86_64-pc-windows-msvc -p minimal-demo -- assets/levels/minimal.json
```

#### Run GPU Compatibility Test:
```batch
cd D:\KengaAI_Engine
set RUST_LOG=info
cargo run --target x86_64-pc-windows-msvc -p gpu-compatibility-test
```

## Troubleshooting White Screen Issue

If you see a white screen:

### 1. Check GPU Compatibility
Intel HD Graphics 4000 (2012) has known limitations:
- Limited OpenGL 4.0 support (engine requires 4.3+)
- Outdated drivers (2014 year)
- Insufficient video memory

### 2. Solutions:
1. **Update Intel drivers** from official website
2. **Use a modern GPU** (NVIDIA/AMD recommended)
3. **Run compatibility tests** to confirm issues

### 3. Run Diagnostic Tools:
```batch
# Full diagnostics
run_diagnostic.bat

# System compatibility check
system_check.bat

# GPU compatibility test
cargo run --target x86_64-pc-windows-msvc -p gpu-compatibility-test
```

## Documentation

Key documentation files:
- `FINAL_DIAGNOSTIC_REPORT.md` - Complete diagnostic results
- `SOLUTION_WHITE_SCREEN.md` - White screen issue solutions
- `SYSTEM_ANALYSIS.md` - System and GPU analysis
- `SESSION_REPORT.md` - Development session reports

## Creating Your Own Game

### 1. Create a new demo:
```bash
cd demos
cargo new my-awesome-game
```

### 2. Add dependencies to `Cargo.toml`:
```toml
[dependencies]
kengaai-fps = { path = "../../crates/fps" }
kengaai-scene-fps = { path = "../../crates/scene_fps" }
winit = "0.29"
anyhow = "1"
env_logger = "0.11"
log = "0.4"
```

### 3. Create levels in `assets/levels/` using JSON format

## Support

For issues and questions:
- Check documentation files first
- Visit GitHub Issues: https://github.com/your-repo/kengaai-engine/issues
- Contact development team

## Hardware Recommendations

### Minimum for Development:
- GPU: NVIDIA GTX 1050 / AMD RX 550 or better
- Drivers: Latest from manufacturer
- OpenGL: 4.3+ support
- VRAM: 2GB+ dedicated

### Recommended:
- GPU: NVIDIA RTX 3060 / AMD RX 6600 or better
- Drivers: Auto-update enabled
- VRAM: 4GB+ dedicated

Note: Intel HD Graphics 4000 (2012) has known compatibility issues.