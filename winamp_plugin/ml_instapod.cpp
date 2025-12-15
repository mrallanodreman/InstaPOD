//=====================================================
// Music4All - Winamp Media Library Plugin (ml_*)
// Provides a left-panel entry inside Winamp's Media Library.
//=====================================================

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shlobj.h>
#include <vector>
#include <string>

#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif

#include "resource.h"
#include "wa_ipc.h"

// ---- Minimal Media Library plugin ABI (small subset) ----
// These values match Winamp/ML SDK public headers.
#define MLHDR_VER 0x17
#define WM_ML_IPC (WM_USER + 0x1000)

// Old tree item API (widely supported; sufficient for a left-panel node)
#define ML_IPC_ADDTREEITEM 0x0101
#define ML_IPC_DELTREEITEM 0x0103
#define ML_IPC_GETCURTREEITEM 0x0104

// View helpers
#define ML_IPC_GETCURRENTVIEW 0x0090

// Tree image / tree item APIs (Winamp 5.3+)
#define ML_IPC_TREEITEM_ADDW 0x0133
#define ML_IPC_TREEITEM_DELETE 0x0127
#define ML_IPC_TREEIMAGE_ADD 0x0140

// Skinning helpers (match Winamp skin / Big Bento)
#define ML_IPC_SKIN_LISTVIEW 0x0500
#define ML_IPC_UNSKIN_LISTVIEW 0x0501

// Dialog skin helpers
#define ML_IPC_SKIN_WADLG_GETFUNC 0x0600

// Notifications delivered via MessageProc
#define ML_MSG_TREE_ONCREATEVIEW 0x100

typedef struct {
    INT_PTR parent_id;
    char *title;
    int has_children;
    INT_PTR this_id; // filled by gen_ml
} mlAddTreeItemStruct;

typedef struct {
    int version;
    char *description;
    int(__cdecl *init)();
    void(__cdecl *quit)();
    INT_PTR(__cdecl *MessageProc)(int message_type, INT_PTR p1, INT_PTR p2, INT_PTR p3);
    HWND hwndWinampParent;
    HWND hwndLibraryParent;
    HINSTANCE hDllInstance;
    void *service; // Winamp 5.66+ (safe to include for MLHDR_VER)
} winampMediaLibraryPlugin;

static HWND g_hwndDialog = NULL;
static HWND g_hwndList = NULL;
static HWND g_hwndDownloads = NULL;
static INT_PTR g_listSkinHandle = 0;
static INT_PTR g_downloadsSkinHandle = 0;
static INT_PTR g_treeId = 0;
static bool g_treeApiNew = false;
static winampMediaLibraryPlugin g_plugin = {0};

typedef void (*BMPFILTERPROC)(const void *, const void *, void *);

typedef struct {
    HINSTANCE hinst;
    int resourceId;
    int imageIndex; // -1 to allocate
    BMPFILTERPROC filterProc; // NULL = no filter
    int width;  // reserved
    int height; // reserved
} MLTREEIMAGE;

typedef struct {
    size_t size;      // sizeof(MLTREEITEMW)
    UINT_PTR id;      // output
    UINT_PTR parentId;
    wchar_t *title;
    size_t titleLen;
    BOOL hasChildren;
    int imageIndex;
} MLTREEITEMW;

typedef int (*WADlg_handleDialogMsgs_t)(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
typedef int (*WADlg_getColor_t)(int idx);

static WADlg_handleDialogMsgs_t g_waHandleDialogMsgs = NULL;
static WADlg_getColor_t g_waGetColor = NULL;
static HFONT g_mlFont = NULL;

static WNDPROC g_oldEditProc = NULL;
static HBRUSH g_bgBrush = NULL;
static COLORREF g_bgColor = RGB(0, 0, 0);
static COLORREF g_textColor = RGB(255, 255, 255);

// --- Downloads queue (background thread) ---
struct DownloadJob {
    std::string title;
    std::string url;
    std::string status;
    std::string filePath;
};

static bool g_downloadInit = false;
static CRITICAL_SECTION g_downloadCs;
static HANDLE g_downloadEvent = NULL;
static HANDLE g_downloadThread = NULL;
static volatile LONG g_downloadStop = 0;
static std::vector<DownloadJob> g_downloadJobs;
static std::vector<int> g_downloadQueue;

static const UINT WM_INSTAPOD_DOWNLOADS_CHANGED = WM_APP + 50;
static const UINT WM_INSTAPOD_STATUS_TEXT = WM_APP + 51;
static const UINT WM_INSTAPOD_WINAMP_PLAYPATH = WM_APP + 52;
static const UINT WM_INSTAPOD_WINAMP_ADDPATH = WM_APP + 53;

static char *dupIpcPathA(const std::string &path) {
    if (path.empty()) return NULL;
    return _strdup(path.c_str());
}

static std::wstring toWideBestEffort(const std::string &s) {
    if (s.empty()) return std::wstring();

    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), -1, NULL, 0);
    if (wlen > 0) {
        std::wstring ws;
        ws.resize((size_t)wlen);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], wlen);
        if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
        return ws;
    }

    wlen = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);
    if (wlen <= 0) return std::wstring();
    std::wstring ws;
    ws.resize((size_t)wlen);
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &ws[0], wlen);
    if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
    return ws;
}

static bool winampEnqueueCopyDataW(HWND sender, const std::wstring &pathW) {
    if (!g_plugin.hwndWinampParent) return false;
    if (pathW.empty()) return false;

    COPYDATASTRUCT cds = {0};
    cds.dwData = IPC_ENQUEUEFILEW;
    cds.cbData = (DWORD)((pathW.size() + 1) * sizeof(wchar_t));
    cds.lpData = (PVOID)pathW.c_str();
    SendMessage(g_plugin.hwndWinampParent, WM_COPYDATA, (WPARAM)sender, (LPARAM)&cds);
    return true;
}

static bool winampEnqueueCopyDataA(HWND sender, const char *pathA) {
    if (!g_plugin.hwndWinampParent) return false;
    if (!pathA || !*pathA) return false;

    COPYDATASTRUCT cds = {0};
    cds.dwData = IPC_ENQUEUEFILE;
    cds.cbData = (DWORD)(strlen(pathA) + 1);
    cds.lpData = (PVOID)pathA;
    SendMessage(g_plugin.hwndWinampParent, WM_COPYDATA, (WPARAM)sender, (LPARAM)&cds);
    return true;
}

static void winampEnqueuePathSafe(HWND sender, const std::string &path) {
    std::wstring w = toWideBestEffort(path);
    if (!w.empty()) {
        winampEnqueueCopyDataW(sender, w);
        return;
    }
    winampEnqueueCopyDataA(sender, path.c_str());
}

static void initDialogSkinning() {
    if (!g_plugin.hwndLibraryParent) return;
    if (!g_waHandleDialogMsgs) {
        g_waHandleDialogMsgs = (WADlg_handleDialogMsgs_t)SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, 2, ML_IPC_SKIN_WADLG_GETFUNC);
    }
    if (!g_waGetColor) {
        g_waGetColor = (WADlg_getColor_t)SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, 1, ML_IPC_SKIN_WADLG_GETFUNC);
    }
    if (!g_mlFont) {
        g_mlFont = (HFONT)SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, 66, ML_IPC_SKIN_WADLG_GETFUNC);
    }

    // Force a pure black panel (requested). Keep text readable.
    g_bgColor = RGB(0, 0, 0);
    g_textColor = RGB(255, 255, 255);

    if (g_bgBrush) {
        DeleteObject(g_bgBrush);
        g_bgBrush = NULL;
    }
    g_bgBrush = CreateSolidBrush(g_bgColor);
}

