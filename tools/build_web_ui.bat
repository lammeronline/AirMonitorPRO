@echo off
:: ============================================================
::  build_web_ui.bat — regenerate web_ui.h from web_ui.html
::  Double-click this file before compiling in Arduino IDE
::  after editing web_ui.html
:: ============================================================
cd /d "%~dp0.."
python tools\build_web_ui.py
if errorlevel 1 (
    echo.
    echo ERROR: build failed. Make sure Python 3 is installed.
    pause
) else (
    echo.
    echo Done! Now compile the sketch in Arduino IDE.
    timeout /t 3
)
