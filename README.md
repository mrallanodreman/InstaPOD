# Music4All (Winamp)

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
- `yt-dlp.exe` (recomendado: colócalo junto a los DLLs en la carpeta `Plugins` de Winamp)
- `ffmpeg` / `ffprobe` (opcional pero recomendado; mejora extracción de audio y manejo de miniaturas)

## Instalación (Winamp)

1. Compila los DLLs desde el código fuente (ver **Compilar**) o descárgalos desde *Releases*.
2. Copia a la carpeta de plugins de Winamp (normalmente `C:\Program Files (x86)\Winamp\Plugins\`):
   - `ml_music4all.dll`
   - `gen_music4all.dll`
   - `yt-dlp.exe` (recomendado)
3. Reinicia Winamp.

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

