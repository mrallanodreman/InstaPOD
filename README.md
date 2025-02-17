# InstaPOD

<table>
  <tr>
    <td>
      InstaPOD es un reproductor de música y gestor multimedia basado en PyQt6. Permite reproducir archivos locales (MP3) y buscar/reproducir canciones de YouTube. Además, incorpora un sistema de notificaciones (usando notify2) y un icono en la bandeja del sistema con opciones para abrir la ventana completa o salir de la aplicación.
    </td>
    <td align="right">
      <img src="https://i.postimg.cc/tRm0mKyK/Logito.png" alt="Logo de InstaPOD" width="800">
    </td>
  </tr>
</table>

## ⚡ Únete a nuestro servidor de Discord  

Intercambia ideas, colabora en proyectos y conéctate con otros desarrolladores y entusiastas de la tecnología.  

🔗 [Enlace de invitación](https://discord.gg/UGhbwxJy6s)  

Nos vemos dentro.  



## 📌 Características 
- **🎵 Reproducción local:** Carga y reproduce archivos MP3 almacenados en la carpeta `Musicpod`.
- **🔎 Búsqueda en YouTube:** Permite buscar canciones en YouTube usando `yt_dlp` y reproducirlas sin necesidad de descargarlas.
- **📻 Modo Radio:** Reproduce en modo radio utilizando los resultados de búsqueda de YouTube.
- **⚙️ Configuración:** Guarda opciones del usuario (por ejemplo, el volumen) mediante `QSettings`.
- **🔔 Notificaciones:** Muestra notificaciones personalizadas mediante `notify2` (en Linux) o las notificaciones integradas de Qt en Windows.
- **🖥️ Bandeja del sistema:** Integra un icono en la bandeja del sistema con opciones para "Abrir Completo" o "Salir".
- **🎨 Animaciones y efectos visuales:** Usa efectos de sombra para mostrar una portada (cover) atractiva.

## 🛠 Requisitos
- Python 3.6 o superior
- PyQt6
- yt-dlp
- notify2 (sólo en Linux; en Windows se usan las notificaciones integradas de Qt)
- mutagen (para extraer carátulas de archivos MP3)

⚠ **Nota (Linux):** Para usar `notify2` en sistemas Linux, asegúrate de tener instalado el paquete `python3-dbus`.

## 🚀 Instalación
### 1️⃣ Clonar el repositorio
Para obtener una copia local del proyecto, usa el siguiente comando:

```sh
 git clone https://github.com/tu_usuario/instapod.git
 cd instapod
```

### 2️⃣ Instalar dependencias
Ejecuta el siguiente comando para instalar las dependencias necesarias:

```sh
pip install -r requirements.txt
```

### 3️⃣ Ejecutar InstaPOD
Para iniciar la aplicación, ejecuta:

```sh
python main.py
```

## 🎛 Uso
1. **Reproducir archivos locales:**
   - Añade archivos MP3 a la carpeta `Musicpod`.
   - Abre la aplicación y selecciona la canción deseada.
2. **Buscar y reproducir desde YouTube:**
   - Usa la barra de búsqueda para encontrar una canción.
   - Haz clic en una canción para reproducirla en streaming.
3. **Modo radio:**
   - Busca un género o artista y activa el modo radio para reproducir canciones de forma continua.
4. **Control de volumen y configuraciones:**
   - Ajusta el volumen y otras configuraciones desde la interfaz.

## 🤝 Contribuir
Si deseas contribuir al proyecto, sigue estos pasos:
1. Haz un fork del repositorio.
2. Crea una nueva rama (`git checkout -b nueva-funcionalidad`).
3. Realiza tus modificaciones y haz commit (`git commit -m 'Añadir nueva funcionalidad'`).
4. Sube tus cambios (`git push origin nueva-funcionalidad`).
5. Crea un pull request.