static void applyFontRecursive(HWND hwnd, HFONT font) {
    if (!font) return;
    SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        SendMessage(child, WM_SETFONT, (WPARAM)font, TRUE);
    }
}

// Forward decls (these helpers are used before the full implementations below)
struct SearchItem;
static void setStatus(HWND hwndDlg, const char *text);
static bool DownloadAndAddToWinamp(const char *youtubeUrl, std::string &outErrText, std::string *outDownloadedPath);

static void ensureDownloadsListColumns(HWND list) {
    if (!list) return;
    HWND header = ListView_GetHeader(list);
    if (header && Header_GetItemCount(header) > 0) return;

    LVCOLUMNA col = {0};
    col.mask = LVCF_TEXT | LVCF_WIDTH;

    col.pszText = (LPSTR)"Descarga";
    col.cx = 140;
    ListView_InsertColumn(list, 0, &col);

    col.pszText = (LPSTR)"Estado";
    col.cx = 90;
    ListView_InsertColumn(list, 1, &col);
}

static void sizeDownloadsColumnsToClient(HWND list) {
    if (!list) return;
    RECT rc;
    GetClientRect(list, &rc);
    int w = rc.right - rc.left;
    if (w <= 50) return;

    int statusW = 90;
    int titleW = w - statusW - 12;
    if (titleW < 80) titleW = 80;
    ListView_SetColumnWidth(list, 0, titleW);
    ListView_SetColumnWidth(list, 1, statusW);
}

static void ensureDownloadThread() {
    if (g_downloadInit) return;
    InitializeCriticalSection(&g_downloadCs);
    g_downloadEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    g_downloadInit = true;
}

static void refreshDownloadsListUI(HWND hwndDlg) {
    (void)hwndDlg;
    if (!g_hwndDownloads) return;
    ListView_DeleteAllItems(g_hwndDownloads);

    EnterCriticalSection(&g_downloadCs);
    int count = (int)g_downloadJobs.size();
    for (int i = 0; i < count; i++) {
        const DownloadJob &j = g_downloadJobs[(size_t)i];
        LVITEMA lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = i;
        lvi.pszText = (LPSTR)j.title.c_str();
        lvi.lParam = (LPARAM)i;
        int row = ListView_InsertItem(g_hwndDownloads, &lvi);
        ListView_SetItemText(g_hwndDownloads, row, 1, (LPSTR)j.status.c_str());
    }
    LeaveCriticalSection(&g_downloadCs);

    if (count > 0) ListView_EnsureVisible(g_hwndDownloads, count - 1, FALSE);
    sizeDownloadsColumnsToClient(g_hwndDownloads);
}

static DWORD WINAPI DownloadWorkerThread(LPVOID);

static void startDownloadThreadIfNeeded() {
    ensureDownloadThread();
    if (!g_downloadThread) {
        InterlockedExchange(&g_downloadStop, 0);
        g_downloadThread = CreateThread(NULL, 0, DownloadWorkerThread, NULL, 0, NULL);
    }
}

static void enqueueDownloadJob(HWND hwndDlg, const char *title, const char *url) {
    startDownloadThreadIfNeeded();

    EnterCriticalSection(&g_downloadCs);
    DownloadJob j;
    j.title = (title && *title) ? title : (url ? url : "");
    j.url = (url ? url : "");
    j.status = "En cola";
    int idx = (int)g_downloadJobs.size();
    g_downloadJobs.push_back(j);
    g_downloadQueue.push_back(idx);
    LeaveCriticalSection(&g_downloadCs);

    refreshDownloadsListUI(hwndDlg);
    SetEvent(g_downloadEvent);
    setStatus(hwndDlg, "Agregado a la cola de descargas");
}

static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        HWND parent = GetParent(hwnd);
        if (parent) {
            HWND btn = GetDlgItem(parent, IDC_BUTTON_SEARCH);
            SendMessage(parent, WM_COMMAND, MAKEWPARAM(IDC_BUTTON_SEARCH, BN_CLICKED), (LPARAM)btn);
            return 0;
        }
    }
    return CallWindowProc(g_oldEditProc, hwnd, msg, wParam, lParam);
}

struct SearchItem {
    std::string title;
    std::string duration;
    std::string uploader;
    std::string url;
    std::string thumb;
};

static std::string g_currentQuery;
static int g_pageSize = 50;
static int g_currentPage = 0;
static bool g_hasMore = false;
static std::vector<SearchItem> g_results;

static bool isLikelyUrl(const char *text) {
    if (!text || !*text) return false;
    return (strstr(text, "http://") || strstr(text, "https://") || strstr(text, "youtu"));
}

static void setStatus(HWND hwndDlg, const char *text) {
    SetDlgItemText(hwndDlg, IDC_STATUS, text ? text : "");
}

static std::string trimLine(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) start++;
    if (start) s.erase(0, start);
    return s;
}

static void writeTextFileA(const char *path, const std::string &text) {
    if (!path || !*path) return;
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
}

static std::string pickUsefulErrorSnippet(const std::string &out) {
    if (out.empty()) return std::string();

    // Prefer lines that look like actual errors.
    const char *needles[] = {
        "ERROR:",
        "Error:",
        "Errno 13",
        "errno 13",
        "HTTP Error",
        "Forbidden",
        "Too Many Requests",
        "429",
        "403",
        "ffmpeg",
        "ffprobe",
        "Access is denied",
        "Permission denied",
        "Unable to download",
        "SSL",
        "certificate",
    };

    size_t bestPos = std::string::npos;
    for (const char *n : needles) {
        size_t p = out.rfind(n);
        if (p != std::string::npos) {
            bestPos = p;
            break;
        }
    }

    if (bestPos == std::string::npos) {
        // fallback: last 300 chars
        if (out.size() <= 300) return trimLine(out);
        return trimLine(out.substr(out.size() - 300));
    }

    size_t lineStart = out.rfind('\n', bestPos);
    if (lineStart == std::string::npos) lineStart = 0; else lineStart++;
    size_t lineEnd = out.find('\n', bestPos);
    if (lineEnd == std::string::npos) lineEnd = out.size();
    std::string line = out.substr(lineStart, lineEnd - lineStart);
    line = trimLine(line);
    if (line.size() > 220) line = line.substr(0, 220);
    return line;
}

static std::string extractDownloadedFilePath(const std::string &outText) {
    if (outText.empty()) return std::string();

    // We pass: --print after_move:filepath, so the final path should appear as a line.
    // Scan from the end for an existing file path.
    size_t pos = outText.size();
    while (pos > 0) {
        size_t lineStart = outText.rfind('\n', pos - 1);
        if (lineStart == std::string::npos) lineStart = 0; else lineStart++;
        size_t lineEnd = outText.find('\n', lineStart);
        if (lineEnd == std::string::npos) lineEnd = outText.size();

        std::string line = trimLine(outText.substr(lineStart, lineEnd - lineStart));
        if (!line.empty()) {
            // Ignore progress/status lines.
            if (line.rfind("[", 0) != 0 && line.rfind("WARNING:", 0) != 0 && line.rfind("ERROR:", 0) != 0) {
                // Remove surrounding quotes.
                if (line.size() >= 2 && ((line.front() == '"' && line.back() == '"') || (line.front() == '\'' && line.back() == '\''))) {
                    line = trimLine(line.substr(1, line.size() - 2));
                }

                // Basic plausibility: drive letter path.
                if (line.size() > 3 && line[1] == ':' && (line.find("\\") != std::string::npos || line.find("/") != std::string::npos)) {
                    DWORD attrs = GetFileAttributesA(line.c_str());
                    if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                        return line;
                    }
                }
            }
        }

        if (lineStart == 0) break;
        pos = lineStart - 1;
    }

    return std::string();
}

