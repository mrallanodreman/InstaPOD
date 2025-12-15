# Music4All (Winamp)

Music4All es un **plugin para Winamp**.

Agrega un panel en **Media Library** para buscar y descargar audio desde YouTube usando `yt-dlp`, guardando como MP3 y añadiéndolo a Winamp.

Este proyecto seguirá evolucionando: iremos añadiendo soporte a más plataformas, aplicaciones y dispositivos.

## Requisitos

- Windows
- Winamp 5.x
- `yt-dlp.exe` (recomendado: colócalo junto a los DLLs en la carpeta `Plugins` de Winamp)
- `ffmpeg` / `ffprobe` (opcional pero recomendado; ayuda a `yt-dlp` con extracción de audio y miniaturas)

## Instalación

1. Compila los DLLs desde el código fuente (ver **Compilar**) o descárgalos desde *Releases*.
2. Copia estos archivos a la carpeta de plugins de Winamp (normalmente `C:\Program Files (x86)\Winamp\Plugins\`):
   - `ml_music4all.dll`
   - `gen_music4all.dll`
   - `yt-dlp.exe` (recomendado)
3. Reinicia Winamp.

Deberías ver **Music4All** dentro del árbol de **Media Library**.

## Compilar

1. Instala Visual Studio (Community sirve) con *Desktop development with C++*.
2. Ejecuta:
   - `winamp_plugin\build.bat`
3. Copia los DLLs generados como se indica en **Instalación**.

## Notas

- Las descargas se realizan con `yt-dlp`.
- Los nombres de archivo se normalizan para ser compatibles con Windows.

## Contribuir

1. Haz un fork del repositorio.
2. Crea una rama (`git checkout -b mi-cambio`).
3. Realiza tus cambios y haz commit.
4. Abre un Pull Request.

