# Music4All — Plugin de Winamp (Win32)

[![Windows](https://img.shields.io/badge/OS-Windows-0078D4)](https://www.microsoft.com/windows)
![Winamp](https://img.shields.io/badge/Winamp-5.x-4CAF50)
![C++](https://img.shields.io/badge/C%2B%2B-Win32-00599C)

**Tags:** `winamp` `plugin` `windows` `media-library` `yt-dlp`

Music4All es un **plugin para Winamp 5.x** que agrega integración para buscar/descargar audio desde YouTube (vía `yt-dlp`) y añadirlo a Winamp.

## Componentes

| Componente | DLL | Dónde aparece |
|---|---|---|
| Media Library | `ml_music4all.dll` | Árbol y panel dentro de **Media Library** |
| General Purpose | `gen_music4all.dll` | Preferences → Plug-ins → **General Purpose** |

## Requisitos

- Windows
- Winamp 5.x
- Visual Studio (Community sirve) con **Desktop development with C++**
- `yt-dlp.exe` (**obligatorio**: copiar junto a los DLLs en `Winamp\Plugins\`)
- `ffmpeg`/`ffprobe` (**recomendado**; normalmente necesario para extracción a MP3 y carátulas/metadata)

## Compilación

### Opción recomendada (script)

Ejecuta el script de build:

```bat
winamp_plugin\build.bat
```

Salida esperada en `winamp_plugin\`:

- `ml_music4all.dll`
- `gen_music4all.dll`

## Instalación

1. Copia a la carpeta de plugins de Winamp:

```
C:\Program Files (x86)\Winamp\Plugins\
```

2. Archivos a copiar:

- `ml_music4all.dll`
- `gen_music4all.dll`
- `yt-dlp.exe` (**obligatorio**)

3. Reinicia Winamp.

## Descargar `yt-dlp.exe` (obligatorio)

Descarga `yt-dlp.exe` desde la página oficial de releases:

- https://github.com/yt-dlp/yt-dlp/releases

Colócalo en la carpeta `Winamp\Plugins\` junto a los DLLs:

```
C:\Program Files (x86)\Winamp\Plugins\
```

## Instalar `ffmpeg` y `ffprobe` (recomendado)

Para extracción a MP3 y para el procesamiento de audio/miniaturas, `yt-dlp` suele requerir `ffmpeg.exe` y `ffprobe.exe`.

Descarga builds para Windows:

- https://www.gyan.dev/ffmpeg/builds/
- https://github.com/BtbN/FFmpeg-Builds/releases

Instalación recomendada:

1. Descomprime el ZIP.
2. Copia `ffmpeg.exe` y `ffprobe.exe` a `Winamp\Plugins\` junto a los DLLs.

Ruta típica:

```
C:\Program Files (x86)\Winamp\Plugins\
```

### Alternativa (instalación automática)

- PowerShell: `install_to_winamp.ps1`
- Admin (UAC): `install_plugins_admin.bat`

## Uso

- **Media Library**: abre el panel **Music4All** desde el árbol.
- **General Purpose**: Preferences → Plug-ins → General Purpose → **Music4All YouTube Downloader** → Configure.

## Troubleshooting

### El plugin no carga

- Compila en **x86 (32-bit)**.
- Asegúrate de que Winamp sea 5.x.

### Fallan descargas

- Coloca `yt-dlp.exe` en `Winamp\Plugins\` junto a los DLLs.
- Asegura `ffmpeg`/`ffprobe` instalados o accesibles por PATH.