static std::vector<std::string> splitTab(const std::string &line) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= line.size()) {
        size_t pos = line.find('\t', start);
        if (pos == std::string::npos) {
            parts.push_back(line.substr(start));
            break;
        }
        parts.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

static bool runProcessCaptureStdout(const std::string &cmdLine, std::string &outText) {
    SECURITY_ATTRIBUTES sa = {sizeof(sa)};
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hRead = NULL;
    HANDLE hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    // In a GUI host (Winamp), STDIN can be invalid; bind it to NUL.
    HANDLE hNullIn = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hNullIn == INVALID_HANDLE_VALUE) hNullIn = NULL;
    si.hStdInput = hNullIn;

    PROCESS_INFORMATION pi = {0};
    // CreateProcessA may modify the command line buffer; provide a writable, null-terminated buffer.
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    BOOL ok = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hWrite);
    if (hNullIn) CloseHandle(hNullIn);

    if (!ok) {
        CloseHandle(hRead);
        return false;
    }

    char buffer[4096];
    DWORD bytesRead = 0;
    outText.clear();
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = 0;
        outText.append(buffer, bytesRead);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);
    return true;
}

static bool fileExistsA(const char *path) {
    if (!path || !*path) return false;
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string getModuleDirA(HINSTANCE hInst) {
    char buf[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameA(hInst, buf, (DWORD)sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) return std::string();
    for (int i = (int)strlen(buf) - 1; i >= 0; --i) {
        if (buf[i] == '\\' || buf[i] == '/') {
            buf[i] = 0;
            break;
        }
    }
    return std::string(buf);
}

static std::string joinPathA(const std::string &dir, const char *leaf) {
    if (dir.empty()) return std::string(leaf ? leaf : "");
    std::string out = dir;
    if (!out.empty() && out.back() != '\\' && out.back() != '/') out.push_back('\\');
    out += (leaf ? leaf : "");
    return out;
}

static std::string findExeNearPlugin(const char *exeName) {
    if (!exeName || !*exeName) return std::string();
    std::string pluginDir = getModuleDirA(g_plugin.hDllInstance);
    if (!pluginDir.empty()) {
        std::string p1 = joinPathA(pluginDir, exeName);
        if (fileExistsA(p1.c_str())) return p1;

        size_t pos = pluginDir.find_last_of("\\/");
        if (pos != std::string::npos) {
            std::string parentDir = pluginDir.substr(0, pos);
            std::string p2 = joinPathA(parentDir, exeName);
            if (fileExistsA(p2.c_str())) return p2;
        }
    }
    return std::string();
}

static std::string findDirOfExeNearPlugin(const char *exeName) {
    std::string full = findExeNearPlugin(exeName);
    if (full.empty()) return std::string();
    size_t pos = full.find_last_of("\\/");
    if (pos == std::string::npos) return std::string();
    return full.substr(0, pos);
}

static bool ensureDirExistsA(const char *dirPath) {
    if (!dirPath || !*dirPath) return false;
    DWORD attrs = GetFileAttributesA(dirPath);
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    // CreateDirectory only creates one level; callers should pass a simple leaf folder.
    return CreateDirectoryA(dirPath, NULL) ? true : false;
}

static bool isDirWritableA(const char *dirPath) {
    if (!dirPath || !*dirPath) return false;
    char testPath[MAX_PATH] = {0};
    snprintf(testPath, sizeof(testPath) - 1, "%s\\music4all_write_test.tmp", dirPath);

    HANDLE h = CreateFileA(testPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}

static bool getDownloadDirA(char outDir[MAX_PATH]) {
    if (!outDir) return false;
    outDir[0] = 0;

    // 1) Try Winamp playlist dir
    char winampDir[MAX_PATH] = {0};
    SendMessage(g_plugin.hwndWinampParent, WM_WA_IPC, (WPARAM)winampDir, IPC_GETPLAYLISTDIR);
    if (winampDir[0]) {
        if (isDirWritableA(winampDir)) {
            strncpy(outDir, winampDir, MAX_PATH - 1);
            outDir[MAX_PATH - 1] = 0;
            return true;
        }
    }

    // 2) Fallback: %USERPROFILE%\Music\Music4All (usually writable)
    char musicDir[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYMUSIC | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, musicDir)) && musicDir[0]) {
        // Create leaf folder Music4All
        char instapodDir[MAX_PATH] = {0};
        snprintf(instapodDir, sizeof(instapodDir) - 1, "%s\\Music4All", musicDir);
        ensureDirExistsA(instapodDir);
        if (isDirWritableA(instapodDir)) {
            strncpy(outDir, instapodDir, MAX_PATH - 1);
            outDir[MAX_PATH - 1] = 0;
            return true;
        }
    }

    // 3) Last resort: current directory
    char cwd[MAX_PATH] = {0};
    if (GetCurrentDirectoryA(MAX_PATH, cwd) && cwd[0] && isDirWritableA(cwd)) {
        strncpy(outDir, cwd, MAX_PATH - 1);
        outDir[MAX_PATH - 1] = 0;
        return true;
    }

    return false;
}

static bool runProcessWaitExitCode(const std::string &cmdLine, DWORD &exitCode) {
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};

    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    BOOL ok = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!ok) return false;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    exitCode = code;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

static bool runProcessCaptureStdoutExitCode(const std::string &cmdLine, std::string &outText, DWORD &exitCode) {
    SECURITY_ATTRIBUTES sa = {sizeof(sa)};
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hRead = NULL;
    HANDLE hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    HANDLE hNullIn = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hNullIn == INVALID_HANDLE_VALUE) hNullIn = NULL;
    si.hStdInput = hNullIn;

    PROCESS_INFORMATION pi = {0};
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    BOOL ok = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hWrite);
    if (hNullIn) CloseHandle(hNullIn);

    if (!ok) {
        CloseHandle(hRead);
        return false;
    }

    outText.clear();
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = 0;
        outText.append(buffer, bytesRead);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    exitCode = code;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);
    return true;
}

static bool runYtDlpCapture(const std::string &args, std::string &outText) {
    // Winamp may not inherit the user's PATH. Prefer yt-dlp.exe near the plugin.
    std::vector<std::string> prefixes;

    {
        std::string exe = findExeNearPlugin("yt-dlp.exe");
        if (!exe.empty()) prefixes.push_back("\"" + exe + "\" ");
    }

    // Common fallbacks
    if (fileExistsA("C:\\Windows\\py.exe")) {
        prefixes.push_back("\"C:\\Windows\\py.exe\" -3 -m yt_dlp ");
    }
    prefixes.push_back("yt-dlp ");
    prefixes.push_back("py -3 -m yt_dlp ");
    prefixes.push_back("python -m yt_dlp ");

    for (const std::string &p : prefixes) {
        if (runProcessCaptureStdout(p + args, outText)) return true;
    }
    return false;
}

static bool runYtDlpWait(const std::string &args) {
    std::vector<std::string> prefixes;

    {
        std::string exe = findExeNearPlugin("yt-dlp.exe");
        if (!exe.empty()) prefixes.push_back("\"" + exe + "\" ");
    }

    if (fileExistsA("C:\\Windows\\py.exe")) {
        prefixes.push_back("\"C:\\Windows\\py.exe\" -3 -m yt_dlp ");
    }
    prefixes.push_back("yt-dlp ");
    prefixes.push_back("py -3 -m yt_dlp ");
    prefixes.push_back("python -m yt_dlp ");

    for (const std::string &p : prefixes) {
        DWORD code = 0;
        if (runProcessWaitExitCode(p + args, code)) return code == 0;
    }
    return false;
}

