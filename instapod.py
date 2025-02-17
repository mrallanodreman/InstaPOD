import sys
import os
import requests
import yt_dlp
import notify2


from PyQt6.QtCore import (
    Qt, QUrl, QObject, QRunnable, QThreadPool, pyqtSignal, QTimer, QSettings
)
from PyQt6.QtGui import QPixmap, QIcon, QAction
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget,
    QHBoxLayout, QVBoxLayout, QListWidget,
    QListWidgetItem, QLabel, QPushButton, QMessageBox,
    QLineEdit, QTabWidget, QSlider, QProgressBar, QGraphicsDropShadowEffect,
    QSystemTrayIcon, QMenu
)
from PyQt6.QtMultimedia import QMediaPlayer, QAudioOutput, QMediaDevices

# ------------------------
# Worker para tareas en segundo plano
# ------------------------
class WorkerSignals(QObject):
    finished = pyqtSignal(object)
    error = pyqtSignal(tuple)
    progress = pyqtSignal(int)

class Worker(QRunnable):
    def __init__(self, fn, *args, **kwargs):
        super().__init__()
        self.fn = fn
        self.args = args
        self.kwargs = kwargs
        self.signals = WorkerSignals()
    def run(self):
        try:
            result = self.fn(*self.args, **self.kwargs)
        except Exception as e:
            import traceback
            self.signals.error.emit((e, traceback.format_exc()))
        else:
            self.signals.finished.emit(result)

# ------------------------
# Funciones de ayuda
# ------------------------
def get_songs_from_musicpod():
    musicpod_dir = os.path.join(os.getcwd(), "Musicpod")
    if not os.path.exists(musicpod_dir):
        os.makedirs(musicpod_dir)
    songs = []
    for filename in os.listdir(musicpod_dir):
        if filename.lower().endswith(".mp3"):
            full_path = os.path.join(musicpod_dir, filename)
            song = {
                "title": os.path.splitext(filename)[0],
                "artist": "Desconocido",
                "cover": "",  # Se intentará extraer la carátula de los metadatos.
                "audio": full_path,
                "duration": "",
            }
            songs.append(song)
    return songs

def get_local_cover_art(file_path):
    try:
        from mutagen.id3 import ID3
        tags = ID3(file_path)
        for tag in tags.values():
            if tag.FrameID.startswith("APIC"):
                return tag.data
    except Exception:
        return None
    return None


