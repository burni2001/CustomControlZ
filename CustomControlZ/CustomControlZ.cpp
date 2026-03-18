#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <strsafe.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <uxtheme.h>
#include <dwmapi.h>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup")

// --- DARK MODE ---

enum PreferredAppMode { Default, AllowDark, ForceDark, ForceLight, Max };
typedef PreferredAppMode(WINAPI* fnSetPreferredAppMode)(PreferredAppMode appMode);

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

void EnableWindows11DarkMode() {
    HMODULE hUxtheme = LoadLibraryEx(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUxtheme) {
        auto SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(
            GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135)));
        if (SetPreferredAppMode) SetPreferredAppMode(AllowDark);
    }
}

void EnableDarkTitleBar(HWND hwnd) {
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
}

// --- RESOURCE IDs ---

#include "resource.h"
#define ICON_ID_EXE    108
#define ICON_ID_IDLE   109
#define ICON_ID_ACTIVE 110

// --- WINDOW/CONTROL IDs ---

#define WM_TRAYICON         (WM_USER + 1)
#define ID_TRAY_SETTINGS    1001
#define ID_TRAY_EXIT        1002
#define ID_TRAY_CHANGE_GAME 1003

// Settings window control IDs
#define BTN_BIND_BASE       2001  // Bind buttons: BTN_BIND_BASE + binding index
#define ID_LABEL_BASE       2200  // Label statics: ID_LABEL_BASE + binding index
#define ID_TITLE_STATIC     2100
#define ID_IMPRINT1_STATIC  2101
#define ID_IMPRINT2_STATIC  2102
#define BTN_EXIT_SETTINGS   3001
#define BTN_EXIT_APP        3002
#define BTN_FONT_SETTINGS   3003

// Game selection window control IDs
#define BTN_GAME_BASE       4000  // Game buttons: BTN_GAME_BASE + profile index
#define BTN_SELECT_EXIT     4999
#define ID_SELECT_TITLE     5000
#define ID_SELECT_SUBTITLE  5001

// --- TIMING & BUFFER CONSTANTS ---

constexpr int SHELL_CHECK_INTERVAL_MS = 100;
constexpr int TRAY_RETRY_INTERVAL_MS  = 500;
constexpr int SHELL_TIMEOUT_SEC       = 30;
constexpr int TRAY_MAX_RETRIES        = 10;

constexpr size_t KEY_NAME_BUFFER    = 64;
constexpr size_t BUTTON_TEXT_BUFFER = 256;
constexpr size_t CONFIG_BUFFER      = 32;
constexpr size_t FONT_NAME_BUFFER   = 64;

// --- LAYOUT CONSTANTS (settings window) ---

constexpr int LAYOUT_LEFT_MARGIN          = 50;
constexpr int LAYOUT_LABEL_WIDTH          = 420;
constexpr int LAYOUT_BUTTON_WIDTH         = 180;
constexpr int LAYOUT_BUTTON_GAP           = 10;
constexpr int LAYOUT_ROW_HEIGHT           = 60;
constexpr int LAYOUT_BUTTON_HEIGHT        = 40;
constexpr int LAYOUT_TITLE_START          = 30;
constexpr int LAYOUT_TITLE_HEIGHT         = 55;
constexpr int LAYOUT_TITLE_SPACING        = 75;
constexpr int LAYOUT_BOTTOM_BUTTON_HEIGHT = 50;
constexpr int LAYOUT_BOTTOM_BUTTON_WIDTH  = 200;
constexpr int LAYOUT_BOTTOM_BUTTON_GAP    = 30;
constexpr int LAYOUT_BOTTOM_SPACING       = 70;
constexpr int LAYOUT_IMPRINT_HEIGHT       = 22;
constexpr int LAYOUT_IMPRINT_SPACING      = 24;
constexpr int LAYOUT_ROW_PADDING          = 10;
constexpr int LAYOUT_LINE_WIDTH           = 620;
constexpr int WINDOW_WIDTH                = 720;
constexpr int LAYOUT_FONT_BUTTON_WIDTH    = 120;
constexpr int LAYOUT_FONT_BUTTON_HEIGHT   = 35;

// Game select window layout
constexpr int SELECT_WIN_WIDTH     = 500;
constexpr int SELECT_TITLE_Y       = 20;
constexpr int SELECT_SUBTITLE_Y    = 75;
constexpr int SELECT_FIRST_BTN_Y   = 130;
constexpr int SELECT_BTN_HEIGHT    = 65;
constexpr int SELECT_BTN_SPACING   = 85;
constexpr int SELECT_BTN_MARGIN_X  = 40;
constexpr int SELECT_EXIT_HEIGHT   = 40;
constexpr int SELECT_EXIT_WIDTH    = 120;
constexpr int SELECT_BOTTOM_MARGIN = 20;

// --- GAME PROFILE ARCHITECTURE ---

#include "GameProfiles.h"

// --- GLOBAL STATE ---

const wchar_t* CONFIG_FILE = L".\\settings.ini";
const wchar_t* MUTEX_NAME  = L"CustomControlZ_Unique_ID";

std::mutex g_configMutex;
wchar_t g_fontName[FONT_NAME_BUFFER] = L"Palatino Linotype";

