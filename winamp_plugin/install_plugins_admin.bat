@echo off
setlocal
cd /d "%~dp0"
echo.
echo ===============================================
echo  Music4All - Install Winamp Plugins (Admin)
echo ===============================================
echo.

set "WINAMP_PLUGINS=C:\Program Files (x86)\Winamp\Plugins"

if not exist "%WINAMP_PLUGINS%\" (
  echo [ERROR] No se encontro la carpeta de Winamp Plugins:
  echo         %WINAMP_PLUGINS%
  pause
  exit /b 1
)

echo Copiando DLLs a: %WINAMP_PLUGINS%
echo (Se solicitara UAC / permisos de administrador)
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -Verb RunAs -FilePath 'powershell' -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-Command', 'Copy-Item -Force -LiteralPath ''%CD%\ml_music4all.dll'' -Destination ''%WINAMP_PLUGINS%\ml_music4all.dll''; Copy-Item -Force -LiteralPath ''%CD%\gen_music4all.dll'' -Destination ''%WINAMP_PLUGINS%\gen_music4all.dll''; Copy-Item -Force -LiteralPath ''%CD%\..\music4all.py'' -Destination ''%WINAMP_PLUGINS%\music4all.py''; Copy-Item -Force -LiteralPath ''%CD%\..\instapod.py'' -Destination ''%WINAMP_PLUGINS%\instapod.py''; Write-Host ''OK: Plugins copiados.''')"

echo.
echo Listo. Reinicia Winamp.
echo.
pause
