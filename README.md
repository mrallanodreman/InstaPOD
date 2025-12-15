# Music4All — Plugin de Winamp

[![Windows](https://img.shields.io/badge/OS-Windows-0078D4)](https://www.microsoft.com/windows)
![Winamp](https://img.shields.io/badge/Winamp-5.x-4CAF50)
![C++](https://img.shields.io/badge/C%2B%2B-Win32-00599C)

**Tags:** `winamp` `plugin` `windows` `media-library` `yt-dlp` `mp3`

Music4All es un **plugin para Winamp** que agrega un panel en **Media Library** para buscar y descargar audio desde YouTube usando `yt-dlp`, guardando como MP3 y añadiéndolo a Winamp.

Este proyecto seguirá evolucionando: se continuará añadiendo soporte a más plataformas, aplicaciones y dispositivos.

## Vista general

| Componente | Archivo | Descripción |
|---|---|---|
| Media Library | `ml_music4all.dll` | Panel integrado en el árbol de Media Library |
| General Purpose | `gen_music4all.dll` | Ventana de utilidad (configuración/acciones) |

## Requisitos

- Windows
- Winamp 5.x
- `yt-dlp.exe` (**obligatorio**: colócalo junto a los DLLs en la carpeta `Plugins` de Winamp)
- `ffmpeg` / `ffprobe` (**recomendado**; en la práctica suele ser necesario para convertir a MP3 y para carátulas/metadata)

## Índice

- [Vista general](#vista-general)
- [Requisitos](#requisitos)
- [Instalación (Winamp)](#instalación-winamp)
- [Compilar (Windows)](#compilar-windows)
- [Troubleshooting](#troubleshooting)
- [Roadmap (alto nivel)](#roadmap-alto-nivel)
- [Contribuir](#contribuir)

## Instalación (Winamp)

1. Compila los DLLs desde el código fuente (ver **Compilar**) o descárgalos desde *Releases*.
2. Copia a la carpeta de plugins de Winamp (normalmente `C:\Program Files (x86)\Winamp\Plugins\`):
   - `ml_music4all.dll`
   - `gen_music4all.dll`
   - `yt-dlp.exe` (**obligatorio**)
3. Reinicia Winamp.

## Descargar `yt-dlp.exe` (obligatorio)

Descarga `yt-dlp.exe` desde la página oficial de releases:

- https://github.com/yt-dlp/yt-dlp/releases

Luego copia `yt-dlp.exe` a la carpeta de plugins de Winamp, junto a los DLLs:

```
C:\Program Files (x86)\Winamp\Plugins\
```

## Instalar `ffmpeg` y `ffprobe` (recomendado)

Aunque algunas descargas pueden funcionar sin FFmpeg, **para extracción a MP3** y para funciones como **carátulas/miniaturas** y **procesamiento de audio**, normalmente necesitas `ffmpeg.exe` y `ffprobe.exe`.

Descarga builds para Windows desde fuentes conocidas:

- https://www.gyan.dev/ffmpeg/builds/
- https://github.com/BtbN/FFmpeg-Builds/releases

Instalación recomendada (simple):

1. Descomprime el ZIP.
2. Copia `ffmpeg.exe` y `ffprobe.exe` a:

```
C:\Program Files (x86)\Winamp\Plugins\
```

Alternativa: añade la carpeta `bin` de FFmpeg a tu **PATH**.

### Verificación rápida

- Abre Winamp → **Media Library** → debería aparecer **Music4All** en el árbol.

## Compilar (Windows)

### Opción recomendada (script)

1. Instala Visual Studio (Community sirve) con **Desktop development with C++**.
2. Ejecuta:

```bat
winamp_plugin\build.bat
```

Los DLLs se generan en `winamp_plugin\`.

## Troubleshooting

### El plugin no carga

- Compila en **x86 (32-bit)**, no x64.
- Asegúrate de estar usando Winamp 5.x.

### Descargas fallan

- Verifica que `yt-dlp.exe` esté en la misma carpeta `Plugins` que los DLLs.
- Instala `ffmpeg`/`ffprobe` o colócalos accesibles por PATH.

## Roadmap (alto nivel)

- Mejoras continuas del panel Media Library.
- Soporte gradual para más plataformas, aplicaciones y dispositivos.

## Contribuir

1. Haz un fork.
2. Crea una rama: `git checkout -b mi-cambio`.
3. Haz commits claros y descriptivos.
4. Abre un Pull Request.