static bool runYtDlpWaitCapture(const std::string &args, std::string &outText, DWORD &exitCode) {
    std::vector<std::string> prefixes;

    {
        std::string exe = findExeNearPlugin("yt-dlp.exe");
        if (!exe.empty()) prefixes.push_back("\"" + exe + "\" ");
    }

    if (fileExistsA("C:\\Windows\\py.exe")) {
        prefixes.push_back("\"C:\\Windows\\py.exe\" -3 -m yt_dlp ");
    }
    prefixes.push_back("yt-dlp ");
    prefixes.push_back("py -3 -m yt_dlp ");
    prefixes.push_back("python -m yt_dlp ");

    for (const std::string &p : prefixes) {
        DWORD code = 0;
        std::string out;
        if (runProcessCaptureStdoutExitCode(p + args, out, code)) {
            outText = out;
            exitCode = code;
            return code == 0;
        }
    }

    outText.clear();
    exitCode = 0;
    return false;
}

static void ensureListColumns(HWND list) {
    // Set up columns once
    if (ListView_GetColumnWidth(list, 0) > 0) return;

    LVCOLUMNA col = {0};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.pszText = (LPSTR)"T\xEDtulo";
    col.cx = 240;
    col.iSubItem = 0;
    ListView_InsertColumn(list, 0, &col);

    col.pszText = (LPSTR)"Dur.";
    col.cx = 60;
    col.iSubItem = 1;
    ListView_InsertColumn(list, 1, &col);

    col.pszText = (LPSTR)"Canal";
    col.cx = 170;
    col.iSubItem = 2;
    ListView_InsertColumn(list, 2, &col);
}

static void sizeListColumnsToClient(HWND list) {
    if (!list) return;
    RECT rc;
    GetClientRect(list, &rc);
    int w = rc.right - rc.left;
    if (w <= 50) return;

    int durW = 60;
    int canalW = 160;
    int titleW = w - (durW + canalW) - 12;
    if (titleW < 140) titleW = 140;

    ListView_SetColumnWidth(list, 1, durW);
    ListView_SetColumnWidth(list, 2, canalW);
    ListView_SetColumnWidth(list, 0, titleW);
}

static void drawThemedButton(DRAWITEMSTRUCT *dis) {
    if (!dis) return;
    RECT rc = dis->rcItem;

    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool focused = (dis->itemState & ODS_FOCUS) != 0;

    COLORREF bg = disabled ? RGB(18, 18, 18) : (pressed ? RGB(55, 55, 55) : RGB(32, 32, 32));
    COLORREF border = disabled ? RGB(40, 40, 40) : RGB(85, 85, 85);
    COLORREF text = disabled ? RGB(140, 140, 140) : RGB(255, 255, 255);

    HBRUSH brBg = CreateSolidBrush(bg);
    FillRect(dis->hDC, &rc, brBg);
    DeleteObject(brBg);

    HBRUSH brBorder = CreateSolidBrush(border);
    FrameRect(dis->hDC, &rc, brBorder);
    DeleteObject(brBorder);

    char caption[256];
    caption[0] = 0;
    GetWindowTextA(dis->hwndItem, caption, (int)sizeof(caption));

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, text);

    RECT rcText = rc;
    // Slight press offset.
    if (pressed) {
        OffsetRect(&rcText, 1, 1);
    }
    DrawTextA(dis->hDC, caption, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (focused && !disabled) {
        RECT rcFocus = rc;
        InflateRect(&rcFocus, -3, -3);
        DrawFocusRect(dis->hDC, &rcFocus);
    }
}

static void layoutEmbeddedUi(HWND hwndDlg) {
    RECT rc;
    GetClientRect(hwndDlg, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    const int m = 12;
    const int gap = 8;
    const int rowH = 20;
    const int statusH = 12;
    const int searchBtnW = 98;

    int y = m;
    HWND hEdit = GetDlgItem(hwndDlg, IDC_EDIT_URL);
    HWND hSearch = GetDlgItem(hwndDlg, IDC_BUTTON_SEARCH);
    HWND hPrev = GetDlgItem(hwndDlg, IDC_BUTTON_PREV);
    HWND hNext = GetDlgItem(hwndDlg, IDC_BUTTON_NEXT);
    HWND hPage = GetDlgItem(hwndDlg, IDC_LABEL_PAGE);
    HWND hDownload = GetDlgItem(hwndDlg, IDC_BUTTON_DOWNLOAD);
    HWND hOpen = GetDlgItem(hwndDlg, IDC_BUTTON_LAUNCH_INSTAPOD);
    HWND hStatus = GetDlgItem(hwndDlg, IDC_STATUS);
    HWND hBrand = GetDlgItem(hwndDlg, IDC_LABEL_BRAND);
    HWND hList = GetDlgItem(hwndDlg, IDC_LIST_RESULTS);
    HWND hDownloadsLabel = GetDlgItem(hwndDlg, IDC_LABEL_DOWNLOADS);
    HWND hDownloads = GetDlgItem(hwndDlg, IDC_LIST_DOWNLOADS);

    // Search row
    int editY = m;
    int editW = w - (m * 2) - gap - searchBtnW;
    if (editW < 120) editW = 120;
    MoveWindow(hEdit, m, editY, editW, rowH, TRUE);
    MoveWindow(hSearch, m + editW + gap, editY, searchBtnW, rowH, TRUE);

    // List occupies most of the center
    int listY = editY + rowH + gap;
    int bottomRowY = h - m - statusH - gap - rowH;
    int listH = bottomRowY - listY - gap;
    if (listH < 60) listH = 60;

    // Right-side downloads panel
    int downloadsW = 210;
    int minDownloadsW = 170;
    if (w < 520) downloadsW = 180;
    if (downloadsW < minDownloadsW) downloadsW = minDownloadsW;
    if (downloadsW > (w / 2)) downloadsW = w / 2;

    int resultsW = (w - (m * 2)) - gap - downloadsW;
    if (resultsW < 180) {
        resultsW = 180;
        downloadsW = (w - (m * 2)) - gap - resultsW;
        if (downloadsW < minDownloadsW) downloadsW = minDownloadsW;
    }

    int downloadsX = m + resultsW + gap;
    MoveWindow(hList, m, listY, resultsW, listH, TRUE);
    if (hDownloadsLabel) MoveWindow(hDownloadsLabel, downloadsX, listY, downloadsW, 12, TRUE);
    if (hDownloads) MoveWindow(hDownloads, downloadsX, listY + 14, downloadsW, listH - 14, TRUE);

    // Bottom row: paging stays under results list; actions stay under downloads panel.
    int prevW = 70;
    int nextW = 70;

    int prevX = m;
    int nextX = m + resultsW - nextW;
    if (nextX < prevX + prevW + gap) nextX = prevX + prevW + gap;
    MoveWindow(hPrev, prevX, bottomRowY, prevW, rowH, TRUE);
    MoveWindow(hNext, nextX, bottomRowY, nextW, rowH, TRUE);

    int pageX = prevX + prevW + gap;
    int pageW = nextX - pageX - gap;
    if (pageW < 60) pageW = 60;
    MoveWindow(hPage, pageX, bottomRowY + 3, pageW, rowH, TRUE);

    int actionGap = gap;
    int actionW2 = (downloadsW - actionGap) / 2;
    int actionW1 = downloadsW - actionGap - actionW2;
    if (actionW1 < 80) actionW1 = 80;
    if (actionW2 < 80) actionW2 = 80;
    int openX = downloadsX;
    int downloadX = downloadsX + actionW1 + actionGap;
    MoveWindow(hOpen, openX, bottomRowY, actionW1, rowH + 2, TRUE);
    MoveWindow(hDownload, downloadX, bottomRowY, actionW2, rowH + 2, TRUE);

    // Status line
    int brandW = 220;
    int brandX = w - m - brandW;
    if (brandX < m + 120) {
        brandW = w - (m * 2);
        brandX = m;
    }
    if (hBrand) MoveWindow(hBrand, brandX, h - m - statusH, brandW, statusH, TRUE);

    int statusW = (hBrand ? (brandX - m - gap) : (w - (m * 2)));
    if (statusW < 80) statusW = 80;
    MoveWindow(hStatus, m, h - m - statusH, statusW, statusH, TRUE);

    // Keep columns fitting the list.
    sizeListColumnsToClient(hList);
    sizeDownloadsColumnsToClient(hDownloads);
}

static void clearResultsList(HWND list) {
    ListView_DeleteAllItems(list);
}

static void renderPage(HWND hwndDlg) {
    if (!g_hwndList) return;
    clearResultsList(g_hwndList);

    int startIndex = g_currentPage * g_pageSize;
    int endIndex = startIndex + g_pageSize;
    if (startIndex < 0) startIndex = 0;
    if (endIndex > (int)g_results.size()) endIndex = (int)g_results.size();

    for (int i = startIndex; i < endIndex; i++) {
        const SearchItem &it = g_results[(size_t)i];
        LVITEMA lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = i - startIndex;
        lvi.pszText = (LPSTR)it.title.c_str();
        lvi.lParam = (LPARAM)i; // store global index
        int row = ListView_InsertItem(g_hwndList, &lvi);
        ListView_SetItemText(g_hwndList, row, 1, (LPSTR)it.duration.c_str());
        ListView_SetItemText(g_hwndList, row, 2, (LPSTR)it.uploader.c_str());
    }

    // Default selection on first row for convenience (so Download works immediately).
    if (ListView_GetItemCount(g_hwndList) > 0) {
        ListView_SetItemState(g_hwndList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(g_hwndList, 0, FALSE);
    }

    char pageText[96];
    int shown = endIndex - startIndex;
    sprintf(pageText, "P\xE1gina %d \xB7 %d", g_currentPage + 1, shown);
    SetDlgItemText(hwndDlg, IDC_LABEL_PAGE, pageText);

    EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_PREV), g_currentPage > 0);
    EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_NEXT), g_hasMore);
    EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_DOWNLOAD), ListView_GetItemCount(g_hwndList) > 0);
}

