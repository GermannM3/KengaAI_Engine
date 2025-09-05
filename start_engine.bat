@echo off
title KengaAI Engine - Quick Start

echo ====================================================
echo        KENGAIA ENGINE QUICK START MENU
echo ====================================================
echo.

:menu
echo Please select an option:
echo.
echo 1. Run diagnostics
echo 2. Check system compatibility
echo 3. Run minimal demo
echo 4. Run GPU compatibility test
echo 5. View documentation
echo 6. Exit
echo.
echo Current directory: %CD%
echo.

set /p choice="Enter your choice (1-6): "

echo.
if "%choice%"=="1" goto run_diagnostics
if "%choice%"=="2" goto check_system
if "%choice%"=="3" goto run_minimal
if "%choice%"=="4" goto run_gpu_test
if "%choice%"=="5" goto view_docs
if "%choice%"=="6" goto exit_script

echo Invalid choice. Please try again.
echo.
goto menu

:run_diagnostics
echo Running full diagnostics...
call run_diagnostic.bat
echo.
goto menu

:check_system
echo Checking system...
call system_check.bat
echo.
goto menu

:run_minimal
echo Running minimal demo...
cd D:\KengaAI_Engine
set RUST_LOG=info
cargo run --target x86_64-pc-windows-msvc -p minimal-demo -- assets/levels/minimal.json
echo.
goto menu

:run_gpu_test
echo Running GPU compatibility test...
cd D:\KengaAI_Engine
set RUST_LOG=info
cargo run --target x86_64-pc-windows-msvc -p gpu-compatibility-test
echo.
goto menu

:view_docs
echo Opening documentation...
echo Available documentation files:
dir *.md
echo.
echo Please open these files in a text editor or browser:
echo - FINAL_DIAGNOSTIC_REPORT.md
echo - SOLUTION_WHITE_SCREEN.md
echo - SYSTEM_ANALYSIS.md
echo - SESSION_REPORT.md
echo.
echo Press any key to continue...
pause >nul
echo.
goto menu

:exit_script
echo Thank you for using KengaAI Engine!
echo.
echo For support, please contact:
echo - GitHub Issues: https://github.com/your-repo/kengaai-engine/issues
echo - Documentation: https://kengaai.github.io/docs
echo.
pause
exit /b 0