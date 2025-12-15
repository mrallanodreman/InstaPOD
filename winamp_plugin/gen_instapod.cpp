//=====================================================
// Music4All - Winamp Plugin (General Purpose)
// Descarga música de YouTube directamente a Winamp
//=====================================================

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "gen.h"
#include "wa_ipc.h"
#include "resource.h"

// Plugin metadata
#define PLUGIN_NAME "Music4All YouTube Downloader"
#define PLUGIN_VERSION "1.0"

// Handles
HWND hwndParent = NULL;
HWND hwndDialog = NULL;
HINSTANCE hInstance = NULL;

// Python process handle
HANDLE hPythonProcess = NULL;

// Forward declarations
void config();
void quit();
int init();
void show_window();
BOOL CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Winamp plugin structure
winampGeneralPurposePlugin plugin = {
    GPPHDR_VER,
    PLUGIN_NAME,
    init,
    config,
    quit,
};

//=====================================================
// Plugin initialization
//=====================================================
int init() {
    return 0;
}

//=====================================================
// Show configuration/main window
//=====================================================
void config() {
    if (!hwndDialog) {
        hwndDialog = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), 
                                   hwndParent, DialogProc);
    }
    
    if (hwndDialog) {
        ShowWindow(hwndDialog, SW_SHOW);
        SetForegroundWindow(hwndDialog);
    }
}

//=====================================================
// Launch Music4All Python script
//=====================================================
void LaunchMusic4All() {
    char pluginDir[MAX_PATH];
    char scriptPath[MAX_PATH];
    char commandLine[MAX_PATH * 2];
    
    // Get plugin directory
    GetModuleFileName(hInstance, pluginDir, MAX_PATH);
    char* lastSlash = strrchr(pluginDir, '\\');
    if (lastSlash) *lastSlash = 0;
    
    // Build paths (expect music4all.py copied next to the DLL)
    sprintf(scriptPath, "%s\\music4all.py", pluginDir);
    if (GetFileAttributesA(scriptPath) == INVALID_FILE_ATTRIBUTES) {
        // Backwards compatibility.
        sprintf(scriptPath, "%s\\instapod.py", pluginDir);
    }

    sprintf(commandLine, "python \"%s\"", scriptPath);
    
    // Launch Python process
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    
    if (CreateProcess(NULL, commandLine, NULL, NULL, FALSE, 
                     CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        hPythonProcess = pi.hProcess;
        CloseHandle(pi.hThread);
    }
}

//=====================================================
// Download from YouTube and add to playlist
//=====================================================
void DownloadAndAddToWinamp(const char* youtubeUrl) {
    char command[1024];
    char outputPath[MAX_PATH];
    
    // Get Winamp music directory
    SendMessage(hwndParent, WM_WA_IPC, (WPARAM)outputPath, IPC_GETPLAYLISTDIR);
    
    // Build yt-dlp command (embed thumbnail as cover art + add metadata for Winamp).
    sprintf(command, "yt-dlp --add-metadata --embed-thumbnail --convert-thumbnails jpg -x --audio-format mp3 -o \"%s\\%%(title)s.%%(ext)s\" \"%s\"",
            outputPath, youtubeUrl);
    
    // Execute download in background
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    
    if (CreateProcess(NULL, command, NULL, NULL, FALSE,
                     CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        // Wait for download to complete
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        // Refresh Winamp playlist
        SendMessage(hwndParent, WM_WA_IPC, 0, IPC_REFRESHPLCACHE);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

//=====================================================
// Dialog message handler
//=====================================================
BOOL CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch(uMsg) {
        case WM_INITDIALOG:
            return TRUE;
            
        case WM_COMMAND:
            switch(LOWORD(wParam)) {
                case IDC_BUTTON_DOWNLOAD: {
                    char url[512];
                    GetDlgItemText(hwndDlg, IDC_EDIT_URL, url, sizeof(url));
                    
                    if (strlen(url) > 0) {
                        SetDlgItemText(hwndDlg, IDC_STATUS, "Descargando...");
                        DownloadAndAddToWinamp(url);
                        SetDlgItemText(hwndDlg, IDC_STATUS, "¡Completado!");
                        SetDlgItemText(hwndDlg, IDC_EDIT_URL, "");
                    }
                    return TRUE;
                }
                
                case IDC_BUTTON_LAUNCH_INSTAPOD:
                    LaunchMusic4All();
                    return TRUE;
                    
                case IDCANCEL:
                    ShowWindow(hwndDlg, SW_HIDE);
                    return TRUE;
            }
            break;
            
        case WM_CLOSE:
            ShowWindow(hwndDlg, SW_HIDE);
            return TRUE;
    }
    return FALSE;
}

//=====================================================
// Cleanup on exit
//=====================================================
void quit() {
    if (hwndDialog) {
        DestroyWindow(hwndDialog);
        hwndDialog = NULL;
    }
    
    if (hPythonProcess) {
        TerminateProcess(hPythonProcess, 0);
        CloseHandle(hPythonProcess);
    }
}

//=====================================================
// DLL entry point
//=====================================================
extern "C" __declspec(dllexport) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin() {
    return &plugin;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        hInstance = hinstDLL;
    }
    return TRUE;
}
