# InstaPOD - Plugin de Winamp
## Descarga música de YouTube directamente en Winamp

### 📋 Requisitos

1. **Visual Studio** (Community Edition es gratis)
2. **Python 3** instalado en el sistema
3. **yt-dlp** instalado: `pip install yt-dlp`
4. **Winamp** instalado

### 🔨 Compilación

#### Opción 1: Visual Studio GUI

1. Abre Visual Studio
2. File → New → Project from Existing Code
3. Selecciona la carpeta `winamp_plugin`
4. Tipo de proyecto: Visual C++
5. Configuración:
   - Output type: **DLL**
   - Output name: **gen_instapod.dll**
   - Configuration: **Release, x86**

6. Click derecho en el proyecto → Properties:
   - C/C++ → General → Additional Include Directories: `.`
   - Linker → Input → Additional Dependencies: `user32.lib gdi32.lib`
   
7. Build → Build Solution (F7)

#### Opción 2: Línea de comandos (Developer Command Prompt)

```cmd
cd c:\Users\Hobeat\InstaPOD-main\winamp_plugin
rc instapod.rc
cl /LD /O2 gen_instapod.cpp instapod.res user32.lib gdi32.lib /Fe:gen_instapod.dll
```

### 📦 Instalación

1. Copia `gen_instapod.dll` a la carpeta de plugins de Winamp:
   ```
   C:\Program Files (x86)\Winamp\Plugins\
   ```

2. Copia `instapod.py` (el archivo principal) a la misma carpeta:
   ```
   C:\Program Files (x86)\Winamp\Plugins\
   ```

3. Reinicia Winamp

4. Ve a: **Options → Preferences → Plug-ins → General Purpose**

5. Verás **"InstaPOD YouTube Downloader"** en la lista

6. Selecciónalo y haz clic en **"Configure"**

### 🎵 Uso

**Desde Winamp:**
1. Abre el plugin (Options → Preferences → Plug-ins → General Purpose → InstaPOD)
2. Pega una URL de YouTube
3. Click en "Descargar y Agregar a Winamp"
4. La canción se descarga como MP3 y se agrega automáticamente a tu biblioteca

**Atajo de teclado (opcional):**
- Puedes asignar un hotkey en Winamp para abrir el plugin rápidamente

### ⚡ Características

✅ Descarga directa desde YouTube
✅ Conversión automática a MP3
✅ Integración nativa con Winamp
✅ Agrega canciones a la playlist automáticamente
✅ Botón para abrir InstaPOD completo
✅ Interfaz simple y rápida

### 🔧 Compilación Alternativa (MinGW)

Si prefieres MinGW en lugar de Visual Studio:

```bash
windres instapod.rc -O coff -o instapod.res
g++ -shared -o gen_instapod.dll gen_instapod.cpp instapod.res -luser32 -lgdi32 -mwindows -s -O2
```

### 📝 Notas

- El plugin requiere que Python y yt-dlp estén en el PATH del sistema
- Las descargas se guardan en la carpeta de música de Winamp
- Puedes personalizar la carpeta de descarga editando el código

### 🐛 Troubleshooting

**"No se puede cargar el plugin":**
- Asegúrate de compilar para x86 (32-bit), no x64
- Verifica que todas las DLLs de Visual C++ Runtime estén instaladas

**"Python no encontrado":**
- Agrega Python al PATH del sistema
- O edita el código para usar una ruta absoluta a python.exe

**"yt-dlp no funciona":**
```cmd
pip install --upgrade yt-dlp
```