static void ensureFetchedForPage(HWND hwndDlg, const std::string &query, int page) {
    // Fetch enough rows for this page; uses ytsearchN to progressively fetch.
    int needed = (page + 1) * g_pageSize;
    if ((int)g_results.size() >= needed || query.empty()) {
        g_hasMore = ((int)g_results.size() > needed);
        return;
    }

    setStatus(hwndDlg, "Buscando...");
    g_results.clear();
    g_hasMore = false;

    // Build yt-dlp command that prints tab-separated rows:
    // title\tduration\tuploader\turl\tthumbnail
    // Use --flat-playlist so it's fast; build url from video id.
    char quotedQuery[1024];
    // naive quoting: replace quotes with spaces
    strncpy(quotedQuery, query.c_str(), sizeof(quotedQuery) - 1);
    quotedQuery[sizeof(quotedQuery) - 1] = 0;
    for (char *p = quotedQuery; *p; p++) {
        if (*p == '"') *p = ' ';
    }

    char args[2048];
    sprintf(args,
            "--flat-playlist --no-warnings --print \"%%(title)s\t%%(duration_string)s\t%%(uploader)s\thttps://www.youtube.com/watch?v=%%(id)s\t%%(thumbnail)s\" \"ytsearch%d:%s\"",
            needed + 1, quotedQuery);

    std::string output;
    if (!runYtDlpCapture(args, output)) {
        setStatus(hwndDlg, "No se pudo ejecutar yt-dlp. Instala yt-dlp o revisa Python/py launcher.");
        return;
    }

    // Parse output lines
    size_t pos = 0;
    while (pos < output.size()) {
        size_t nl = output.find('\n', pos);
        if (nl == std::string::npos) nl = output.size();
        std::string line = trimLine(output.substr(pos, nl - pos));
        pos = nl + 1;
        if (line.empty()) continue;

        auto parts = splitTab(line);
        if (parts.size() < 4) continue;
        SearchItem item;
        item.title = parts[0];
        item.duration = parts.size() > 1 ? parts[1] : "";
        item.uploader = parts.size() > 2 ? parts[2] : "";
        item.url = parts.size() > 3 ? parts[3] : "";
        item.thumb = parts.size() > 4 ? parts[4] : "";
        g_results.push_back(item);
    }

    // If we got more than needed, keep it as indicator for next page.
    if ((int)g_results.size() > needed) {
        g_hasMore = true;
    } else {
        g_hasMore = false;
    }

    if (g_results.empty()) {
        setStatus(hwndDlg, "Sin resultados");
    } else {
        int shown = (page + 1) * g_pageSize;
        if ((int)g_results.size() < shown) shown = (int)g_results.size();
        char st[96];
        sprintf(st, "Listo (%d)", shown);
        setStatus(hwndDlg, st);
    }
}

static bool getSelectedUrl(HWND hwndDlg, std::string &outUrl) {
    outUrl.clear();
    if (!g_hwndList) return false;
    int sel = ListView_GetNextItem(g_hwndList, -1, LVNI_SELECTED);
    if (sel < 0) {
        setStatus(hwndDlg, "Selecciona un resultado primero");
        return false;
    }
    LVITEMA lvi = {0};
    lvi.mask = LVIF_PARAM;
    lvi.iItem = sel;
    if (!ListView_GetItem(g_hwndList, &lvi)) return false;
    int idx = (int)lvi.lParam;
    if (idx < 0 || idx >= (int)g_results.size()) return false;
    outUrl = g_results[(size_t)idx].url;
    return !outUrl.empty();
}

static bool getSelectedResult(HWND hwndDlg, SearchItem &outItem) {
    (void)hwndDlg;
    if (!g_hwndList) return false;
    int sel = ListView_GetNextItem(g_hwndList, -1, LVNI_SELECTED);
    if (sel < 0) {
        setStatus(hwndDlg, "Selecciona un resultado primero");
        return false;
    }
    LVITEMA lvi = {0};
    lvi.mask = LVIF_PARAM;
    lvi.iItem = sel;
    if (!ListView_GetItem(g_hwndList, &lvi)) return false;
    int idx = (int)lvi.lParam;
    if (idx < 0 || idx >= (int)g_results.size()) return false;
    outItem = g_results[(size_t)idx];
    return !outItem.url.empty();
}