HINSTANCE g_hInstance     = nullptr;
HWND g_hMainWindow        = nullptr;
HWND g_hSettingsWnd       = nullptr;
HWND g_hGameSelectWnd     = nullptr;
NOTIFYICONDATA g_nid      = {};
std::atomic<bool> g_isAppRunning(true);
std::atomic<int>  g_waitingForBindID(0);
HICON g_hIconIdle         = nullptr;
HICON g_hIconActive       = nullptr;
HICON g_hIconExe          = nullptr;
bool  g_customIconsLoaded = false;

GameProfile* g_activeProfile = nullptr;

HBRUSH g_hBrushBg     = nullptr;
HBRUSH g_hBrushButton = nullptr;
HBRUSH g_hBrushExit   = nullptr;
HFONT  g_hFontTitle   = nullptr;
HFONT  g_hFontNormal  = nullptr;
HFONT  g_hFontButton  = nullptr;
HFONT  g_hFontImprint = nullptr;

std::thread       g_logicThread;
std::atomic<bool> g_logicRunning(false);

// --- FONT HELPERS ---

inline HFONT CreateUIFont(int height, int weight = FW_NORMAL) {
    std::lock_guard<std::mutex> lock(g_configMutex);
    return CreateFont(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, g_fontName);
}

void RecreateAllFonts() {
    if (g_hFontTitle)   DeleteObject(g_hFontTitle);
    if (g_hFontNormal)  DeleteObject(g_hFontNormal);
    if (g_hFontButton)  DeleteObject(g_hFontButton);
    if (g_hFontImprint) DeleteObject(g_hFontImprint);

    g_hFontTitle   = CreateUIFont(48, FW_BOLD);
    g_hFontNormal  = CreateUIFont(28);
    g_hFontButton  = CreateUIFont(24);
    g_hFontImprint = CreateUIFont(16);
}

void UpdateAllControlFonts(HWND hwnd) {
    SendMessage(GetDlgItem(hwnd, ID_TITLE_STATIC),    WM_SETFONT, (WPARAM)g_hFontTitle,   TRUE);
    SendMessage(GetDlgItem(hwnd, ID_IMPRINT1_STATIC), WM_SETFONT, (WPARAM)g_hFontImprint, TRUE);
    SendMessage(GetDlgItem(hwnd, ID_IMPRINT2_STATIC), WM_SETFONT, (WPARAM)g_hFontImprint, TRUE);

    if (g_activeProfile) {
        for (int i = 0; i < g_activeProfile->bindingCount; i++) {
            SendMessage(GetDlgItem(hwnd, ID_LABEL_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            InvalidateRect(GetDlgItem(hwnd, BTN_BIND_BASE + i), nullptr, TRUE);
        }
    }

    InvalidateRect(GetDlgItem(hwnd, BTN_FONT_SETTINGS), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, BTN_EXIT_SETTINGS), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, BTN_EXIT_APP),      nullptr, TRUE);
    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
}

struct ButtonStyle {
    HBRUSH   brush;
    COLORREF borderColor;
    COLORREF textColor;
    HFONT    font;
};

ButtonStyle GetButtonStyle(UINT ctlID) {
    if (!g_activeProfile) return { g_hBrushButton, RGB(100,100,100), RGB(200,200,200), g_hFontButton };
    const Theme& t = g_activeProfile->theme;
    if (ctlID == BTN_EXIT_APP)      return { g_hBrushExit,   t.exitBorder,     t.exitText, g_hFontNormal };
    if (ctlID == BTN_EXIT_SETTINGS) return { g_hBrushButton, t.minimizeBorder, t.text,     g_hFontNormal };
    if (ctlID == BTN_FONT_SETTINGS) return { g_hBrushButton, t.border,         t.accent,   g_hFontButton };
    return { g_hBrushButton, t.border, t.accent, g_hFontButton };
}

void CleanupFonts() {
    if (g_hFontTitle)   { DeleteObject(g_hFontTitle);   g_hFontTitle   = nullptr; }
    if (g_hFontNormal)  { DeleteObject(g_hFontNormal);  g_hFontNormal  = nullptr; }
    if (g_hFontButton)  { DeleteObject(g_hFontButton);  g_hFontButton  = nullptr; }
    if (g_hFontImprint) { DeleteObject(g_hFontImprint); g_hFontImprint = nullptr; }
}

void CleanupBrushes() {
    if (g_hBrushBg)     { DeleteObject(g_hBrushBg);     g_hBrushBg     = nullptr; }
    if (g_hBrushButton) { DeleteObject(g_hBrushButton); g_hBrushButton = nullptr; }
    if (g_hBrushExit)   { DeleteObject(g_hBrushExit);   g_hBrushExit   = nullptr; }
}

void CleanupIcons() {
    if (g_customIconsLoaded) {
        if (g_hIconIdle)   DestroyIcon(g_hIconIdle);
        if (g_hIconActive) DestroyIcon(g_hIconActive);
        if (g_hIconExe)    DestroyIcon(g_hIconExe);
    }
}

void RebuildThemeBrushes(GameProfile* profile) {
    CleanupBrushes();
    g_hBrushBg     = CreateSolidBrush(profile->theme.bg);
    g_hBrushButton = CreateSolidBrush(profile->theme.button);
    g_hBrushExit   = CreateSolidBrush(profile->theme.exitFill);
}

// --- TRAY MENU ---

