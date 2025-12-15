#!/usr/bin/env python3
"""
InstaPOD - Winamp Companion
Descarga música de YouTube y la agrega automáticamente a Winamp
"""

import os
import sys
import time
import subprocess
import tkinter as tk
from tkinter import ttk, messagebox
import threading
import yt_dlp
import winreg

class WinampCompanion:
    def __init__(self):
        self.window = tk.Tk()
        self.window.title("InstaPOD - Winamp Companion")
        self.window.geometry("500x300")
        self.window.configure(bg='#2b2b2b')
        
        # Get Winamp path
        self.winamp_path = self.get_winamp_path()
        self.music_folder = self.get_winamp_music_folder()
        
        self.setup_ui()
        
    def get_winamp_path(self):
        """Obtener la ruta de instalación de Winamp desde el registro"""
        try:
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Winamp")
            value, _ = winreg.QueryValueEx(key, "")
            winreg.CloseKey(key)
            return value
        except:
            try:
                key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Winamp")
                value, _ = winreg.QueryValueEx(key, "")
                winreg.CloseKey(key)
                return value
            except:
                return r"C:\Program Files (x86)\Winamp"
    
    def get_winamp_music_folder(self):
        """Obtener la carpeta de música de Winamp"""
        default_folder = os.path.join(os.path.expanduser("~"), "Music", "Winamp")
        
        if self.winamp_path and os.path.exists(self.winamp_path):
            winamp_music = os.path.join(self.winamp_path, "Music")
            if os.path.exists(winamp_music):
                return winamp_music
        
        if not os.path.exists(default_folder):
            os.makedirs(default_folder, exist_ok=True)
        
        return default_folder
    
    def setup_ui(self):
        """Configurar la interfaz"""
        # Title
        title = tk.Label(
            self.window,
            text="🎵 InstaPOD Winamp Companion",
            font=("Segoe UI", 16, "bold"),
            bg='#2b2b2b',
            fg='white'
        )
        title.pack(pady=15)
        
        # Info frame
        info_frame = tk.Frame(self.window, bg='#3a3a3a', relief=tk.FLAT, bd=1)
        info_frame.pack(fill=tk.X, padx=20, pady=5)
        
        info_label = tk.Label(
            info_frame,
            text=f"📁 Carpeta de música: {self.music_folder}",
            font=("Segoe UI", 9),
            bg='#3a3a3a',
            fg='#aaa',
            anchor='w',
            padx=10,
            pady=5
        )
        info_label.pack(fill=tk.X)
        
        # URL input
        url_label = tk.Label(
            self.window,
            text="URL de YouTube o búsqueda:",
            font=("Segoe UI", 10),
            bg='#2b2b2b',
            fg='white'
        )
        url_label.pack(pady=(10, 5))
        
        self.url_entry = tk.Entry(
            self.window,
            font=("Segoe UI", 10),
            bg='#3a3a3a',
            fg='white',
            insertbackground='white',
            relief=tk.FLAT,
            bd=5
        )
        self.url_entry.pack(fill=tk.X, padx=20)
        self.url_entry.bind('<Return>', lambda e: self.download())
        
        # Buttons frame
        btn_frame = tk.Frame(self.window, bg='#2b2b2b')
        btn_frame.pack(pady=15)
        
        self.download_btn = tk.Button(
            btn_frame,
            text="⬇ Descargar y Agregar",
            command=self.download,
            font=("Segoe UI", 10, "bold"),
            bg='#667eea',
            fg='white',
            activebackground='#5568d3',
            activeforeground='white',
            relief=tk.FLAT,
            bd=0,
            padx=20,
            pady=10,
            cursor='hand2'
        )
        self.download_btn.pack(side=tk.LEFT, padx=5)
        
        open_folder_btn = tk.Button(
            btn_frame,
            text="📁 Abrir Carpeta",
            command=self.open_music_folder,
            font=("Segoe UI", 10),
            bg='#3a3a3a',
            fg='white',
            activebackground='#4a4a4a',
            activeforeground='white',
            relief=tk.FLAT,
            bd=0,
            padx=20,
            pady=10,
            cursor='hand2'
        )
        open_folder_btn.pack(side=tk.LEFT, padx=5)
        
        # Status
        self.status_label = tk.Label(
            self.window,
            text="Listo para descargar",
            font=("Segoe UI", 9),
            bg='#2b2b2b',
            fg='#aaa'
        )
        self.status_label.pack(pady=10)
        
        # Progress bar
        self.progress = ttk.Progressbar(
            self.window,
            mode='indeterminate',
            length=400
        )
        
    def update_status(self, text, color='#aaa'):
        """Actualizar el texto de estado"""
        self.status_label.config(text=text, fg=color)
        self.window.update()
    
    def download(self):
        """Descargar música de YouTube"""
        url = self.url_entry.get().strip()
        
        if not url:
            messagebox.showwarning("URL vacía", "Por favor ingresa una URL de YouTube")
            return
        
        # Disable button
        self.download_btn.config(state='disabled')
        self.progress.pack(pady=10)
        self.progress.start(10)
        
        # Run download in thread
        thread = threading.Thread(target=self._download_thread, args=(url,))
        thread.daemon = True
        thread.start()
    
    def _download_thread(self, url):
        """Thread para descargar"""
        try:
            self.update_status("Descargando...", '#667eea')
            
            # Find FFmpeg location
            ffmpeg_locations = [
                os.path.join(os.environ.get('LOCALAPPDATA', ''), 'Microsoft', 'WinGet', 'Packages'),
                r"C:\ffmpeg\bin",
                r"C:\Program Files\ffmpeg\bin",
            ]
            
            ffmpeg_path = None
            for location in ffmpeg_locations:
                if os.path.exists(location):
                    for root, dirs, files in os.walk(location):
                        if 'ffmpeg.exe' in files:
                            ffmpeg_path = root
                            break
                    if ffmpeg_path:
                        break
            
            ydl_opts = {
                'format': 'bestaudio/best',
                'outtmpl': os.path.join(self.music_folder, '%(title)s.%(ext)s'),
                'postprocessors': [
                    {
                        'key': 'FFmpegExtractAudio',
                        'preferredcodec': 'mp3',
                        'preferredquality': '192',
                    },
                    {
                        'key': 'FFmpegMetadata',
                        'add_metadata': True,
                    },
                    {
                        'key': 'EmbedThumbnail',
                        'already_have_thumbnail': False,
                    }
                ],
                'writethumbnail': True,
                'embedthumbnail': True,
                'quiet': False,
                'no_warnings': False,
            }
            
            if ffmpeg_path:
                ydl_opts['ffmpeg_location'] = ffmpeg_path
            
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                info = ydl.extract_info(url, download=True)
                title = info.get('title', 'Canción')
                artist = info.get('artist') or info.get('uploader', 'Desconocido')
                album = info.get('album', 'YouTube')
                
                # Obtener el archivo descargado
                filename = ydl.prepare_filename(info)
                mp3_file = os.path.splitext(filename)[0] + '.mp3'
            
            # Escribir metadatos adicionales con mutagen
            self._write_metadata(mp3_file, info)
            
            self.window.after(0, self._download_complete, title)
            
        except Exception as e:
            self.window.after(0, self._download_error, str(e))
    
    def _download_complete(self, title):
        """Callback cuando descarga completa"""
        self.progress.stop()
        self.progress.pack_forget()
        self.download_btn.config(state='normal')
        self.url_entry.delete(0, tk.END)
        self.update_status(f"✓ Descargado: {title}", '#4ade80')
        
        # Try to add to Winamp
        self.add_to_winamp()
        
        messagebox.showinfo("Éxito", f"Descargado: {title}\n\nLa canción está en: {self.music_folder}")
    
    def _download_error(self, error):
        """Callback cuando hay error"""
        self.progress.stop()
        self.progress.pack_forget()
        self.download_btn.config(state='normal')
        self.update_status(f"✗ Error: {error[:50]}...", '#ef4444')
        messagebox.showerror("Error", f"Error al descargar:\n{error}")
    
    def _write_metadata(self, mp3_file, info):
        """Escribir metadatos ID3 correctos al MP3"""
        try:
            from mutagen.id3 import ID3, TIT2, TPE1, TALB, TDRC, COMM, APIC
            from mutagen.mp3 import MP3
            
            if not os.path.exists(mp3_file):
                return
            
            # Cargar archivo MP3
            audio = MP3(mp3_file, ID3=ID3)
            
            # Agregar o actualizar tags ID3
            if audio.tags is None:
                audio.add_tags()
            
            # Título
            title = info.get('title', 'Desconocido')
            audio.tags.add(TIT2(encoding=3, text=title))
            
            # Artista
            artist = info.get('artist') or info.get('uploader', 'Desconocido')
            audio.tags.add(TPE1(encoding=3, text=artist))
            
            # Álbum
            album = info.get('album', 'YouTube')
            audio.tags.add(TALB(encoding=3, text=album))
            
            # Año
            upload_date = info.get('upload_date')
            if upload_date:
                year = upload_date[:4]
                audio.tags.add(TDRC(encoding=3, text=year))
            
            # Descripción/Comentario
            description = info.get('description', '')
            if description:
                audio.tags.add(COMM(encoding=3, lang='eng', desc='', text=description[:200]))
            
            # Portada (thumbnail)
            thumbnail_file = os.path.splitext(mp3_file)[0] + '.jpg'
            if os.path.exists(thumbnail_file):
                with open(thumbnail_file, 'rb') as img:
                    audio.tags.add(
                        APIC(
                            encoding=3,
                            mime='image/jpeg',
                            type=3,  # Cover (front)
                            desc='Cover',
                            data=img.read()
                        )
                    )
                # Eliminar archivo temporal de thumbnail
                try:
                    os.remove(thumbnail_file)
                except:
                    pass
            
            # Guardar cambios
            audio.save()
            
        except Exception as e:
            print(f"Error escribiendo metadatos: {e}")
    
    def add_to_winamp(self):
        """Actualizar biblioteca de Winamp"""
        try:
            # Refresh Winamp's media library
            winamp_exe = os.path.join(self.winamp_path, "winamp.exe")
            if os.path.exists(winamp_exe):
                subprocess.Popen([winamp_exe, "/REFRESH"])
        except:
            pass
    
    def open_music_folder(self):
        """Abrir carpeta de música"""
        os.startfile(self.music_folder)
    
    def run(self):
        """Ejecutar la aplicación"""
        self.window.mainloop()

if __name__ == "__main__":
    app = WinampCompanion()
    app.run()