static DWORD WINAPI DownloadWorkerThread(LPVOID) {
    for (;;) {
        WaitForSingleObject(g_downloadEvent, INFINITE);
        if (InterlockedCompareExchange(&g_downloadStop, 0, 0)) break;

        for (;;) {
            int jobIndex = -1;
            EnterCriticalSection(&g_downloadCs);
            if (!g_downloadQueue.empty()) {
                jobIndex = g_downloadQueue.front();
                g_downloadQueue.erase(g_downloadQueue.begin());
                if (jobIndex >= 0 && jobIndex < (int)g_downloadJobs.size()) {
                    g_downloadJobs[(size_t)jobIndex].status = "Descargando...";
                }
            }
            LeaveCriticalSection(&g_downloadCs);

            if (jobIndex < 0) break;

            if (g_hwndDialog) PostMessage(g_hwndDialog, WM_INSTAPOD_DOWNLOADS_CHANGED, 0, 0);
            if (g_hwndDialog) PostMessage(g_hwndDialog, WM_INSTAPOD_STATUS_TEXT, 0, (LPARAM)_strdup("Descargando..."));

            std::string url;
            EnterCriticalSection(&g_downloadCs);
            if (jobIndex >= 0 && jobIndex < (int)g_downloadJobs.size()) url = g_downloadJobs[(size_t)jobIndex].url;
            LeaveCriticalSection(&g_downloadCs);

            std::string err;
            std::string downloadedPath;
            bool ok = false;
            if (!url.empty()) ok = DownloadAndAddToWinamp(url.c_str(), err, &downloadedPath);
            else {
                err = "URL inv\xE1lida";
                ok = false;
            }

            EnterCriticalSection(&g_downloadCs);
            if (jobIndex >= 0 && jobIndex < (int)g_downloadJobs.size()) {
                if (ok) g_downloadJobs[(size_t)jobIndex].status = "Completado";
                else {
                    if (err.empty()) err = "Error";
                    if (err.size() > 60) err = err.substr(0, 60);
                    g_downloadJobs[(size_t)jobIndex].status = err;
                }
                if (ok) g_downloadJobs[(size_t)jobIndex].filePath = downloadedPath;
            }
            LeaveCriticalSection(&g_downloadCs);

            if (g_hwndDialog) PostMessage(g_hwndDialog, WM_INSTAPOD_DOWNLOADS_CHANGED, 0, 0);
            if (g_hwndDialog) {
                if (ok) PostMessage(g_hwndDialog, WM_INSTAPOD_STATUS_TEXT, 0, (LPARAM)_strdup("Completado. Re-escaneado."));
                else PostMessage(g_hwndDialog, WM_INSTAPOD_STATUS_TEXT, 0, (LPARAM)_strdup("Error al descargar. Ver Descargas."));
            }

            if (InterlockedCompareExchange(&g_downloadStop, 0, 0)) break;
        }

        if (InterlockedCompareExchange(&g_downloadStop, 0, 0)) break;
    }
    return 0;
}

