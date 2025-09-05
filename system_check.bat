@echo off
title KengaAI Engine - System Compatibility Check

echo ====================================================
echo    KENGAIA ENGINE - SYSTEM COMPATIBILITY CHECK
echo ====================================================
echo.

echo Checking system information...
echo ----------------------------------------------------
systeminfo | findstr /C:"OS Name" /C:"OS Version" /C:"System Type" /C:"Processor"
echo.

echo Checking GPU information...
echo ----------------------------------------------------
dxdiag /t dxdiag_report.txt >nul 2>&1
if exist dxdiag_report.txt (
    echo GPU Information saved to dxdiag_report.txt
    echo Please check the file for detailed GPU specifications
) else (
    echo Could not generate GPU report
)
echo.

echo Checking OpenGL support...
echo ----------------------------------------------------
echo Creating OpenGL capability checker...
echo.

REM Create a simple OpenGL checker in Python
echo import OpenGL.GL as gl > opengl_check.py
echo import pygame >> opengl_check.py
echo import sys >> opengl_check.py
echo. >> opengl_check.py
echo def check_opengl(): >> opengl_check.py
echo     try: >> opengl_check.py
echo         pygame.init() >> opengl_check.py
echo         pygame.display.set_mode((800, 600), pygame.OPENGL ^| pygame.DOUBLEBUF) >> opengl_check.py
echo         print("OpenGL Version:", gl.glGetString(gl.GL_VERSION).decode()) >> opengl_check.py
echo         print("GLSL Version:", gl.glGetString(gl.GL_SHADING_LANGUAGE_VERSION).decode()) >> opengl_check.py
echo         print("Vendor:", gl.glGetString(gl.GL_VENDOR).decode()) >> opengl_check.py
echo         print("Renderer:", gl.glGetString(gl.GL_RENDERER).decode()) >> opengl_check.py
echo         extensions = gl.glGetString(gl.GL_EXTENSIONS).decode() >> opengl_check.py
echo         ext_list = extensions.split() >> opengl_check.py
echo         print("Number of extensions:", len(ext_list)) >> opengl_check.py
echo         print("^> OpenGL check completed") >> opengl_check.py
echo         pygame.quit() >> opengl_check.py
echo     except Exception as e: >> opengl_check.py
echo         print("^> OpenGL check failed:", str(e)) >> opengl_check.py
echo         if pygame.get_init(): >> opengl_check.py
echo             pygame.quit() >> opengl_check.py
echo. >> opengl_check.py
echo if __name__ == "__main__": >> opengl_check.py
echo     check_opengl() >> opengl_check.py

echo Running OpenGL compatibility check...
python opengl_check.py
if errorlevel 1 (
    echo Failed to run OpenGL check - Python may not be installed
)
echo.

echo Checking Rust and Cargo installation...
echo ----------------------------------------------------
rustc --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Rust is not installed!
    echo Please install Rust from https://rustup.rs/
) else (
    echo Rust is installed
    rustc --version
)

cargo --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Cargo is not installed!
) else (
    echo Cargo is installed
    cargo --version
)
echo.

echo Checking Visual Studio Build Tools...
echo ----------------------------------------------------
cl.exe >nul 2>&1
if errorlevel 1 (
    echo WARNING: Visual Studio Build Tools not found in PATH
    echo This may cause compilation issues
) else (
    echo Visual Studio Build Tools detected
)
echo.

echo ====================================================
echo       COMPATIBILITY CHECK COMPLETE
echo ====================================================
echo.
echo Recommendations:
echo 1. Ensure you have the latest Intel HD Graphics drivers
echo 2. Install Python 3.x for additional diagnostics
echo 3. Consider using a more powerful GPU for development
echo.
pause