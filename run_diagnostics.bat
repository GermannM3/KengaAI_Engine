@echo off
echo === ЗАПУСК ДИАГНОСТИКИ KENGAIA ENGINE ===
echo Система: %OS%
echo Архитектура: %PROCESSOR_ARCHITECTURE%
echo Процессор: %PROCESSOR_IDENTIFIER%
echo Дата: %DATE% Время: %TIME%
echo =========================================

echo Установка уровня логирования...
set RUST_LOG=trace
echo Уровень логирования установлен на TRACE

echo Запуск диагностики из корня проекта...
cd /d D:\KengaAI_Engine
cargo run --target x86_64-pc-windows-msvc -p minimal-demo -- assets/levels/minimal.json

echo =========================================
echo Диагностика завершена
pause