static void LaunchMusic4All() {
    char pluginDir[MAX_PATH];
    char cmdLine[2 * MAX_PATH];
    char scriptPath[MAX_PATH];

    GetModuleFileName(g_plugin.hDllInstance, pluginDir, MAX_PATH);
    char *lastSlash = strrchr(pluginDir, '\\');
    if (lastSlash) *lastSlash = 0;

    // Expect music4all.py (public launcher) next to the DLL (Winamp Plugins folder).
    // Fallback to instapod.py for backwards compatibility.
    sprintf(scriptPath, "%s\\music4all.py", pluginDir);
    if (GetFileAttributesA(scriptPath) == INVALID_FILE_ATTRIBUTES) {
        sprintf(scriptPath, "%s\\instapod.py", pluginDir);
    }
    sprintf(cmdLine, "python \"%s\"", scriptPath);

    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    if (CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static bool DownloadAndAddToWinamp(const char *youtubeUrl, std::string &outErrText, std::string *outDownloadedPath) {
    outErrText.clear();
    if (outDownloadedPath) outDownloadedPath->clear();
    char outputPath[MAX_PATH] = {0};

    if (!getDownloadDirA(outputPath) || !outputPath[0]) {
        outErrText = "No hay carpeta de descarga con permisos. Usa Music\\Music4All.";
        return false;
    }

    std::string ffmpegDir = findDirOfExeNearPlugin("ffmpeg.exe");

    char args[2800];
    // Force Windows-safe filenames and avoid extremely long paths.
    // %(title).180B trims title to 180 bytes.
    const char *outTpl = "-o \"%s\\%%(title).180B.%%(ext)s\"";

    if (!ffmpegDir.empty()) {
        sprintf(args,
                "--ffmpeg-location \"%s\" --windows-filenames --trim-filenames 180 --no-warnings --retries 5 --fragment-retries 5 --force-overwrites --no-progress --add-metadata --embed-thumbnail --convert-thumbnails jpg --print after_move:filepath -x --audio-format mp3 ",
                ffmpegDir.c_str());
        size_t len = strlen(args);
        snprintf(args + len, sizeof(args) - len, outTpl, outputPath);
        len = strlen(args);
        snprintf(args + len, sizeof(args) - len, " \"%s\"", youtubeUrl);
    } else {
        sprintf(args,
                "--windows-filenames --trim-filenames 180 --no-warnings --retries 5 --fragment-retries 5 --force-overwrites --no-progress --add-metadata --embed-thumbnail --convert-thumbnails jpg --print after_move:filepath -x --audio-format mp3 ");
        size_t len = strlen(args);
        snprintf(args + len, sizeof(args) - len, outTpl, outputPath);
        len = strlen(args);
        snprintf(args + len, sizeof(args) - len, " \"%s\"", youtubeUrl);
    }

    DWORD code = 0;
    std::string out;
    if (!runYtDlpWaitCapture(args, out, code)) {
        // Persist full output for debugging.
        char logPath[MAX_PATH] = {0};
        snprintf(logPath, sizeof(logPath) - 1, "%s\\music4all_yt-dlp_last_error.txt", outputPath);
        if (!out.empty()) writeTextFileA(logPath, out);

        outErrText = pickUsefulErrorSnippet(out);

        // If we got access denied, the folder isn't writable.
        if (outErrText.find("Access is denied") != std::string::npos || outErrText.find("Permission denied") != std::string::npos) {
            outErrText = "Permiso denegado escribiendo. Intenta en Music\\Music4All.";
        }

        // Friendly hints for common issues.
        if (outErrText.find("Errno 13") != std::string::npos || outErrText.find("errno 13") != std::string::npos) {
            outErrText = "Permiso denegado (Errno 13). Cierra el archivo si est\xE1 abierto y reintenta.";
        }
        if (outErrText.find("HTTP Error 403") != std::string::npos || outErrText.find("Forbidden") != std::string::npos) {
            outErrText = "YouTube bloque\xF3 la descarga (403). Actualiza yt-dlp y reintenta.";
        } else if (outErrText.find("Too Many Requests") != std::string::npos || outErrText.find("HTTP Error 429") != std::string::npos) {
            outErrText = "Demasiadas solicitudes (429). Espera y reintenta.";
        }

        if (ffmpegDir.empty()) {
            // Most common for MP3 extraction.
            if (outErrText.find("ffmpeg") != std::string::npos || outErrText.find("ffprobe") != std::string::npos || code != 0) {
                outErrText = "Falta ffmpeg.exe/ffprobe.exe (para MP3). Copialos a Winamp\\Plugins.";
            }
        }

        // If we still have something generic, point to the log.
        if (outErrText.empty()) {
            outErrText = "yt-dlp fall\xF3. Ver music4all_yt-dlp_last_error.txt";
        }
        return false;
    }

    if (outDownloadedPath) {
        *outDownloadedPath = extractDownloadedFilePath(out);
    }

    // Re-scan/refresh cache so the new file appears (do not force playback).
    SendMessage(g_plugin.hwndWinampParent, WM_WA_IPC, 0, IPC_REFRESHPLCACHE);
    return true;
}

static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DRAWITEM) {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
        if (dis && dis->CtlType == ODT_BUTTON) {
            drawThemedButton(dis);
            return TRUE;
        }
    }

    // Color handling first (we want a clean black panel).
    if (uMsg == WM_CTLCOLORDLG || uMsg == WM_CTLCOLORSTATIC || uMsg == WM_CTLCOLORBTN || uMsg == WM_CTLCOLOREDIT) {
        if (g_bgBrush) {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, g_textColor);
            SetBkColor(hdc, g_bgColor);
            return (INT_PTR)g_bgBrush;
        }
    }

    // Let Winamp/ML skinning handle the rest where possible.
    if (g_waHandleDialogMsgs && uMsg != WM_INITDIALOG) {
        if (g_waHandleDialogMsgs(hwndDlg, uMsg, wParam, lParam)) {
            return TRUE;
        }
    }

    switch (uMsg) {
        case WM_INITDIALOG:
            {
                initDialogSkinning();
                INITCOMMONCONTROLSEX icc = {sizeof(icc)};
                icc.dwICC = ICC_LISTVIEW_CLASSES;
                InitCommonControlsEx(&icc);

                g_hwndList = GetDlgItem(hwndDlg, IDC_LIST_RESULTS);
                if (g_hwndList) {
                    ListView_SetExtendedListViewStyle(g_hwndList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
                    ensureListColumns(g_hwndList);
                    sizeListColumnsToClient(g_hwndList);

                    // Ask Media Library to skin the listview like Winamp.
                    // This makes it look consistent with Big Bento.
                    g_listSkinHandle = SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM)g_hwndList, ML_IPC_SKIN_LISTVIEW);
                }

                g_hwndDownloads = GetDlgItem(hwndDlg, IDC_LIST_DOWNLOADS);
                if (g_hwndDownloads) {
                    ListView_SetExtendedListViewStyle(g_hwndDownloads, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
                    ensureDownloadsListColumns(g_hwndDownloads);
                    sizeDownloadsColumnsToClient(g_hwndDownloads);
                    g_downloadsSkinHandle = SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM)g_hwndDownloads, ML_IPC_SKIN_LISTVIEW);
                }

                ensureDownloadThread();
                refreshDownloadsListUI(hwndDlg);

                EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_DOWNLOAD), FALSE);
                EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_PREV), FALSE);
                EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_NEXT), FALSE);
                setStatus(hwndDlg, "Listo");
                layoutEmbeddedUi(hwndDlg);

                // Pressing Enter in the search box should always trigger a new search.
                HWND hEdit = GetDlgItem(hwndDlg, IDC_EDIT_URL);
                if (hEdit && !g_oldEditProc) {
                    g_oldEditProc = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);
                }

                applyFontRecursive(hwndDlg, g_mlFont);

                // Allow ML to do its own dialog message handling too.
                if (g_waHandleDialogMsgs) {
                    g_waHandleDialogMsgs(hwndDlg, uMsg, wParam, lParam);
                }
            }
            return TRUE;

        case WM_NCDESTROY:
            // Media Library may destroy/recreate the view when switching tabs.
            // Ensure we don't keep stale HWNDs/skin handles.
            if (g_listSkinHandle) {
                SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM)g_listSkinHandle, ML_IPC_UNSKIN_LISTVIEW);
                g_listSkinHandle = 0;
            }
            if (g_downloadsSkinHandle) {
                SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM)g_downloadsSkinHandle, ML_IPC_UNSKIN_LISTVIEW);
                g_downloadsSkinHandle = 0;
            }
            g_hwndDialog = NULL;
            g_hwndList = NULL;
            g_hwndDownloads = NULL;
            g_oldEditProc = NULL;
            return FALSE;

        case WM_SIZE:
            layoutEmbeddedUi(hwndDlg);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BUTTON_SEARCH: {
                    char q[512] = {0};
                    GetDlgItemText(hwndDlg, IDC_EDIT_URL, q, sizeof(q));
                    if (strlen(q) == 0) {
                        setStatus(hwndDlg, "Escribe un t\xE9rmino o pega un link");
                        return TRUE;
                    }

                    // New search: always reset previous results.
                    g_results.clear();
                    g_hasMore = false;
                    g_currentPage = 0;

                    if (isLikelyUrl(q)) {
                        // Treat as direct URL (single result)
                        SearchItem item;
                        item.title = q;
                        item.duration = "";
                        item.uploader = "";
                        item.url = q;
                        item.thumb = "";
                        g_results.push_back(item);
                        g_currentQuery = q;
                        renderPage(hwndDlg);
                        setStatus(hwndDlg, "Link listo para descargar");
                        return TRUE;
                    }

                    g_currentQuery = q;
                    ensureFetchedForPage(hwndDlg, g_currentQuery, g_currentPage);
                    renderPage(hwndDlg);
                    return TRUE;
                }

                case IDC_BUTTON_DOWNLOAD: {
                    SearchItem it;
                    if (!getSelectedResult(hwndDlg, it)) return TRUE;
                    enqueueDownloadJob(hwndDlg, it.title.c_str(), it.url.c_str());
                    return TRUE;
                }

                case IDC_BUTTON_PREV:
                    if (g_currentPage > 0) {
                        g_currentPage--;
                        // we already fetched progressively from scratch each search
                        g_hasMore = (int)g_results.size() > (g_currentPage + 1) * g_pageSize;
                        renderPage(hwndDlg);
                    }
                    return TRUE;

                case IDC_BUTTON_NEXT:
                    if (!g_currentQuery.empty() && g_hasMore) {
                        g_currentPage++;
                        ensureFetchedForPage(hwndDlg, g_currentQuery, g_currentPage);
                        renderPage(hwndDlg);
                    }
                    return TRUE;

                case IDC_BUTTON_LAUNCH_INSTAPOD:
                    LaunchMusic4All();
                    return TRUE;
            }
            break;

        case WM_INSTAPOD_DOWNLOADS_CHANGED:
            refreshDownloadsListUI(hwndDlg);
            return TRUE;

        case WM_INSTAPOD_STATUS_TEXT:
            if (lParam) {
                char *p = (char *)lParam;
                setStatus(hwndDlg, p);
                free(p);
            }
            return TRUE;

        case WM_INSTAPOD_WINAMP_PLAYPATH:
            if (lParam) {
                char *p = (char *)lParam;
                std::string path(p);
                free(p);

                // Deterministic "play this file": clear playlist, enqueue, then play.
                SendMessage(g_plugin.hwndWinampParent, WM_WA_IPC, 0, IPC_DELETE);
                winampEnqueuePathSafe(hwndDlg, path);
                SendMessage(g_plugin.hwndWinampParent, WM_WA_IPC, 0, IPC_STARTPLAY);

                setStatus(hwndDlg, "Reproduciendo...");
            }
            return TRUE;

        case WM_INSTAPOD_WINAMP_ADDPATH:
            if (lParam) {
                char *p = (char *)lParam;
                winampEnqueuePathSafe(hwndDlg, std::string(p));
                free(p);
                setStatus(hwndDlg, "A\xF1" "adido a playlist");
            }
            return TRUE;

        case WM_NOTIFY:
            if (wParam == IDC_LIST_RESULTS) {
                LPNMHDR hdr = (LPNMHDR)lParam;
                if (hdr && hdr->code == NM_DBLCLK) {
                    // Double-click downloads selected
                    SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDC_BUTTON_DOWNLOAD, BN_CLICKED), (LPARAM)GetDlgItem(hwndDlg, IDC_BUTTON_DOWNLOAD));
                    return TRUE;
                }
            }

            if (wParam == IDC_LIST_DOWNLOADS) {
                LPNMHDR hdr = (LPNMHDR)lParam;
                if (hdr && hdr->code == NM_DBLCLK) {
                    int sel = ListView_GetNextItem(g_hwndDownloads, -1, LVNI_SELECTED);
                    if (sel >= 0) {
                        LVITEMA lvi = {0};
                        lvi.mask = LVIF_PARAM;
                        lvi.iItem = sel;
                        if (ListView_GetItem(g_hwndDownloads, &lvi)) {
                            int jobIndex = (int)lvi.lParam;
                            std::string path;
                            EnterCriticalSection(&g_downloadCs);
                            if (jobIndex >= 0 && jobIndex < (int)g_downloadJobs.size()) path = g_downloadJobs[(size_t)jobIndex].filePath;
                            LeaveCriticalSection(&g_downloadCs);

                            if (!path.empty() && fileExistsA(path.c_str())) {
                                // Defer IPC out of the notify handler.
                                char *p = dupIpcPathA(path);
                                if (p) PostMessage(hwndDlg, WM_INSTAPOD_WINAMP_PLAYPATH, 0, (LPARAM)p);
                            } else {
                                setStatus(hwndDlg, "A\xFAn no est\xE1 lista la descarga");
                            }
                        }
                    }
                    return TRUE;
                }

                if (hdr && hdr->code == NM_RCLICK) {
                    int sel = ListView_GetNextItem(g_hwndDownloads, -1, LVNI_SELECTED);
                    if (sel < 0) return TRUE;

                    LVITEMA lvi = {0};
                    lvi.mask = LVIF_PARAM;
                    lvi.iItem = sel;
                    if (!ListView_GetItem(g_hwndDownloads, &lvi)) return TRUE;

                    int jobIndex = (int)lvi.lParam;
                    std::string path;
                    EnterCriticalSection(&g_downloadCs);
                    if (jobIndex >= 0 && jobIndex < (int)g_downloadJobs.size()) path = g_downloadJobs[(size_t)jobIndex].filePath;
                    LeaveCriticalSection(&g_downloadCs);

                    POINT pt;
                    GetCursorPos(&pt);
                    HMENU menu = CreatePopupMenu();
                    const UINT CMD_ADD = 1;
                    AppendMenuA(menu, MF_STRING, CMD_ADD, "A\xF1" "adir a playlist");

                    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwndDlg, NULL);
                    DestroyMenu(menu);

                    if (cmd == CMD_ADD) {
                        if (!path.empty() && fileExistsA(path.c_str())) {
                            // Add to playlist (do not force playback).
                            char *p = dupIpcPathA(path);
                            if (p) PostMessage(hwndDlg, WM_INSTAPOD_WINAMP_ADDPATH, 0, (LPARAM)p);
                        } else {
                            setStatus(hwndDlg, "A\xFAn no est\xE1 lista la descarga");
                        }
                    }
                    return TRUE;
                }
            }
            break;

        case WM_CLOSE:
            // Media Library owns view lifetime; just hide
            ShowWindow(hwndDlg, SW_HIDE);
            return TRUE;

    }
    return FALSE;
}

