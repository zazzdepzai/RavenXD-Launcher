#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <urlmon.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

static const wchar_t* APP_NAME = L"RavenXD Launcher";
static const int W = 820, H = 520;
HWND g_hwnd{}, g_status{}, g_version{}, g_forge{}, g_ram{}, g_play{};
HFONT g_font{}, g_small{}, g_title{};
HBRUSH g_bg{}, g_card{}, g_white{}, g_blue{};
std::atomic_bool g_updateBusy{false};

struct Config {
    std::wstring updateUrl = L"https://example.com/ravenxd/version.txt";
    std::wstring modUrl = L"https://example.com/ravenxd/RavenXD.jar";
    std::wstring currentVersion = L"2.0";
};
Config cfg;

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path().wstring();
}
std::wstring ReadText(const fs::path& p) {
    std::wifstream f(p);
    f.imbue(std::locale(""));
    std::wstringstream ss; ss << f.rdbuf();
    return ss.str();
}
void WriteText(const fs::path& p, const std::wstring& s) {
    std::wofstream f(p); f.imbue(std::locale(""));
    f << s;
}
void SetStatus(const std::wstring& s) {
    if (g_status) SetWindowTextW(g_status, s.c_str());
}
std::wstring Trim(std::wstring s) {
    while (!s.empty() && iswspace(s.front())) s.erase(s.begin());
    while (!s.empty() && iswspace(s.back())) s.pop_back();
    return s;
}
void LoadConfig() {
    fs::path p = fs::path(ExeDir()) / L"launcher" / L"config.ini";
    if (!fs::exists(p)) {
        WriteText(p,
            L"# RavenXD Launcher update endpoints\n"
            L"VERSION_URL=https://example.com/ravenxd/version.txt\n"
            L"MOD_URL=https://example.com/ravenxd/RavenXD.jar\n"
            L"VERSION=2.0\n");
        return;
    }
    std::wifstream f(p);
    std::wstring line;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.rfind(L"VERSION_URL=", 0) == 0) cfg.updateUrl = line.substr(12);
        else if (line.rfind(L"MOD_URL=", 0) == 0) cfg.modUrl = line.substr(8);
        else if (line.rfind(L"VERSION=", 0) == 0) cfg.currentVersion = line.substr(8);
    }
}
fs::path MinecraftDir() {
    wchar_t* appdata = nullptr;
    size_t len = 0;
    _wdupenv_s(&appdata, &len, L"APPDATA");
    fs::path p = appdata ? fs::path(appdata) / L".minecraft" : fs::path();
    if (appdata) free(appdata);
    return p;
}
bool Download(const std::wstring& url, const fs::path& dst) {
    if (url.find(L"example.com") != std::wstring::npos) return false;
    return SUCCEEDED(URLDownloadToFileW(nullptr, url.c_str(), dst.c_str(), 0, nullptr));
}
void EnsureModFolder() {
    auto mods = MinecraftDir() / L"mods";
    fs::create_directories(mods);
}
void CheckUpdate(bool silent = false) {
    if (g_updateBusy.exchange(true)) return;
    SetStatus(L"Checking RavenXD mod update...");
    std::thread([silent] {
        fs::path cache = fs::path(ExeDir()) / L"launcher" / L"version.txt";
        std::wstring remote = L"";
        fs::path tmp = fs::path(ExeDir()) / L"launcher" / L".remote_version.txt";
        if (Download(cfg.updateUrl, tmp)) remote = Trim(ReadText(tmp));
        std::error_code ec; fs::remove(tmp, ec);

        if (!remote.empty() && remote != cfg.currentVersion) {
            SetStatus(L"New RavenXD version found: " + remote);
            EnsureModFolder();
            fs::path jar = MinecraftDir() / L"mods" / L"RavenXD.jar";
            fs::path tmpJar = fs::path(ExeDir()) / L"launcher" / L"RavenXD.new.jar";
            if (Download(cfg.modUrl, tmpJar)) {
                std::error_code e2;
                fs::copy_file(tmpJar, jar, fs::copy_options::overwrite_existing, e2);
                fs::remove(tmpJar, e2);
                if (!e2) {
                    WriteText(cache, remote);
                    SetStatus(L"RavenXD updated to v" + remote);
                } else SetStatus(L"Update failed.");
            } else SetStatus(L"Could not download RavenXD.jar.");
        } else {
            SetStatus(silent ? L"RavenXD is up to date." : L"RavenXD checked: latest version.");
        }
        g_updateBusy = false;
    }).detach();
}
void InstallLocalMod() {
    fs::path src = fs::path(ExeDir()) / L"mod" / L"RavenXD.jar";
    fs::path dst = MinecraftDir() / L"mods" / L"RavenXD.jar";
    if (!fs::exists(src)) {
        SetStatus(L"Put RavenXD.jar into launcher\\mod\\ first.");
        return;
    }
    EnsureModFolder();
    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    SetStatus(ec ? L"Could not install RavenXD.jar." : L"RavenXD.jar installed.");
}
void LaunchMinecraft() {
    InstallLocalMod();
    int forge = (int)SendMessageW(g_forge, CB_GETCURSEL, 0, 0);
    wchar_t ramBuf[32]{}; GetWindowTextW(g_ram, ramBuf, 32);
    std::wstring ram = ramBuf;
    if (ram.empty()) ram = L"2048";

    // Uses the official Minecraft launcher by default. Forge/None selection is retained
    // as launcher UI state; users can point to their preferred Java/launcher setup.
    wchar_t launcherPath[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, launcherPath);
    std::wstring msg =
        L"RavenXD Launcher\n\n"
        L"Version: 1.8.9\n"
        L"Loader: " + std::wstring(forge == 0 ? L"Forge" : L"None") +
        L"\nRAM: " + ram + L" MB\n\n"
        L"RavenXD.jar was copied to:\n" + (MinecraftDir() / L"mods" / L"RavenXD.jar").wstring() +
        L"\n\nOpen the official Minecraft launcher now?";
    if (MessageBoxW(g_hwnd, msg.c_str(), APP_NAME, MB_ICONINFORMATION | MB_YESNO) == IDYES) {
        ShellExecuteW(nullptr, L"open", L"minecraft://", nullptr, nullptr, SW_SHOWNORMAL);
    }
}
void DrawTextW2(HDC dc, const std::wstring& s, int x, int y, HFONT font, COLORREF color) {
    HFONT old = (HFONT)SelectObject(dc, font);
    SetTextColor(dc, color); SetBkMode(dc, TRANSPARENT);
    TextOutW(dc, x, y, s.c_str(), (int)s.size());
    SelectObject(dc, old);
}
LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch(m) {
    case WM_CREATE: {
        g_font = CreateFontW(17,0,0,0,500,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        g_small = CreateFontW(14,0,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        g_title = CreateFontW(30,0,0,0,700,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        g_bg = CreateSolidBrush(RGB(8,9,13)); g_card = CreateSolidBrush(RGB(17,20,29));
        g_white = CreateSolidBrush(RGB(245,247,255)); g_blue = CreateSolidBrush(RGB(123,172,240));
        g_status = CreateWindowW(L"STATIC", L"Ready.", WS_CHILD|WS_VISIBLE, 42, 448, 700, 25, h, nullptr, nullptr, nullptr);
        SendMessageW(g_status, WM_SETFONT, (WPARAM)g_small, TRUE);
        g_version = CreateWindowW(L"STATIC", L"RavenXD  v2.0", WS_CHILD|WS_VISIBLE, 42, 82, 300, 30, h, nullptr, nullptr, nullptr);
        SendMessageW(g_version, WM_SETFONT, (WPARAM)g_font, TRUE);
        g_forge = CreateWindowW(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST, 42, 180, 220, 34, h, nullptr, nullptr, nullptr);
        SendMessageW(g_forge, CB_ADDSTRING, 0, (LPARAM)L"Forge");
        SendMessageW(g_forge, CB_ADDSTRING, 0, (LPARAM)L"None");
        SendMessageW(g_forge, CB_SETCURSEL, 0, 0);
        g_ram = CreateWindowW(L"EDIT", L"2048", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_NUMBER, 300, 180, 130, 34, h, nullptr, nullptr, nullptr);
        g_play = CreateWindowW(L"BUTTON", L"PLAY 1.8.9", WS_CHILD|WS_VISIBLE|BS_OWNERDRAW, 42, 350, 220, 52, h, (HMENU)1001, nullptr, nullptr);
        HWND upd = CreateWindowW(L"BUTTON", L"CHECK UPDATE", WS_CHILD|WS_VISIBLE, 280, 350, 160, 52, h, (HMENU)1002, nullptr, nullptr);
        HWND inst = CreateWindowW(L"BUTTON", L"INSTALL MOD", WS_CHILD|WS_VISIBLE, 458, 350, 160, 52, h, (HMENU)1003, nullptr, nullptr);
        SendMessageW(upd, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(inst, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(g_forge, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(g_ram, WM_SETFONT, (WPARAM)g_font, TRUE);
        LoadConfig();
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w) == 1001) LaunchMinecraft();
        else if (LOWORD(w) == 1002) CheckUpdate(false);
        else if (LOWORD(w) == 1003) InstallLocalMod();
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w; SetBkColor(dc, RGB(8,9,13)); SetTextColor(dc, RGB(220,225,240)); return (LRESULT)g_bg;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT r; GetClientRect(h, &r);
        FillRect(dc, &r, g_bg);
        // glass cards
        RECT card{28, 62, 790, 330}; FillRect(dc, &card, g_card);
        DrawTextW2(dc, L"RavenXD", 42, 22, g_title, RGB(142,190,255));
        DrawTextW2(dc, L"iOS HUD Mini Launcher", 44, 54, g_small, RGB(135,145,165));
        DrawTextW2(dc, L"Minecraft Java Edition", 42, 126, g_small, RGB(135,145,165));
        DrawTextW2(dc, L"Version", 42, 158, g_small, RGB(135,145,165));
        DrawTextW2(dc, L"Loader", 42, 214, g_small, RGB(135,145,165));
        DrawTextW2(dc, L"RAM (MB)", 300, 214, g_small, RGB(135,145,165));
        DrawTextW2(dc, L"1.8.9", 42, 240, g_font, RGB(245,247,255));
        DrawTextW2(dc, L"RavenXD.jar", 42, 286, g_small, RGB(142,190,255));
        DrawTextW2(dc, L"Launcher\\mod\\RavenXD.jar  →  .minecraft\\mods\\RavenXD.jar", 42, 305, g_small, RGB(130,138,155));
        EndPaint(h, &ps); return 0;
    }
    case WM_DRAWITEM: {
        auto* d = (DRAWITEMSTRUCT*)l;
        FillRect(d->hDC, &d->rcItem, g_blue);
        DrawTextW2(d->hDC, L"PLAY 1.8.9", d->rcItem.left+55, d->rcItem.top+16, g_font, RGB(8,10,15));
        return TRUE;
    }
    case WM_DESTROY:
        DeleteObject(g_font); DeleteObject(g_small); DeleteObject(g_title);
        DeleteObject(g_bg); DeleteObject(g_card); DeleteObject(g_white); DeleteObject(g_blue);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    WNDCLASSW wc{}; wc.hInstance=inst; wc.lpfnWndProc=WndProc; wc.lpszClassName=L"RavenXDLauncher";
    wc.hCursor=LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, APP_NAME, WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, W, H, nullptr, nullptr, inst, nullptr);
    ShowWindow(g_hwnd, show); UpdateWindow(g_hwnd);
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)){ TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