HMENU CreateTrayMenu() {
    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
        if (g_activeProfile && g_hSettingsWnd) {
            AppendMenu(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Settings");
        } else {
            AppendMenu(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Choose Game");
        }
        AppendMenu(hMenu, MF_STRING, ID_TRAY_CHANGE_GAME, L"Change Game");
        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
    }
    return hMenu;
}

// --- FONT CYCLING ---

void CycleFontToNext() {
    wchar_t current[FONT_NAME_BUFFER];
    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        StringCchCopy(current, ARRAYSIZE(current), g_fontName);
    }

    const wchar_t* next = L"Palatino Linotype";
    if (_wcsicmp(current, L"Palatino Linotype") == 0) next = L"Segoe UI";
    else if (_wcsicmp(current, L"Segoe UI") == 0)      next = L"Comic Sans MS";

    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        StringCchCopy(g_fontName, ARRAYSIZE(g_fontName), next);
    }
}

// --- CONFIG MANAGEMENT ---

inline bool IsValidKey(WORD key) {
    return key > 0 && key <= 0xFF;
}

void SaveConfig(GameProfile* profile) {
    wchar_t localFontName[FONT_NAME_BUFFER];
    WORD vals[MAX_BINDINGS] = {};
    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        StringCchCopy(localFontName, ARRAYSIZE(localFontName), g_fontName);
        for (int i = 0; i < profile->bindingCount; i++) {
            vals[i] = profile->bindings[i].currentVk;
        }
    }

    wchar_t buf[CONFIG_BUFFER];
    for (int i = 0; i < profile->bindingCount; i++) {
        StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", vals[i]);
        WritePrivateProfileString(profile->iniSection, profile->bindings[i].iniKey, buf, CONFIG_FILE);
    }
    WritePrivateProfileString(L"UI", L"FontName", localFontName, CONFIG_FILE);
}

void LoadConfig(GameProfile* profile) {
    WORD vals[MAX_BINDINGS] = {};
    for (int i = 0; i < profile->bindingCount; i++) {
        vals[i] = static_cast<WORD>(
            GetPrivateProfileInt(profile->iniSection, profile->bindings[i].iniKey,
                                 profile->bindings[i].defaultVk, CONFIG_FILE));
    }

    wchar_t tempFont[FONT_NAME_BUFFER];
    GetPrivateProfileString(L"UI", L"FontName", L"Palatino Linotype",
                            tempFont, ARRAYSIZE(tempFont), CONFIG_FILE);

    std::lock_guard<std::mutex> lock(g_configMutex);
    for (int i = 0; i < profile->bindingCount; i++) {
        if (IsValidKey(vals[i])) profile->bindings[i].currentVk = vals[i];
    }
    StringCchCopy(g_fontName, ARRAYSIZE(g_fontName), tempFont);
}

// --- KEY NAMING ---

void GetKeyName(WORD vk, wchar_t* buffer, size_t size) {
    switch (vk) {
    case VK_LEFT:    StringCchCopy(buffer, size, L"Left Arrow");  return;
    case VK_RIGHT:   StringCchCopy(buffer, size, L"Right Arrow"); return;
    case VK_UP:      StringCchCopy(buffer, size, L"Up Arrow");    return;
    case VK_DOWN:    StringCchCopy(buffer, size, L"Down Arrow");  return;
    case VK_CAPITAL: StringCchCopy(buffer, size, L"Caps Lock");   return;
    case VK_ESCAPE:  StringCchCopy(buffer, size, L"Escape");      return;
    case VK_SPACE:   StringCchCopy(buffer, size, L"Space");       return;
    case VK_RETURN:  StringCchCopy(buffer, size, L"Enter");       return;
    case VK_TAB:     StringCchCopy(buffer, size, L"Tab");         return;
    case VK_SHIFT:   StringCchCopy(buffer, size, L"Shift");       return;
    case VK_CONTROL: StringCchCopy(buffer, size, L"Ctrl");        return;
    case VK_MENU:    StringCchCopy(buffer, size, L"Alt");         return;
    case VK_PRIOR:   StringCchCopy(buffer, size, L"Page Up");     return;
    case VK_NEXT:    StringCchCopy(buffer, size, L"Page Down");   return;
    case VK_HOME:    StringCchCopy(buffer, size, L"Home");        return;
    case VK_END:     StringCchCopy(buffer, size, L"End");         return;
    case VK_INSERT:  StringCchCopy(buffer, size, L"Insert");      return;
    case VK_DELETE:  StringCchCopy(buffer, size, L"Delete");      return;
    }

    UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    if (vk >= VK_PRIOR && vk <= VK_HELP) scanCode |= 0x100;

    if (scanCode == 0 || GetKeyNameTextW(scanCode << 16, buffer, static_cast<int>(size)) == 0) {
        StringCchPrintf(buffer, size, L"Key %d", vk);
    }
}

void UpdateButtonText(HWND hBtn, WORD vk) {
    wchar_t keyName[KEY_NAME_BUFFER];
    wchar_t fullText[BUTTON_TEXT_BUFFER];
    GetKeyName(vk, keyName, ARRAYSIZE(keyName));
    StringCchPrintf(fullText, ARRAYSIZE(fullText), L"[ %s ]", keyName);
    SetWindowText(hBtn, fullText);
}

// --- SYSTEM HELPERS ---