static int __cdecl initMl() {
    // Add a left-panel item in Media Library (try newer tree API first to support custom icon).
    g_treeApiNew = false;
    g_treeId = 0;

    int imgIdx = -1;
    MLTREEIMAGE img = {0};
    img.hinst = g_plugin.hDllInstance;
    img.resourceId = IDB_MUSIC4ALL_TREE;
    img.imageIndex = -1;
    img.filterProc = NULL;
    img.width = 0;
    img.height = 0;
    imgIdx = (int)(INT_PTR)SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM)&img, ML_IPC_TREEIMAGE_ADD);

    MLTREEITEMW ti = {0};
    ti.size = sizeof(ti);
    ti.parentId = 0;
    ti.title = (wchar_t *)L"Music4All";
    ti.titleLen = 0;
    ti.hasChildren = FALSE;
    ti.imageIndex = imgIdx;
    ti.id = 0;

    SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM)&ti, ML_IPC_TREEITEM_ADDW);
    if (ti.id) {
        g_treeId = (INT_PTR)ti.id;
        g_treeApiNew = true;
    } else {
        // Fallback to legacy add-tree-item API.
        mlAddTreeItemStruct item = {0};
        item.parent_id = 0;
        item.title = (char *)"Music4All";
        item.has_children = 0;
        item.this_id = 0;

        SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM)&item, ML_IPC_ADDTREEITEM);
        g_treeId = item.this_id;
        g_treeApiNew = false;
    }
    return 0;
}

static void __cdecl quitMl() {
    if (g_hwndDialog) {
        if (g_listSkinHandle) {
            SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM)g_listSkinHandle, ML_IPC_UNSKIN_LISTVIEW);
            g_listSkinHandle = 0;
        }
        if (g_downloadsSkinHandle) {
            SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM)g_downloadsSkinHandle, ML_IPC_UNSKIN_LISTVIEW);
            g_downloadsSkinHandle = 0;
        }
        DestroyWindow(g_hwndDialog);
        g_hwndDialog = NULL;
        g_hwndList = NULL;
        g_hwndDownloads = NULL;
    }

    if (g_downloadInit) {
        InterlockedExchange(&g_downloadStop, 1);
        if (g_downloadEvent) SetEvent(g_downloadEvent);
        if (g_downloadThread) {
            DWORD wait = WaitForSingleObject(g_downloadThread, 5000);
            if (wait == WAIT_TIMEOUT) {
                TerminateThread(g_downloadThread, 0);
            }
            CloseHandle(g_downloadThread);
            g_downloadThread = NULL;
        }
        if (g_downloadEvent) {
            CloseHandle(g_downloadEvent);
            g_downloadEvent = NULL;
        }
        DeleteCriticalSection(&g_downloadCs);
        g_downloadInit = false;
        g_downloadJobs.clear();
        g_downloadQueue.clear();
    }

    if (g_treeId) {
        if (g_treeApiNew) {
            SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM)g_treeId, ML_IPC_TREEITEM_DELETE);
        } else {
            SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM)g_treeId, ML_IPC_DELTREEITEM);
        }
        g_treeId = 0;
        g_treeApiNew = false;
    }
}

static INT_PTR __cdecl messageProc(int message_type, INT_PTR p1, INT_PTR p2, INT_PTR p3) {
    (void)p3;

    if (message_type == ML_MSG_TREE_ONCREATEVIEW) {
        // param1 = param of tree item, param2 = HWND of parent. return HWND if it is us.
        // Some builds can be picky about the exact param value; also verify current tree selection.
        INT_PTR current = SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, 0, ML_IPC_GETCURTREEITEM);
        if (p1 != g_treeId && current != g_treeId) return 0;

        HWND parent = (HWND)p2;
        // Best-effort: if Media Library provides a current view container, prefer that as the parent.
        HWND currentView = (HWND)SendMessage(g_plugin.hwndLibraryParent, WM_ML_IPC, 0, ML_IPC_GETCURRENTVIEW);
        if (currentView) parent = currentView;

        // If ML destroyed the dialog (common when switching tabs), our cached handle can be stale.
        if (g_hwndDialog && !IsWindow(g_hwndDialog)) {
            g_hwndDialog = NULL;
            g_hwndList = NULL;
            g_hwndDownloads = NULL;
            g_listSkinHandle = 0;
            g_downloadsSkinHandle = 0;
            g_oldEditProc = NULL;
        }

        if (!g_hwndDialog) {
            g_hwndDialog = CreateDialog(g_plugin.hDllInstance, MAKEINTRESOURCE(IDD_DIALOG_ML), parent, DialogProc);
        } else {
            SetParent(g_hwndDialog, parent);
        }

        if (g_hwndDialog) {
            // Fill the parent (right panel) so we don't end up in the left column.
            RECT pr;
            GetClientRect(parent, &pr);
            SetWindowPos(g_hwndDialog, NULL, 0, 0, pr.right - pr.left, pr.bottom - pr.top, SWP_NOZORDER | SWP_NOACTIVATE);
            ShowWindow(g_hwndDialog, SW_SHOW);
            return (INT_PTR)g_hwndDialog;
        }
    }

    return 0;
}

extern "C" __declspec(dllexport) winampMediaLibraryPlugin *winampGetMediaLibraryPlugin() {
    g_plugin.version = MLHDR_VER;
    g_plugin.description = (char *)"Music4All (Media Library Panel)";
    g_plugin.init = initMl;
    g_plugin.quit = quitMl;
    g_plugin.MessageProc = messageProc;
    return &g_plugin;
}
