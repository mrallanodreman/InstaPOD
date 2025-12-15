@echo off
echo =====================================================
echo Music4All Winamp Plugin - Build Script
echo =====================================================
echo.

REM Buscar Visual Studio
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% (
    echo ERROR: Visual Studio no encontrado
    echo Descarga Visual Studio Community desde:
    echo https://visualstudio.microsoft.com/downloads/
    pause
    exit /b 1
)

REM Obtener path de Visual Studio
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set VSPATH=%%i
)

echo Visual Studio encontrado en: %VSPATH%
echo.

REM Configurar entorno de Visual Studio
call "%VSPATH%\VC\Auxiliary\Build\vcvars32.bat"

echo.
echo Compilando recursos...
rc instapod.rc
if errorlevel 1 (
    echo ERROR: Fallo al compilar recursos
    pause
    exit /b 1
)

echo.
echo Compilando plugin...
cl /LD /O2 /EHsc gen_instapod.cpp instapod.res user32.lib gdi32.lib /Fe:gen_music4all.dll
if errorlevel 1 (
    echo ERROR: Fallo al compilar plugin
    pause
    exit /b 1
)

echo.
echo Compilando plugin Media Library...
cl /LD /O2 /EHsc ml_instapod.cpp instapod.res user32.lib gdi32.lib comctl32.lib shell32.lib /Fe:ml_music4all.dll
if errorlevel 1 (
    echo ERROR: Fallo al compilar plugin Media Library
    pause
    exit /b 1
)

echo.
echo =====================================================
echo Compilacion exitosa!
echo =====================================================
echo.
echo Archivo generado: gen_music4all.dll
echo Archivo generado: ml_music4all.dll
echo.
echo Instalacion:
echo 1. Copia gen_music4all.dll y ml_music4all.dll a: C:\Program Files (x86)\Winamp\Plugins\
echo 2. Copia music4all.py y instapod.py a: C:\Program Files (x86)\Winamp\Plugins\
echo 3. Reinicia Winamp
echo.

REM Limpiar archivos temporales
del *.obj 2>nul
del *.exp 2>nul
del *.lib 2>nul
del instapod.res 2>nul

pause
