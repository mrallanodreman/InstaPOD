# InstaPOD

InstaPOD es un reproductor de música y gestor multimedia basado en PyQt6. Permite reproducir archivos locales (MP3) y buscar/reproducir canciones de YouTube. Además, incorpora un sistema de notificaciones (usando notify2) y un icono en la bandeja del sistema con opciones para abrir la ventana completa o salir de la aplicación.

## Características

- **Reproducción local**: Carga y reproduce archivos MP3 almacenados en la carpeta `Musicpod`.
- **Búsqueda en YouTube**: Permite buscar canciones en YouTube usando `yt_dlp` y reproducirlas sin necesidad de descargarlas.
- **Modo Radio**: Reproduce en modo radio utilizando los resultados de búsqueda de YouTube.
- **Configuración**: Guarda opciones del usuario (por ejemplo, el volumen) mediante `QSettings`.
- **Notificaciones**: Muestra notificaciones personalizadas mediante `notify2` (en Linux) o las notificaciones integradas de Qt en Windows.
- **Bandeja del sistema**: Integra un icono en la bandeja del sistema con opciones para "Abrir Completo" o "Salir".
- **Animaciones y efectos visuales**: Usa efectos de sombra para mostrar una portada (cover) atractiva.

## Requisitos

- Python 3.6 o superior
- [PyQt6](https://www.riverbankcomputing.com/software/pyqt/intro)
- [yt-dlp](https://github.com/yt-dlp/yt-dlp)
- [notify2](https://pypi.org/project/notify2/) (sólo en Linux; en Windows se usan las notificaciones integradas de Qt)
- [mutagen](https://mutagen.readthedocs.io/en/latest/) (para extraer carátulas de archivos MP3)

> **Nota (Linux):** Para usar `notify2` en sistemas Linux, asegúrate de tener instalado el paquete `python3-dbus`.

## Instalación

1. **Clona el repositorio:**

   ```bash
   git clone https://github.com/tu_usuario/instapod.git
   cd instapod
