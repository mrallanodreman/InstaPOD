# Music4All (Winamp)

This is the **Winamp plugin** for Music4All.

Music4All adds a Media Library panel to search and download audio from YouTube via `yt-dlp`, saving files as MP3 and adding them to Winamp.

We’ll keep expanding support to more apps, platforms, and devices over time.

This repository also includes an optional Python “companion” UI.

## Requirements

- Windows
- Winamp (5.x)
- `yt-dlp.exe` (recommended: place next to the plugin DLLs in Winamp’s `Plugins` folder)
- `ffmpeg` / `ffprobe` (optional but recommended; used by `yt-dlp` for audio extraction and thumbnails)

## Install

1. Build the DLLs from source (see **Build from source**) or download them from your project releases.
2. Copy these files into your Winamp `Plugins` folder (typically `C:\Program Files (x86)\Winamp\Plugins\`):
   - `ml_music4all.dll` (built or downloaded)
   - `gen_music4all.dll` (built or downloaded)
   - `music4all.py`
   - `instapod.py` (implementation used by `music4all.py`)
3. Restart Winamp.

You should see **Music4All** in the Winamp Media Library tree.

## Build from source

1. Install Visual Studio (Community is fine) with C++ desktop tools.
2. Run:
   - `winamp_plugin\build.bat`
3. Copy the generated DLLs (and the Python files) as described in **Install**.

## Optional: Python companion app

The companion can be launched from inside Winamp (button “Abrir Music4All”) or directly:

```bat
python music4all.py
```

Python dependencies are listed in `requirements.txt`.

## Notes

- Downloads are performed via `yt-dlp`.
- Output files are saved with Windows-safe filenames.
Si deseas contribuir al proyecto, sigue estos pasos:
1. Haz un fork del repositorio.
2. Crea una nueva rama (`git checkout -b nueva-funcionalidad`).
3. Realiza tus modificaciones y haz commit (`git commit -m 'Añadir nueva funcionalidad'`).
4. Sube tus cambios (`git push origin nueva-funcionalidad`).
5. Crea un pull request.