bool IsGameRunning(GameProfile* profile) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            bool match = (_wcsicmp(pe.szExeFile, profile->processName1) == 0);
            if (!match && profile->processName2) {
                match = (_wcsicmp(pe.szExeFile, profile->processName2) == 0);
            }
            if (match) { found = true; break; }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return found;
}

inline void SendKeyInput(WORD vk, bool keyUp) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    if (scanCode == 0) {
        input.ki.wVk     = vk;
        input.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
    } else {
        input.ki.wScan   = static_cast<WORD>(scanCode);
        input.ki.dwFlags = KEYEVENTF_SCANCODE | (keyUp ? KEYEVENTF_KEYUP : 0);
    }
    SendInput(1, &input, sizeof(INPUT));
}

inline void PressKey(WORD vk)   { SendKeyInput(vk, false); }
inline void ReleaseKey(WORD vk) { SendKeyInput(vk, true);  }
inline bool IsKeyDown(WORD vk)  { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

inline void PressMouse(WORD vk) {
    INPUT inp = {};
    inp.type = INPUT_MOUSE;
    switch (vk) {
        case VK_LBUTTON: inp.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;   break;
        case VK_RBUTTON: inp.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;  break;
        case VK_MBUTTON: inp.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN; break;
        default: return;
    }
    SendInput(1, &inp, sizeof(INPUT));
}

inline void ReleaseMouse(WORD vk) {
    INPUT inp = {};
    inp.type = INPUT_MOUSE;
    switch (vk) {
        case VK_LBUTTON: inp.mi.dwFlags = MOUSEEVENTF_LEFTUP;   break;
        case VK_RBUTTON: inp.mi.dwFlags = MOUSEEVENTF_RIGHTUP;  break;
        case VK_MBUTTON: inp.mi.dwFlags = MOUSEEVENTF_MIDDLEUP; break;
        default: return;
    }
    SendInput(1, &inp, sizeof(INPUT));
}

void SetTrayIconState(bool active, GameProfile* profile) {
    g_nid.hIcon = active ? g_hIconActive : g_hIconIdle;
    StringCchCopy(g_nid.szTip, ARRAYSIZE(g_nid.szTip),
                  active ? profile->tipActive : profile->tipIdle);
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
}

// Returns true if the named process is running at High integrity (elevated / as Administrator).
// Uses OpenProcessToken + GetTokenInformation(TokenIntegrityLevel).
// NOTE: GetLastError after SendInput does NOT distinguish UIPI failures from other failures.
// This proactive check is the only reliable way to detect UIPI risk. (FIX-02)
bool IsProcessRunningElevated(const wchar_t* processName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(PROCESSENTRY32W);
    bool elevated = false;

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) {
                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (hProc) {
                    HANDLE hToken = nullptr;
                    if (OpenProcessToken(hProc, TOKEN_QUERY, &hToken)) {
                        DWORD infoLen = 0;
                        GetTokenInformation(hToken, TokenIntegrityLevel, nullptr, 0, &infoLen);
                        if (infoLen > 0) {
                            auto* label = static_cast<TOKEN_MANDATORY_LABEL*>(_alloca(infoLen));
                            if (GetTokenInformation(hToken, TokenIntegrityLevel, label, infoLen, &infoLen)) {
                                DWORD level = *GetSidSubAuthority(
                                    label->Label.Sid,
                                    *GetSidSubAuthorityCount(label->Label.Sid) - 1);
                                elevated = (level >= SECURITY_MANDATORY_HIGH_RID);
                            }
                        }
                        CloseHandle(hToken);
                    }
                    CloseHandle(hProc);
                }
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return elevated;
}

// --- GAME LOGIC THREAD MANAGEMENT ---

void StopGameLogicThread() {
    if (g_logicRunning) {
        g_logicRunning = false;
        if (g_logicThread.joinable()) g_logicThread.join();
    }
}

void StartGameLogicThread(GameProfile* profile) {
    StopGameLogicThread();
    g_logicRunning = true;
    g_logicThread = std::thread([profile]() {
        profile->logicFn(profile, g_logicRunning);
    });
}

// --- GAME PROFILES (included after utility functions they depend on) ---

#include "games/EldenRing.h"
#include "games/ToxicCommando.h"

GameProfile* g_gameProfiles[] = {
    &g_EldenRingProfile,
    &g_ToxicCommandoProfile,
};
constexpr int g_gameProfileCount = static_cast<int>(ARRAYSIZE(g_gameProfiles));

// --- SETTINGS WINDOW ---

inline bool IsModifierOnlyKey(WORD vk) {
    return vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU ||
           vk == VK_LSHIFT || vk == VK_RSHIFT ||
           vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_LMENU || vk == VK_RMENU ||
           vk == VK_LWIN || vk == VK_RWIN;
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        EnableDarkTitleBar(hwnd);
        RecreateAllFonts();

        GameProfile* profile = g_activeProfile;
        const int buttonX = LAYOUT_LEFT_MARGIN + LAYOUT_LABEL_WIDTH + LAYOUT_BUTTON_GAP;
        int rowBaseY = LAYOUT_TITLE_START + LAYOUT_TITLE_SPACING;

        // Font button (top-left corner)
        CreateWindow(L"BUTTON", L"Change Font",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            10, 10, LAYOUT_FONT_BUTTON_WIDTH, LAYOUT_FONT_BUTTON_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_FONT_SETTINGS, nullptr, nullptr);

        // Title
        HWND hTitle = CreateWindow(L"STATIC", L"Key Bindings",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, LAYOUT_TITLE_START, WINDOW_WIDTH, LAYOUT_TITLE_HEIGHT,
            hwnd, (HMENU)(INT_PTR)ID_TITLE_STATIC, nullptr, nullptr);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

        // Binding rows (one per profile binding)
        for (int i = 0; i < profile->bindingCount; i++) {
            int rowY  = rowBaseY + i * LAYOUT_ROW_HEIGHT;
            int labelY = rowY + 8; // small gap below separator line

            HWND hLabel = CreateWindow(L"STATIC", profile->bindings[i].label,
                WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                LAYOUT_LEFT_MARGIN, labelY, LAYOUT_LABEL_WIDTH, LAYOUT_BUTTON_HEIGHT,
                hwnd, (HMENU)(INT_PTR)(ID_LABEL_BASE + i), nullptr, nullptr);
            SendMessage(hLabel, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            CreateWindow(L"BUTTON", L"...",
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                buttonX, labelY, LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                hwnd, (HMENU)(INT_PTR)(BTN_BIND_BASE + i), nullptr, nullptr);
        }

        // Bottom buttons
        int bottomY = rowBaseY + profile->bindingCount * LAYOUT_ROW_HEIGHT + 15;
        int buttonsStartX = (WINDOW_WIDTH - LAYOUT_BOTTOM_BUTTON_WIDTH * 2 - LAYOUT_BOTTOM_BUTTON_GAP) / 2;

        CreateWindow(L"BUTTON", L"Minimize",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            buttonsStartX, bottomY, LAYOUT_BOTTOM_BUTTON_WIDTH, LAYOUT_BOTTOM_BUTTON_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_EXIT_SETTINGS, nullptr, nullptr);

        CreateWindow(L"BUTTON", L"Exit",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            buttonsStartX + LAYOUT_BOTTOM_BUTTON_WIDTH + LAYOUT_BOTTOM_BUTTON_GAP, bottomY,
            LAYOUT_BOTTOM_BUTTON_WIDTH, LAYOUT_BOTTOM_BUTTON_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_EXIT_APP, nullptr, nullptr);

        // Imprint
        int imprintY = bottomY + LAYOUT_BOTTOM_SPACING;
        HWND hImp1 = CreateWindow(L"STATIC", L"Idea and development: B\u00e9rni (burni2001)",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, imprintY, WINDOW_WIDTH, LAYOUT_IMPRINT_HEIGHT,
            hwnd, (HMENU)(INT_PTR)ID_IMPRINT1_STATIC, nullptr, nullptr);
        SendMessage(hImp1, WM_SETFONT, (WPARAM)g_hFontImprint, TRUE);

        imprintY += LAYOUT_IMPRINT_SPACING;
        HWND hImp2 = CreateWindow(L"STATIC", L"Development tools: Visual Studio, GitHub Copilot",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, imprintY, WINDOW_WIDTH, LAYOUT_IMPRINT_HEIGHT,
            hwnd, (HMENU)(INT_PTR)ID_IMPRINT2_STATIC, nullptr, nullptr);
        SendMessage(hImp2, WM_SETFONT, (WPARAM)g_hFontImprint, TRUE);

        // Set initial button texts
        std::lock_guard<std::mutex> lock(g_configMutex);
        for (int i = 0; i < profile->bindingCount; i++) {
            UpdateButtonText(GetDlgItem(hwnd, BTN_BIND_BASE + i), profile->bindings[i].currentVk);
        }
        break;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_hBrushBg);
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        if (g_activeProfile) {
            HBRUSH hRowBrush = CreateSolidBrush(g_activeProfile->theme.rowBg);
            HPEN   hLinePen  = CreatePen(PS_SOLID, 2, g_activeProfile->theme.separator);
            HPEN   hOldPen   = (HPEN)SelectObject(hdc, hLinePen);

            int rowBaseY = LAYOUT_TITLE_START + LAYOUT_TITLE_SPACING;
            for (int i = 0; i < g_activeProfile->bindingCount; i++) {
                int rowY = rowBaseY + i * LAYOUT_ROW_HEIGHT;

                // Separator line
                MoveToEx(hdc, LAYOUT_LEFT_MARGIN, rowY, nullptr);
                LineTo(hdc, LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH, rowY);

                // Row background highlight
                RECT rowRect = {
                    LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING,
                    rowY + 5,
                    LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH + LAYOUT_ROW_PADDING,
                    rowY + 5 + LAYOUT_BUTTON_HEIGHT
                };
                FillRect(hdc, &rowRect, hRowBrush);
            }

            SelectObject(hdc, hOldPen);
            DeleteObject(hLinePen);
            DeleteObject(hRowBrush);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        if (g_activeProfile) SetTextColor(hdcStatic, g_activeProfile->theme.text);
        else                  SetTextColor(hdcStatic, RGB(200, 200, 200));
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (pDIS->CtlType == ODT_BUTTON) {
            HDC hdc = pDIS->hDC;
            RECT rc = pDIS->rcItem;

            ButtonStyle style = GetButtonStyle(pDIS->CtlID);
            FillRect(hdc, &rc, style.brush);

            HPEN hPen     = CreatePen(PS_SOLID, 2, style.borderColor);
            HPEN hOldPen  = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBr);
            DeleteObject(hPen);

            wchar_t text[BUTTON_TEXT_BUFFER];
            GetWindowText(pDIS->hwndItem, text, ARRAYSIZE(text));
            HFONT hOldFont = (HFONT)SelectObject(hdc, style.font);
            SetTextColor(hdc, style.textColor);
            SetBkMode(hdc, TRANSPARENT);
            DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, hOldFont);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == BTN_EXIT_SETTINGS) {
            ShowWindow(hwnd, SW_HIDE);
            g_waitingForBindID = 0;
        } else if (id == BTN_EXIT_APP) {
            if (MessageBox(hwnd, L"Are you sure you want to exit?", L"Exit",
                           MB_YESNO | MB_ICONQUESTION) == IDYES) {
                DestroyWindow(g_hMainWindow);
            }
        } else if (id == BTN_FONT_SETTINGS) {
            CycleFontToNext();
            RecreateAllFonts();
            UpdateAllControlFonts(hwnd);
            if (g_activeProfile) SaveConfig(g_activeProfile);
        } else if (id >= BTN_BIND_BASE && g_activeProfile &&
                   id < BTN_BIND_BASE + g_activeProfile->bindingCount) {
            g_waitingForBindID = id;
            SetWindowText(GetDlgItem(hwnd, id), L"...");
            SetFocus(hwnd);
        }
        break;
    }

    case WM_KEYDOWN: {
        int bindID = g_waitingForBindID.load();

        if (wParam == VK_ESCAPE && bindID == 0) {
            ShowWindow(hwnd, SW_HIDE);
            break;
        }

        if (bindID != 0 && g_activeProfile) {
            WORD newKey = static_cast<WORD>(wParam);
            if (IsModifierOnlyKey(newKey)) {
                MessageBox(hwnd, L"Modifier keys alone are not allowed!",
                           L"Invalid Key", MB_OK | MB_ICONWARNING);
                g_waitingForBindID = 0;
                break;
            }
            int idx = bindID - BTN_BIND_BASE;
            {
                std::lock_guard<std::mutex> lock(g_configMutex);
                g_activeProfile->bindings[idx].currentVk = newKey;
                UpdateButtonText(GetDlgItem(hwnd, bindID), newKey);
            }
            g_waitingForBindID = 0;
            SaveConfig(g_activeProfile);
        }
        break;
    }

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        g_waitingForBindID = 0;
        return 0;

    case WM_DESTROY:
        CleanupFonts();
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool CreateSettingsWindow(HINSTANCE hInstance, GameProfile* profile) {
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASS wc = {};
        wc.lpfnWndProc   = SettingsProc;
        wc.hInstance     = hInstance;
        wc.lpszClassName = L"CustomControlZSettingsClass";
        wc.hbrBackground = nullptr; // Handled in WM_ERASEBKGND
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(ICON_ID_EXE));
        if (!RegisterClass(&wc)) return false;
        classRegistered = true;
    }

    int windowHeight = LAYOUT_TITLE_START + LAYOUT_TITLE_SPACING
                     + profile->bindingCount * LAYOUT_ROW_HEIGHT
                     + 15
                     + LAYOUT_BOTTOM_BUTTON_HEIGHT
                     + LAYOUT_BOTTOM_SPACING
                     + LAYOUT_IMPRINT_HEIGHT + LAYOUT_IMPRINT_SPACING
                     + LAYOUT_IMPRINT_HEIGHT
                     + 40;

    g_hSettingsWnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        L"CustomControlZSettingsClass",
        profile->settingsTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, windowHeight,
        nullptr, nullptr, hInstance, nullptr
    );
    return g_hSettingsWnd != nullptr;
}

