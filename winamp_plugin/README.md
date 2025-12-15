# Music4All - Plugin de Winamp

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
- `yt-dlp.exe` (recomendado: copiar junto a los DLLs en `Winamp\Plugins\`)
- `ffmpeg`/`ffprobe` (opcional pero recomendado)

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
- `yt-dlp.exe` (recomendado)

3. Reinicia Winamp.

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
