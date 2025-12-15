@echo off
title InstaPOD - Winamp Companion Installer
color 0A

echo.
echo ========================================
echo   InstaPOD Winamp Companion
echo   Instalador Automatico
echo ========================================
echo.

REM Check Python
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python no encontrado
    echo Por favor instala Python desde python.org
    pause
    exit /b 1
)

echo [OK] Python encontrado
echo.

REM Install dependencies
echo Instalando dependencias...
pip install yt-dlp >nul 2>&1
if errorlevel 1 (
    echo [WARNING] Error instalando yt-dlp
) else (
    echo [OK] yt-dlp instalado
)

echo.
echo ========================================
echo   Instalacion Completa
echo ========================================
echo.
echo Para usar:
echo   1. Ejecuta: python winamp_companion.py
echo   2. Pega una URL de YouTube
echo   3. Click en "Descargar y Agregar"
echo   4. La musica se guarda en tu carpeta de Winamp
echo.
echo TIP: Crea un acceso directo para abrir rapido
echo.

pause