// --- GAME SELECTION WINDOW ---

void OnGameSelected(int profileIndex) {
    StopGameLogicThread();

    g_activeProfile = g_gameProfiles[profileIndex];
    LoadConfig(g_activeProfile);
    RebuildThemeBrushes(g_activeProfile);

    if (g_hSettingsWnd) {
        DestroyWindow(g_hSettingsWnd);
        g_hSettingsWnd = nullptr;
    }

    if (!CreateSettingsWindow(g_hInstance, g_activeProfile)) return;

    ShowWindow(g_hGameSelectWnd, SW_HIDE);
    ShowWindow(g_hSettingsWnd, SW_SHOW);
    SetForegroundWindow(g_hSettingsWnd);

    StartGameLogicThread(g_activeProfile);
}

void ShowChangeGameUI() {
    StopGameLogicThread();
    if (g_hSettingsWnd)   ShowWindow(g_hSettingsWnd, SW_HIDE);
    if (g_hGameSelectWnd) {
        ShowWindow(g_hGameSelectWnd, SW_SHOW);
        SetForegroundWindow(g_hGameSelectWnd);
    }
}

LRESULT CALLBACK GameSelectProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        EnableDarkTitleBar(hwnd);

        HFONT hFontTitle    = CreateFont(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hFontSubtitle = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hFontGameBtn  = CreateFont(24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        // Store fonts in GWLP_USERDATA for cleanup
        HFONT* fonts = new HFONT[3]{ hFontTitle, hFontSubtitle, hFontGameBtn };
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)fonts);

        HWND hTitle = CreateWindow(L"STATIC", L"CustomControlZ",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, SELECT_TITLE_Y, SELECT_WIN_WIDTH, 42, hwnd,
            (HMENU)(INT_PTR)ID_SELECT_TITLE, nullptr, nullptr);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

        HWND hSub = CreateWindow(L"STATIC", L"Choose your game:",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, SELECT_SUBTITLE_Y, SELECT_WIN_WIDTH, 28, hwnd,
            (HMENU)(INT_PTR)ID_SELECT_SUBTITLE, nullptr, nullptr);
        SendMessage(hSub, WM_SETFONT, (WPARAM)hFontSubtitle, TRUE);

        for (int i = 0; i < g_gameProfileCount; i++) {
            int btnY = SELECT_FIRST_BTN_Y + i * SELECT_BTN_SPACING;
            CreateWindow(L"BUTTON", g_gameProfiles[i]->displayName,
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                SELECT_BTN_MARGIN_X, btnY,
                SELECT_WIN_WIDTH - 2 * SELECT_BTN_MARGIN_X, SELECT_BTN_HEIGHT,
                hwnd, (HMENU)(INT_PTR)(BTN_GAME_BASE + i), nullptr, nullptr);
        }

        // Exit button at the bottom
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        int exitY = clientRect.bottom - SELECT_EXIT_HEIGHT - SELECT_BOTTOM_MARGIN;

        CreateWindow(L"BUTTON", L"Exit",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            SELECT_WIN_WIDTH - SELECT_EXIT_WIDTH - 15, exitY,
            SELECT_EXIT_WIDTH, SELECT_EXIT_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_SELECT_EXIT, nullptr, nullptr);
        break;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH hDark = CreateSolidBrush(RGB(20, 20, 20));
        FillRect(hdc, &rc, hDark);
        DeleteObject(hDark);
        return 1;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(240, 240, 240));
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (pDIS->CtlType != ODT_BUTTON) break;

        HDC hdc = pDIS->hDC;
        RECT rc = pDIS->rcItem;
        HFONT* fonts    = (HFONT*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        HFONT hBtnFont  = fonts ? fonts[2] : nullptr;

        if (pDIS->CtlID == BTN_SELECT_EXIT) {
            HBRUSH hBr = CreateSolidBrush(RGB(140, 30, 30));
            FillRect(hdc, &rc, hBr);
            DeleteObject(hBr);
            HPEN hPen    = CreatePen(PS_SOLID, 2, RGB(200, 70, 70));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, hOldPen); SelectObject(hdc, hOldBr);
            DeleteObject(hPen);
            if (hBtnFont) SelectObject(hdc, hBtnFont);
            SetTextColor(hdc, RGB(255, 210, 210));
            SetBkMode(hdc, TRANSPARENT);
            DrawText(hdc, L"Exit", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            int idx = (int)pDIS->CtlID - BTN_GAME_BASE;
            if (idx >= 0 && idx < g_gameProfileCount) {
                GameProfile* gp = g_gameProfiles[idx];
                HBRUSH hBr = CreateSolidBrush(gp->theme.button);
                FillRect(hdc, &rc, hBr);
                DeleteObject(hBr);
                HPEN hPen    = CreatePen(PS_SOLID, 2, gp->theme.border);
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
                SelectObject(hdc, hOldPen); SelectObject(hdc, hOldBr);
                DeleteObject(hPen);
                if (hBtnFont) SelectObject(hdc, hBtnFont);
                SetTextColor(hdc, gp->theme.accent);
                SetBkMode(hdc, TRANSPARENT);
                DrawText(hdc, gp->displayName, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
        return TRUE;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == BTN_SELECT_EXIT) {
            DestroyWindow(g_hMainWindow);
        } else if (id >= BTN_GAME_BASE && id < BTN_GAME_BASE + g_gameProfileCount) {
            OnGameSelected(id - BTN_GAME_BASE);
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(g_hMainWindow);
        return 0;

    case WM_DESTROY: {
        HFONT* fonts = (HFONT*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (fonts) {
            for (int i = 0; i < 3; i++) if (fonts[i]) DeleteObject(fonts[i]);
            delete[] fonts;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        break;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool CreateGameSelectionWindow(HINSTANCE hInstance) {
    WNDCLASS wc = {};
    wc.lpfnWndProc   = GameSelectProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"CustomControlZSelectClass";
    wc.hbrBackground = nullptr;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(ICON_ID_EXE));
    if (!RegisterClass(&wc)) return false;

    int winHeight = SELECT_FIRST_BTN_Y
                  + g_gameProfileCount * SELECT_BTN_SPACING
                  + 20
                  + SELECT_EXIT_HEIGHT
                  + SELECT_BOTTOM_MARGIN + 40;

    g_hGameSelectWnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        L"CustomControlZSelectClass",
        L"CustomControlZ",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, SELECT_WIN_WIDTH, winHeight,
        nullptr, nullptr, hInstance, nullptr
    );
    return g_hGameSelectWnd != nullptr;
}

// --- MAIN TRAY WINDOW ---

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_TRAYICON:
        if (lParam == WM_LBUTTONDBLCLK) {
            if (g_activeProfile && g_hSettingsWnd) {
                ShowWindow(g_hSettingsWnd, SW_SHOW);
                SetForegroundWindow(g_hSettingsWnd);
            } else if (g_hGameSelectWnd) {
                ShowWindow(g_hGameSelectWnd, SW_SHOW);
                SetForegroundWindow(g_hGameSelectWnd);
            }
        } else if (lParam == WM_RBUTTONUP) {
            POINT curPoint;
            GetCursorPos(&curPoint);
            SetForegroundWindow(hwnd);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

            HMENU hMenu = CreateTrayMenu();
            if (hMenu) {
                UINT uFlags = TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_BOTTOMALIGN;
                uFlags |= (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;

                int cmd = TrackPopupMenuEx(hMenu, uFlags, curPoint.x, curPoint.y, hwnd, nullptr);
                DestroyMenu(hMenu);
                SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

                if (cmd == ID_TRAY_SETTINGS) {
                    if (g_activeProfile && g_hSettingsWnd) {
                        ShowWindow(g_hSettingsWnd, SW_SHOW);
                        SetForegroundWindow(g_hSettingsWnd);
                    } else if (g_hGameSelectWnd) {
                        ShowWindow(g_hGameSelectWnd, SW_SHOW);
                        SetForegroundWindow(g_hGameSelectWnd);
                    }
                } else if (cmd == ID_TRAY_CHANGE_GAME) {
                    ShowChangeGameUI();
                } else if (cmd == ID_TRAY_EXIT) {
                    DestroyWindow(hwnd);
                }

                PostMessage(hwnd, WM_NULL, 0, 0);
            }
        }
        break;

    case WM_DESTROY:
        StopGameLogicThread();
        if (g_hSettingsWnd)   { DestroyWindow(g_hSettingsWnd);   g_hSettingsWnd   = nullptr; }
        if (g_hGameSelectWnd) { DestroyWindow(g_hGameSelectWnd); g_hGameSelectWnd = nullptr; }
        Shell_NotifyIcon(NIM_DELETE, &g_nid);
        CleanupIcons();
        CleanupBrushes();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// --- STARTUP HELPERS ---

bool WaitForShellReady(int maxWaitSeconds = SHELL_TIMEOUT_SEC) {
    ULONGLONG startTime = GetTickCount64();
    ULONGLONG timeout   = static_cast<ULONGLONG>(maxWaitSeconds) * 1000ULL;
    while ((GetTickCount64() - startTime) < timeout) {
        if (FindWindow(L"Shell_TrayWnd", nullptr) != nullptr) return true;
        Sleep(SHELL_CHECK_INTERVAL_MS);
    }
    return false;
}

bool AddTrayIconWithRetry(PNOTIFYICONDATA pnid, int maxRetries = TRAY_MAX_RETRIES) {
    for (int i = 0; i < maxRetries; i++) {
        if (Shell_NotifyIcon(NIM_ADD, pnid)) return true;
        Sleep(TRAY_RETRY_INTERVAL_MS);
    }
    return false;
}

inline void SafeCloseMutex(HANDLE& hMutex) {
    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        hMutex = nullptr;
    }
}

// --- ENTRY POINT ---

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE,
    _In_ LPWSTR,
    _In_ int)
{
    EnableWindows11DarkMode();

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    g_hInstance = hInstance;

    WaitForShellReady();

    // Load icons
    g_hIconExe    = LoadIcon(hInstance, MAKEINTRESOURCE(ICON_ID_EXE));
    g_hIconIdle   = LoadIcon(hInstance, MAKEINTRESOURCE(ICON_ID_IDLE));
    g_hIconActive = LoadIcon(hInstance, MAKEINTRESOURCE(ICON_ID_ACTIVE));

    if (g_hIconIdle && g_hIconActive) {
        g_customIconsLoaded = true;
    } else {
        g_hIconIdle   = LoadIcon(nullptr, IDI_APPLICATION);
        g_hIconActive = LoadIcon(nullptr, IDI_SHIELD);
    }

    // Create game selection window (shown on launch)
    if (!CreateGameSelectionWindow(hInstance)) {
        SafeCloseMutex(hMutex);
        return -1;
    }

    // Create hidden tray window
    WNDCLASS wcTray = {};
    wcTray.lpfnWndProc   = WindowProc;
    wcTray.hInstance     = hInstance;
    wcTray.lpszClassName = L"CustomControlZTrayClass";
    wcTray.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(ICON_ID_EXE));
    if (!RegisterClass(&wcTray)) {
        SafeCloseMutex(hMutex);
        return -1;
    }

    g_hMainWindow = CreateWindowEx(0, L"CustomControlZTrayClass", L"CustomControlZ",
                                   0, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
    if (!g_hMainWindow) {
        SafeCloseMutex(hMutex);
        return -1;
    }

    // Setup tray icon
    g_nid.cbSize           = sizeof(NOTIFYICONDATA);
    g_nid.hWnd             = g_hMainWindow;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_hIconIdle;
    StringCchCopy(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"CustomControlZ");
    AddTrayIconWithRetry(&g_nid);

    // Show game selection window
    ShowWindow(g_hGameSelectWnd, SW_SHOW);
    SetForegroundWindow(g_hGameSelectWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_isAppRunning = false;
    StopGameLogicThread();
    SafeCloseMutex(hMutex);

    return static_cast<int>(msg.wParam);
}