# ------------------------
# Clase principal del reproductor
# ------------------------
class MusicPlayer(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("InstaPOD - Allan Odreman")
        self.resize(1000, 400)

        # Configuración para guardar opciones (volumen, etc.)
        self.settings = QSettings("User", "InstaPOD")

        # Icono de la aplicación
        icon_path = os.path.join(os.getcwd(), "Musicpod", "Logito.png")
        if os.path.exists(icon_path):
            self.setWindowIcon(QIcon(icon_path))

        self.threadpool = QThreadPool()

        # Variables de reproducción
        self.radio_mode = False
        self.radio_index = 0
        self.current_video_info = None

        # Carga de canciones locales
        self.songs_data = get_songs_from_musicpod()

        # Selección del dispositivo de audio predeterminado
        default_output = QMediaDevices.defaultAudioOutput()
        self.audio_output = QAudioOutput(default_output)

        # Interfaz principal
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.main_layout = QHBoxLayout(self.central_widget)

        # ----- Pestañas: Biblioteca, Buscar y Configuración -----
        self.tab_widget = QTabWidget()

        # Pestaña "Biblioteca" (local)
        self.library_tab = QWidget()
        self.library_layout = QVBoxLayout(self.library_tab)
        # Barra de búsqueda local
        self.local_search_line = QLineEdit()
        self.local_search_line.setPlaceholderText("Filtrar canciones locales...")
        self.local_search_line.textChanged.connect(self.filter_local_songs)
        self.library_layout.addWidget(self.local_search_line)
        # Lista de canciones locales
        self.song_list = QListWidget()
        self.song_list.currentRowChanged.connect(self.on_song_selected)
        self.library_layout.addWidget(self.song_list)
        self.update_library_list(self.songs_data)
        self.tab_widget.addTab(self.library_tab, QIcon.fromTheme("folder-music"), "Biblioteca")

        # Pestaña "Buscar" (YouTube)
        self.search_tab = QWidget()
        self.search_layout = QVBoxLayout(self.search_tab)
        self.search_bar_layout = QHBoxLayout()
        self.search_line = QLineEdit()
        self.search_line.setPlaceholderText("Buscar canciones en YouTube...")
        self.search_line.returnPressed.connect(self.perform_search)
        self.search_button = QPushButton()
        self.search_button.setIcon(QIcon.fromTheme("system-search"))
        self.search_button.clicked.connect(self.perform_search)
        self.search_bar_layout.addWidget(self.search_line)
        self.search_bar_layout.addWidget(self.search_button)
        self.search_layout.addLayout(self.search_bar_layout)
        self.search_list = QListWidget()
        self.search_list.itemDoubleClicked.connect(self.on_search_result_double_clicked)
        self.search_layout.addWidget(self.search_list)
        self.tab_widget.addTab(self.search_tab, QIcon.fromTheme("system-search"), "Buscar")

        # Pestaña "Configuración" (con emoji de engranaje)
        self.config_tab = QWidget()
        self.config_layout = QVBoxLayout(self.config_tab)
        config_label = QLabel("Configuración")
        config_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.config_layout.addWidget(config_label)
        # Ejemplo: widget para configurar el volumen
        self.volume_config = QLineEdit()
        self.volume_config.setPlaceholderText("Ej: 50")
        self.config_layout.addWidget(self.volume_config)
        # Aquí podrías agregar más opciones de configuración...
        self.tab_widget.addTab(self.config_tab, QIcon.fromTheme("preferences-system"), " Configuración")

        self.main_layout.addWidget(self.tab_widget, stretch=1)

        # ----- Panel Reproductor -----
        self.player_layout = QVBoxLayout()

        # Portada (cover) con efecto: se muestra por defecto el Logito.png
        self.cover_label = QLabel()
        self.cover_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.cover_label.setStyleSheet("border: 2px solid #444; border-radius: 5px; margin: 1px;")
        self.player_layout.addWidget(self.cover_label)

        # Título y Artista
        self.title_label = QLabel("Bienvenido a InstaPOD")
        self.title_label.setStyleSheet("font-size: 16px; font-weight: bold;")
        self.title_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.player_layout.addWidget(self.title_label)
        self.artist_label = QLabel("Artista")
        self.artist_label.setStyleSheet("color: #555; font-size: 14px;")
        self.artist_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.player_layout.addWidget(self.artist_label)

        # Controles de reproducción
        self.controls_layout = QHBoxLayout()
        self.prev_button = QPushButton("⏮")
        self.play_button = QPushButton("▶️")
        self.pause_button = QPushButton("⏸")
        self.next_button = QPushButton("⏭")
        self.download_button = QPushButton("⬇")
        self.prev_button.clicked.connect(self.play_previous)
        self.play_button.clicked.connect(self.play_song)
        self.pause_button.clicked.connect(self.pause_song)
        self.next_button.clicked.connect(self.next_track)
        self.download_button.clicked.connect(self.download_current_song)
        self.controls_layout.addWidget(self.prev_button)
        self.controls_layout.addWidget(self.play_button)
        self.controls_layout.addWidget(self.pause_button)
        self.controls_layout.addWidget(self.next_button)
        self.controls_layout.addWidget(self.download_button)
        self.player_layout.addLayout(self.controls_layout)

        # Botón Radio
        self.radio_layout = QHBoxLayout()
        self.radio_button = QPushButton("Radio: OFF")
        self.radio_button.setCheckable(True)
        self.radio_button.clicked.connect(self.toggle_radio_mode)
        self.radio_layout.addWidget(self.radio_button)
        self.player_layout.addLayout(self.radio_layout)

        # Slider de progreso y tiempo
        self.progress_slider = QSlider(Qt.Orientation.Horizontal)
        self.progress_slider.setRange(0, 0)
        self.progress_slider.sliderMoved.connect(self.seek_position)
        self.player_layout.addWidget(self.progress_slider)
        self.time_label = QLabel("00:00 / 00:00")
        self.time_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.player_layout.addWidget(self.time_label)

        # Barra de descarga
        self.download_progress_bar = QProgressBar()
        self.download_progress_bar.setRange(0, 100)
        self.download_progress_bar.hide()
        self.player_layout.addWidget(self.download_progress_bar)

        # Control de Volumen
        self.volume_label = QLabel("Volumen")
        self.volume_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.player_layout.addWidget(self.volume_label)
        self.volume_slider = QSlider(Qt.Orientation.Horizontal)
        self.volume_slider.setRange(0, 100)
        # Cargar el volumen guardado (por defecto 50 si no hay guardado)
        saved_volume = self.settings.value("volume", 50, type=int)
        self.volume_slider.setValue(saved_volume)
        self.volume_slider.valueChanged.connect(self.set_volume)
        self.volume_slider.setStyleSheet("""
            QSlider::groove:horizontal {
                height: 4px;
                background: #eee;
                border-radius: 2px;
            }
            QSlider::handle:horizontal {
                background: #666;
                width: 12px;
                margin: -4px 0;
                border-radius: 6px;
            }
        """)
        self.player_layout.addWidget(self.volume_slider)

        self.main_layout.addLayout(self.player_layout, stretch=2)

        # ----- Configurar QMediaPlayer -----
        self.player = QMediaPlayer()
        self.player.setAudioOutput(self.audio_output)
        self.player.positionChanged.connect(self.update_position)
        self.player.durationChanged.connect(self.update_duration)
        self.player.mediaStatusChanged.connect(self.handle_media_status)

        # Resultados de búsqueda (YouTube)
        self.search_results = []

        # Portada por defecto (Logito.png con efecto de sombra)
        self.set_default_cover()

        # Timer para refrescar la biblioteca local cada 5 segundos
        self.refresh_timer = QTimer(self)
        self.refresh_timer.timeout.connect(self.refresh_library)
        self.refresh_timer.start(5000)

        # ----- System Tray (Bandeja del sistema) -----
        self.tray_icon = QSystemTrayIcon(self)
        tray_icon_path = icon_path if os.path.exists(icon_path) else ""
        self.tray_icon.setIcon(QIcon(tray_icon_path) if tray_icon_path else QIcon())
        self.tray_icon.setToolTip("InstaPOD - Allan Odreman")
        self.tray_menu = QMenu()
        # Acción "Abrir Completo" para mostrar la ventana principal
        open_action = QAction("Abrir Completo", self)
        open_action.triggered.connect(self.show_main_window)
        self.tray_menu.addAction(open_action)
        # Acción "Salir"
        quit_action = QAction("Salir", self)
        quit_action.triggered.connect(self.close_app)
        self.tray_menu.addAction(quit_action)
        self.tray_icon.setContextMenu(self.tray_menu)
        self.tray_icon.show()

    def show_custom_notification(self, title: str, message: str):
        # Inicializa notify2 (esto se hace una vez, pero aquí lo incluimos para asegurar que esté inicializado)
        notify2.init("InstaPOD")
        # Define la ruta a tu icono (puedes usar "Logito.png")
        icon_path = os.path.join(os.getcwd(), "Musicpod", "Logito.png")
        # Crea la notificación
        n = notify2.Notification(title, message, icon_path)
        n.set_timeout(3000)  # Tiempo en milisegundos
        n.show()


    # -------------------------------------------------
    #       BÚSQUEDA LOCAL (Filtro)
    # -------------------------------------------------
    def filter_local_songs(self, text: str):
        text = text.lower().strip()
        if not text:
            self.update_library_list(self.songs_data)
            return
        filtered = [s for s in self.songs_data if text in s["title"].lower() or text in s["artist"].lower()]
        self.update_library_list(filtered)

    def update_library_list(self, songs=None):
        if songs is None:
            songs = self.songs_data
        self.song_list.clear()
        if songs:
            for song in songs:
                self.song_list.addItem(song["title"])
        else:
            empty_item = QListWidgetItem("Librería vacía")
            empty_item.setFlags(Qt.ItemFlag.NoItemFlags)
            self.song_list.addItem(empty_item)

    # -------------------------------------------------
    #       NOTIFICACIONES & BANDEJA DEL SISTEMA
    # -------------------------------------------------
    def close_app(self):
        """Cierra la aplicación, detiene la reproducción y finaliza los hilos en ejecución."""
        # Detener la reproducción de audio si está en curso
        if self.player.playbackState() == QMediaPlayer.PlaybackState.PlayingState:
            self.player.stop()

        # Detener la radio si está activada
        self.radio_mode = False  # Asegura que no se sigan cargando canciones de la radio

        # Detener cualquier hilo activo en la pool
        self.threadpool.waitForDone()  # Espera a que terminen todas las tareas en la cola

        # Ocultar el icono de la bandeja del sistema
        self.tray_icon.hide()

        # Cerrar la aplicación completamente
        self.close()

    def closeEvent(self, event):
        self.hide()
        if self.tray_icon.isVisible():
            self.show_custom_notification("InstaPOD", "InstaPOD se minimizó a la bandeja")
        event.ignore()

    def show_notification(self, title: str, message: str):
        if self.tray_icon.isVisible():
            self.tray_icon.showMessage(title, message, QSystemTrayIcon.MessageIcon.NoIcon, 3000)

    # -------------------------------------------------
    #       PORTADA POR DEFECTO
    # -------------------------------------------------
    def set_default_cover(self):
        default_icon_path = os.path.join(os.getcwd(), "Musicpod", "Logito.png")
        if os.path.exists(default_icon_path):
            pixmap = QPixmap(default_icon_path)
            pixmap = pixmap.scaled(220, 220, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation)
            self.cover_label.setPixmap(pixmap)
            shadow = QGraphicsDropShadowEffect()
            shadow.setBlurRadius(10)
            shadow.setOffset(5, 5)
            self.cover_label.setGraphicsEffect(shadow)
        else:
            self.cover_label.setText("No se encontró Logito.png")

    # -------------------------------------------------
    #       REPRODUCTOR LOCAL
    # -------------------------------------------------
    def load_song(self, index: int):
        if index < 0 or index >= len(self.songs_data):
            return
        self.current_index = index
        song = self.songs_data[index]
        if not song["cover"]:
            cover_data = get_local_cover_art(song["audio"])
            if cover_data:
                self.set_cover_from_data(cover_data)
            else:
                self.update_cover("")
        else:
            self.update_cover(song["cover"])
        self.title_label.setText(song["title"])
        self.artist_label.setText(song["artist"])
        media_url = QUrl.fromLocalFile(song["audio"])
        if not media_url.isValid():
            QMessageBox.warning(self, "Error", "Ruta de audio no válida.")
            return
        self.player.setSource(media_url)
        self.player.setAudioOutput(self.audio_output)
        self.show_notification("Reproduciendo", f"{song['title']}")

    def on_song_selected(self, row: int):
        if self.songs_data and row != -1:
            self.load_song(row)
            self.play_song()

    def play_song(self):
        self.player.play()

    def pause_song(self):
        self.player.pause()

    def play_next(self):
        new_index = (self.current_index + 1) % len(self.songs_data)
        self.load_song(new_index)
        self.play_song()

    def play_previous(self):
        new_index = (self.current_index - 1) % len(self.songs_data)
        self.load_song(new_index)
        self.play_song()

    # -------------------------------------------------
    #       ACTUALIZACIÓN DE COVER
    # -------------------------------------------------
    def update_cover(self, cover_url: str):
        if cover_url:
            try:
                resp = requests.get(cover_url, timeout=5)
                img_data = resp.content
                pixmap = QPixmap()
                pixmap.loadFromData(img_data)
            except Exception:
                pixmap = QPixmap(400, 400)
                pixmap.fill(Qt.GlobalColor.gray)
        else:
            pixmap = QPixmap(400, 400)
            pixmap.fill(Qt.GlobalColor.gray)
        pixmap = pixmap.scaled(400, 400, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation)
        self.cover_label.setPixmap(pixmap)

    def set_cover_from_data(self, data):
        pixmap = QPixmap()
        pixmap.loadFromData(data)
        pixmap = pixmap.scaled(400, 400, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation)
        self.cover_label.setPixmap(pixmap)

    # -------------------------------------------------
    #       POSICIÓN Y DURACIÓN
    # -------------------------------------------------
    def update_position(self, position):
        self.progress_slider.setValue(position)
        self.update_time_label(position, self.player.duration())

    def update_duration(self, duration):
        self.progress_slider.setRange(0, duration)
        self.update_time_label(self.player.position(), duration)

    def update_time_label(self, position, duration):
        def ms_to_mmss(ms):
            seconds = ms // 1000
            minutes = seconds // 60
            seconds = seconds % 60
            return f"{minutes:02d}:{seconds:02d}"
        self.time_label.setText(f"{ms_to_mmss(position)} / {ms_to_mmss(duration)}")

    def seek_position(self, position):
        self.player.setPosition(position)

    # -------------------------------------------------
    #       BÚSQUEDA EN YOUTUBE
    # -------------------------------------------------
    def perform_search(self):
        query = self.search_line.text().strip()
        if not query:
            return
        worker = Worker(self.search_youtube, query)
        worker.signals.finished.connect(self.search_finished)
        worker.signals.error.connect(self.worker_error)
        self.threadpool.start(worker)

    def search_youtube(self, query):
        ydl_opts = {'quiet': True, 'skip_download': True, 'extract_flat': True}
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            result = ydl.extract_info(f"ytsearch50:{query}", download=False)
            entries = result.get('entries', [])
        return entries

    def search_finished(self, entries):
        self.search_results = entries
        self.search_list.clear()
        for entry in entries:
            title = entry.get('title', 'Sin título')
            self.search_list.addItem(title)

    def worker_error(self, error_tuple):
        e, traceback_str = error_tuple
        QMessageBox.warning(self, "Error", f"Error: {e}\n{traceback_str}")

    def on_search_result_double_clicked(self, item):
        index = self.search_list.row(item)
        if index < 0 or index >= len(self.search_results):
            return
        video_entry = self.search_results[index]
        video_id = video_entry.get('id')
        if not video_id:
            QMessageBox.warning(self, "Error", "No se pudo obtener el ID del video.")
            return
        video_url = f"https://www.youtube.com/watch?v={video_id}"
        worker = Worker(self.extract_video_info, video_url)
        worker.signals.finished.connect(self.video_info_extracted)
        worker.signals.error.connect(self.worker_error)
        self.threadpool.start(worker)

    def extract_video_info(self, video_url):
        ydl_opts = {'quiet': True, 'format': 'bestaudio/best'}
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            video_info = ydl.extract_info(video_url, download=False)
        return video_info

    def video_info_extracted(self, video_info):
        self.current_video_info = video_info
        audio_url = video_info.get('url')
        self.title_label.setText(video_info.get('title', 'Sin título'))
        self.artist_label.setText("YouTube")
        thumbnail_url = video_info.get('thumbnail', '')
        self.update_cover(thumbnail_url)
        self.player.setSource(QUrl(audio_url))
        self.player.setAudioOutput(self.audio_output)
        self.player.play()
        self.show_notification("Reproduciendo desde YouTube", video_info.get('title', 'Sin título'))

    # -------------------------------------------------
    #       MODO RADIO
    # -------------------------------------------------
    def toggle_radio_mode(self):
        self.radio_mode = not self.radio_mode
        if self.radio_mode:
            self.radio_button.setText("Radio: ON")
            if self.search_results:
                self.radio_index = 0
                self.play_next_radio_track()
            else:
                QMessageBox.information(self, "Radio", "No hay resultados de búsqueda para reproducir en modo radio.")
                self.radio_mode = False
                self.radio_button.setText("Radio: OFF")
        else:
            self.radio_button.setText("Radio: OFF")

    def handle_media_status(self, status):
        if status == QMediaPlayer.MediaStatus.EndOfMedia and self.radio_mode:
            self.play_next_radio_track()

    def play_next_radio_track(self):
        if not self.search_results:
            return
        self.radio_index = (self.radio_index + 1) % len(self.search_results)
        video_entry = self.search_results[self.radio_index]
        video_id = video_entry.get('id')
        if not video_id:
            return
        video_url = f"https://www.youtube.com/watch?v={video_id}"
        worker = Worker(self.extract_video_info, video_url)
        worker.signals.finished.connect(self.video_info_extracted)
        worker.signals.error.connect(self.worker_error)
        self.threadpool.start(worker)

    # -------------------------------------------------
    #       BOTÓN SIGUIENTE
    # -------------------------------------------------
    def next_track(self):
        """
        Si está en modo radio y en la pestaña "Buscar", avanza en la lista de YouTube.
        De lo contrario, avanza en la biblioteca local.
        """
        if self.radio_mode and self.tab_widget.currentIndex() == 1 and self.search_results:
            self.play_next_radio_track()
        else:
            self.play_next()

    def play_next(self):
        new_index = (self.current_index + 1) % len(self.songs_data)
        self.load_song(new_index)
        self.play_song()

    # -------------------------------------------------
    #       DESCARGA
    # -------------------------------------------------
    def download_current_song(self):
        source = self.player.source()
        if source.scheme() == "file":
            QMessageBox.information(self, "Descarga", "La canción ya está en la biblioteca.")
            return
        if not self.current_video_info:
            QMessageBox.warning(self, "Descarga", "No hay información del video para descargar.")
            return
        video_url = self.current_video_info.get('webpage_url')
        if not video_url:
            QMessageBox.warning(self, "Descarga", "No se pudo obtener la URL del video.")
            return
        download_folder = os.path.join(os.getcwd(), "Musicpod")
        if not os.path.exists(download_folder):
            os.makedirs(download_folder)
        ydl_opts = {
            'format': 'bestaudio/best',
            'outtmpl': os.path.join(download_folder, '%(title)s.%(ext)s'),
            'postprocessors': [
                {'key': 'FFmpegExtractAudio', 'preferredcodec': 'mp3', 'preferredquality': '192'},
                {'key': 'EmbedThumbnail'}
            ],
            'writethumbnail': True,
            'addmetadata': True,
            'quiet': True,
        }
        self.download_progress_bar.setValue(0)
        self.download_progress_bar.show()

        def progress_hook(info):
            if info.get('status') == 'downloading':
                total = info.get('total_bytes') or info.get('total_bytes_estimate')
                if total:
                    downloaded = info.get('downloaded_bytes', 0)
                    percent = int(downloaded * 100 / total)
                    worker.signals.progress.emit(percent)
            elif info.get('status') == 'finished':
                worker.signals.progress.emit(100)

        worker = Worker(self.download_song, video_url, ydl_opts, progress_callback=progress_hook)
        worker.signals.progress.connect(self.update_download_progress)
        worker.signals.finished.connect(self.download_complete)
        worker.signals.error.connect(self.worker_error)
        self.threadpool.start(worker)

    def download_song(self, video_url, ydl_opts, progress_callback):
        ydl_opts['progress_hooks'] = [progress_callback]
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            ydl.download([video_url])
        return True

    def update_download_progress(self, value):
        self.download_progress_bar.setValue(value)

    def download_complete(self, result):
        self.download_progress_bar.setFormat("Descarga completada.")
        self.show_notification("Descarga completada", "La canción se guardó en Musicpod.")
        QTimer.singleShot(2000, lambda: self.download_progress_bar.hide())

    def set_volume(self, value):
        # Convertir valor (0-100) a flotante (0.0-1.0)
        self.audio_output.setVolume(value / 100.0)
        # Guardar el valor en QSettings
        self.settings.setValue("volume", value)

    # --- Refrescar la biblioteca ---
    def refresh_library(self):
        new_songs = get_songs_from_musicpod()
        if len(new_songs) != len(self.songs_data):
            self.songs_data = new_songs
            self.update_library_list()

    # ----- Método para mostrar la ventana completa desde la bandeja -----
    def show_main_window(self):
        self.showNormal()
        self.activateWindow()

def main():
    app = QApplication(sys.argv)
    window = MusicPlayer()
    window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
