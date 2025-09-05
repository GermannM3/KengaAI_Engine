@echo off
title KengaAI Engine - Automated Diagnostic

echo ====================================================
echo    KENGAIA ENGINE AUTOMATED DIAGNOSTIC TOOL
echo ====================================================
echo.

echo [1/5] Checking system prerequisites...
echo ----------------------------------------
rustc --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Rust is not installed!
    echo Please install Rust from https://rustup.rs/
    pause
    exit /b 1
) else (
    echo ✓ Rust is installed
)

cargo --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Cargo is not found!
    pause
    exit /b 1
) else (
    echo ✓ Cargo is available
)

echo.

echo [2/5] Checking GPU compatibility...
echo -----------------------------------
echo Creating OpenGL compatibility test...

REM Create Python test script
(
echo import OpenGL.GL as gl
echo import pygame
echo import sys
echo.
echo def test_opengl():
echo     try:
echo         pygame.init()
echo         pygame.display.set_mode((800, 600), pygame.OPENGL ^| pygame.DOUBLEBUF)
echo         version = gl.glGetString(gl.GL_VERSION).decode()
echo         print("OpenGL Version:", version)
echo         glsl_version = gl.glGetString(gl.GL_SHADING_LANGUAGE_VERSION).decode()
echo         print("GLSL Version:", glsl_version)
echo         vendor = gl.glGetString(gl.GL_VENDOR).decode()
echo         print("Vendor:", vendor)
echo         renderer = gl.glGetString(gl.GL_RENDERER).decode()
echo         print("Renderer:", renderer)
echo         pygame.quit()
echo         return True
echo     except Exception as e:
echo         print("OpenGL test failed:", str(e))
echo         if pygame.get_init():
echo             pygame.quit()
echo         return False
echo.
echo if __name__ == "__main__":
echo     test_opengl()
) > opengl_test.py

python opengl_test.py > opengl_result.txt 2>&1
if errorlevel 1 (
    echo WARNING: Could not run OpenGL test - Python may not be installed
) else (
    type opengl_result.txt
)

echo.

echo [3/5] Building diagnostic tools...
echo ---------------------------------
cd /d D:\KengaAI_Engine
cargo build --target x86_64-pc-windows-msvc -p gpu-compatibility-test --release > build.log 2>&1
if errorlevel 1 (
    echo ERROR: Failed to build diagnostic tools
    echo See build.log for details
) else (
    echo ✓ Diagnostic tools built successfully
)

echo.

echo [4/5] Running GPU compatibility test...
echo ---------------------------------------
echo This test will open a window to check GPU compatibility.
echo If window shows colored graphics, GPU is compatible.
echo If window stays white or crashes, GPU needs upgrade.
echo Press any key to start test...
pause >nul

set RUST_LOG=info
cargo run --target x86_64-pc-windows-msvc -p gpu-compatibility-test --release > test_run.log 2>&1
if errorlevel 1 (
    echo Test completed with issues. See test_run.log for details.
) else (
    echo Test completed successfully.
)

echo.

echo [5/5] Generating diagnostic report...
echo -------------------------------------
echo Diagnostic completed.
echo Check the following files for detailed information:
echo - build.log (compilation log)
echo - test_run.log (runtime log)
echo - opengl_result.txt (OpenGL capabilities)

echo.

echo ====================================================
echo    DIAGNOSTIC COMPLETED
echo ====================================================
echo.
echo Summary:
echo - System prerequisites: Checked
echo - GPU compatibility: Tested
echo - Diagnostic tools: Built and run
echo.
echo If you still have issues:
echo 1. Update Intel HD Graphics drivers
echo 2. Consider GPU upgrade for development
echo 3. Contact support with diagnostic logs
echo.
pause