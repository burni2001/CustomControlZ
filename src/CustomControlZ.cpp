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

#ifndef APP_VERSION
#define APP_VERSION L"1.0"
#endif

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
#define BTN_BIND_BASE               2001  // Bind buttons: BTN_BIND_BASE + binding index
#define ID_LABEL_BASE               2200  // Label statics: ID_LABEL_BASE + binding index
#define ID_TIMING_SWITCH_BASE       2400  // Edit: switch delay (durationMs)  — 2400 + binding index
#define ID_TIMING_RETURN_BASE       2500  // Edit: return delay (returnDelayMs) — 2500 + binding index
#define BTN_OUTPUT_KEY_BASE         2600  // Output key bind buttons: 2600 + binding index
#define BTN_LONG_OUTPUT_KEY_BASE    2700  // Long-output key bind buttons: 2700 + binding index
#define BTN_CLEAR_BASE              2800  // Clear input-key buttons: 2800 + binding index
#define BTN_CLEAR_OUTPUT_BASE       2850  // Clear output-key buttons: 2850 + binding index
#define BTN_CLEAR_LONG_OUTPUT_BASE  2900  // Clear long-output-key buttons: 2900 + binding index
#define ID_OUTPUT_LABEL_BASE        3100  // Output key row labels: 3100 + binding index
#define ID_LONG_OUTPUT_LABEL_BASE   3200  // Long-output key row labels: 3200 + binding index
#define ID_TITLE_STATIC             2100
#define ID_IMPRINT1_STATIC          2101
#define ID_IMPRINT2_STATIC          2102
#define BTN_EXIT_SETTINGS           3001
#define BTN_EXIT_APP                3002
#define BTN_FONT_SETTINGS           3003
#define BTN_CHANGE_GAME             3004

// Game selection window control IDs
#define BTN_GAME_BASE        4000  // Game buttons: BTN_GAME_BASE + profile index
#define BTN_SELECT_MINIMIZE  4997
#define BTN_SELECT_CREDITS   4998
#define BTN_SELECT_EXIT      4999
#define ID_SELECT_TITLE      5000
#define ID_SELECT_SUBTITLE   5001

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

constexpr int LAYOUT_LEFT_MARGIN          = 42;
constexpr int LAYOUT_LABEL_WIDTH          = 348;
constexpr int LAYOUT_BUTTON_WIDTH         = 151;
constexpr int LAYOUT_BUTTON_GAP           = 8;
constexpr int LAYOUT_ROW_HEIGHT           = 50;
constexpr int LAYOUT_BUTTON_HEIGHT        = 34;
constexpr int LAYOUT_TITLE_START          = 43;
constexpr int LAYOUT_TITLE_HEIGHT         = 43;
constexpr int LAYOUT_TITLE_SPACING        = 79;  // includes room for legend below title
constexpr int LAYOUT_BOTTOM_BUTTON_HEIGHT = 42;
constexpr int LAYOUT_BOTTOM_BUTTON_WIDTH  = 168;
constexpr int LAYOUT_BOTTOM_BUTTON_GAP    = 24;
constexpr int LAYOUT_BOTTOM_SPACING       = 59;
constexpr int LAYOUT_IMPRINT_HEIGHT       = 19;
constexpr int LAYOUT_IMPRINT_SPACING      = 22;
constexpr int LAYOUT_ROW_PADDING          = 8;
constexpr int LAYOUT_LINE_WIDTH           = 516;
constexpr int WINDOW_WIDTH                = 600;
constexpr int WINDOW_HEIGHT               = 600;
constexpr int LAYOUT_FONT_BUTTON_WIDTH    = 101;
constexpr int LAYOUT_FONT_BUTTON_HEIGHT   = 29;
constexpr int LAYOUT_LEGEND_HEIGHT        = 17;  // height of app/in-game key legend below title
constexpr int LAYOUT_TIMING_ROW_HEIGHT    = 41;  // Height of each timing sub-row (ms delay inputs)
constexpr int LAYOUT_TIMING_ROWS_HEIGHT   = 82;  // Total height of both timing sub-rows (2 × 41)
constexpr int LAYOUT_TIMING_EDIT_WIDTH    = 90;  // Width of timing value edit boxes
constexpr int LAYOUT_OUTPUT_ROW_HEIGHT    = 42;  // Height of each output-key configurable sub-row
constexpr int LAYOUT_CLEAR_BUTTON_WIDTH   = 30;  // Width of the × clear button
constexpr int LAYOUT_CLEAR_BUTTON_GAP     = 5;   // Gap between bind button and its × clear button

// Game select window layout
constexpr int SELECT_TITLE_Y       = 17;
constexpr int SELECT_SUBTITLE_Y    = 62;
constexpr int SELECT_FIRST_BTN_Y   = 108;
constexpr int SELECT_BTN_MARGIN_X  = 34;
constexpr int SELECT_GAME_BTN_HEIGHT  = 72;  // Height of each game selection button
constexpr int SELECT_GAME_BTN_SPACING = 82;  // Spacing between game buttons (height + 10px gap)

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

    g_hFontTitle   = CreateUIFont(41, FW_BOLD);
    g_hFontNormal  = CreateUIFont(24);
    g_hFontButton  = CreateUIFont(20);
    g_hFontImprint = CreateUIFont(17);
}

void UpdateAllControlFonts(HWND hwnd) {
    SendMessage(GetDlgItem(hwnd, ID_TITLE_STATIC), WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

    if (g_activeProfile) {
        for (int i = 0; i < g_activeProfile->bindingCount; i++) {
            SendMessage(GetDlgItem(hwnd, ID_LABEL_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            InvalidateRect(GetDlgItem(hwnd, BTN_BIND_BASE + i), nullptr, TRUE);
            InvalidateRect(GetDlgItem(hwnd, BTN_CLEAR_BASE + i), nullptr, TRUE);
            if (g_activeProfile->bindings[i].behavior.type == BehaviorType::MeleeBurst) {
                SendMessage(GetDlgItem(hwnd, ID_TIMING_SWITCH_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
                SendMessage(GetDlgItem(hwnd, ID_TIMING_RETURN_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            }
            const auto& beh = g_activeProfile->bindings[i].behavior;
            if (beh.outputVkLabel) {
                SendMessage(GetDlgItem(hwnd, ID_OUTPUT_LABEL_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
                InvalidateRect(GetDlgItem(hwnd, BTN_OUTPUT_KEY_BASE + i), nullptr, TRUE);
            }
            if (beh.longOutputVkLabel) {
                SendMessage(GetDlgItem(hwnd, ID_LONG_OUTPUT_LABEL_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
                InvalidateRect(GetDlgItem(hwnd, BTN_LONG_OUTPUT_KEY_BASE + i), nullptr, TRUE);
            }
        }
    }

    InvalidateRect(GetDlgItem(hwnd, BTN_FONT_SETTINGS), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, BTN_CHANGE_GAME),   nullptr, TRUE);
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
    // × clear buttons: reddish border to signal destructive action
    if (ctlID >= BTN_CLEAR_BASE && ctlID < BTN_CLEAR_BASE + MAX_BINDINGS)
        return { g_hBrushButton, t.exitBorder, t.exitText, g_hFontButton };
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
        AppendMenu(hMenu, MF_STRING, ID_TRAY_CHANGE_GAME, L"Select Game");
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
    WORD vals[MAX_BINDINGS]          = {};
    int  switchMs[MAX_BINDINGS]      = {};
    int  returnMs[MAX_BINDINGS]      = {};
    WORD outputVk[MAX_BINDINGS]      = {};
    WORD longOutputVk[MAX_BINDINGS]  = {};
    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        StringCchCopy(localFontName, ARRAYSIZE(localFontName), g_fontName);
        for (int i = 0; i < profile->bindingCount; i++) {
            vals[i] = profile->bindings[i].currentVk;
            if (profile->bindings[i].behavior.type == BehaviorType::MeleeBurst) {
                switchMs[i] = profile->bindings[i].behavior.durationMs;
                returnMs[i] = profile->bindings[i].behavior.returnDelayMs;
            }
            if (profile->bindings[i].behavior.outputVkLabel)
                outputVk[i] = profile->bindings[i].behavior.outputVk;
            if (profile->bindings[i].behavior.longOutputVkLabel)
                longOutputVk[i] = profile->bindings[i].behavior.longOutputVk;
        }
    }

    wchar_t buf[CONFIG_BUFFER];
    wchar_t key[64];
    for (int i = 0; i < profile->bindingCount; i++) {
        StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", vals[i]);
        WritePrivateProfileString(profile->iniSection, profile->bindings[i].iniKey, buf, CONFIG_FILE);

        if (profile->bindings[i].behavior.type == BehaviorType::MeleeBurst) {
            StringCchPrintf(key, ARRAYSIZE(key), L"%s_SwitchMs", profile->bindings[i].iniKey);
            StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", switchMs[i]);
            WritePrivateProfileString(profile->iniSection, key, buf, CONFIG_FILE);

            StringCchPrintf(key, ARRAYSIZE(key), L"%s_ReturnMs", profile->bindings[i].iniKey);
            StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", returnMs[i]);
            WritePrivateProfileString(profile->iniSection, key, buf, CONFIG_FILE);
        }

        if (profile->bindings[i].behavior.outputVkLabel) {
            StringCchPrintf(key, ARRAYSIZE(key), L"%s_OutputKey", profile->bindings[i].iniKey);
            StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", outputVk[i]);
            WritePrivateProfileString(profile->iniSection, key, buf, CONFIG_FILE);
        }

        if (profile->bindings[i].behavior.longOutputVkLabel) {
            StringCchPrintf(key, ARRAYSIZE(key), L"%s_LongOutputKey", profile->bindings[i].iniKey);
            StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", longOutputVk[i]);
            WritePrivateProfileString(profile->iniSection, key, buf, CONFIG_FILE);
        }
    }
    WritePrivateProfileString(profile->iniSection, L"FontName", localFontName, CONFIG_FILE);
}

void LoadConfig(GameProfile* profile) {
    WORD vals[MAX_BINDINGS]         = {};
    int  switchMs[MAX_BINDINGS]     = {};
    int  returnMs[MAX_BINDINGS]     = {};
    WORD outputVk[MAX_BINDINGS]     = {};
    WORD longOutputVk[MAX_BINDINGS] = {};
    bool hasOutputVk[MAX_BINDINGS]     = {};
    bool hasLongOutputVk[MAX_BINDINGS] = {};
    wchar_t key[64];

    for (int i = 0; i < profile->bindingCount; i++) {
        vals[i] = static_cast<WORD>(
            GetPrivateProfileInt(profile->iniSection, profile->bindings[i].iniKey,
                                 profile->bindings[i].defaultVk, CONFIG_FILE));

        if (profile->bindings[i].behavior.type == BehaviorType::MeleeBurst) {
            StringCchPrintf(key, ARRAYSIZE(key), L"%s_SwitchMs", profile->bindings[i].iniKey);
            switchMs[i] = GetPrivateProfileInt(profile->iniSection, key,
                              profile->bindings[i].behavior.durationMs, CONFIG_FILE);

            StringCchPrintf(key, ARRAYSIZE(key), L"%s_ReturnMs", profile->bindings[i].iniKey);
            returnMs[i] = GetPrivateProfileInt(profile->iniSection, key,
                              profile->bindings[i].behavior.returnDelayMs, CONFIG_FILE);
        }

        if (profile->bindings[i].behavior.outputVkLabel) {
            StringCchPrintf(key, ARRAYSIZE(key), L"%s_OutputKey", profile->bindings[i].iniKey);
            // Use UINT_MAX as sentinel default to detect whether the key exists in the INI
            UINT raw = (UINT)GetPrivateProfileInt(profile->iniSection, key, -1, CONFIG_FILE);
            if (raw != (UINT)-1) { outputVk[i] = static_cast<WORD>(raw); hasOutputVk[i] = true; }
        }

        if (profile->bindings[i].behavior.longOutputVkLabel) {
            StringCchPrintf(key, ARRAYSIZE(key), L"%s_LongOutputKey", profile->bindings[i].iniKey);
            UINT raw = (UINT)GetPrivateProfileInt(profile->iniSection, key, -1, CONFIG_FILE);
            if (raw != (UINT)-1) { longOutputVk[i] = static_cast<WORD>(raw); hasLongOutputVk[i] = true; }
        }
    }

    wchar_t tempFont[FONT_NAME_BUFFER];
    GetPrivateProfileString(profile->iniSection, L"FontName", L"Palatino Linotype",
                            tempFont, ARRAYSIZE(tempFont), CONFIG_FILE);

    std::lock_guard<std::mutex> lock(g_configMutex);
    for (int i = 0; i < profile->bindingCount; i++) {
        // Allow currentVk=0 (means "no key bound / disabled")
        profile->bindings[i].currentVk = vals[i];
        if (profile->bindings[i].behavior.type == BehaviorType::MeleeBurst) {
            profile->bindings[i].behavior.durationMs    = max(50, min(5000, switchMs[i]));
            profile->bindings[i].behavior.returnDelayMs = max(50, min(5000, returnMs[i]));
        }
        if (hasOutputVk[i])     profile->bindings[i].behavior.outputVk     = outputVk[i];
        if (hasLongOutputVk[i]) profile->bindings[i].behavior.longOutputVk = longOutputVk[i];
    }
    StringCchCopy(g_fontName, ARRAYSIZE(g_fontName), tempFont);
}

// --- KEY NAMING ---

void GetKeyName(WORD vk, wchar_t* buffer, size_t size) {
    switch (vk) {
    case 0:          StringCchCopy(buffer, size, L"\u2014");       return;  // em dash: no key bound
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

void ShowChangeGameUI(); // Forward declaration

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

        // Top buttons: Select Game (left) | Font (right corner)
        CreateWindow(L"BUTTON", L"Select Game",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            10, 10, LAYOUT_FONT_BUTTON_WIDTH, LAYOUT_FONT_BUTTON_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_CHANGE_GAME, nullptr, nullptr);

        CreateWindow(L"BUTTON", L"Font",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            WINDOW_WIDTH - LAYOUT_FONT_BUTTON_WIDTH - 10, 10, LAYOUT_FONT_BUTTON_WIDTH, LAYOUT_FONT_BUTTON_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_FONT_SETTINGS, nullptr, nullptr);

        // Title
        HWND hTitle = CreateWindow(L"STATIC", L"Key Bindings",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, LAYOUT_TITLE_START, WINDOW_WIDTH, LAYOUT_TITLE_HEIGHT,
            hwnd, (HMENU)(INT_PTR)ID_TITLE_STATIC, nullptr, nullptr);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

        // Binding rows (one per profile binding)
        int rowY = rowBaseY;
        const int clearX = buttonX + LAYOUT_BUTTON_WIDTH + LAYOUT_CLEAR_BUTTON_GAP;
        for (int i = 0; i < profile->bindingCount; i++) {
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

            // × clear button to unset this input binding
            CreateWindow(L"BUTTON", L"\u00d7",
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                clearX, labelY, LAYOUT_CLEAR_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                hwnd, (HMENU)(INT_PTR)(BTN_CLEAR_BASE + i), nullptr, nullptr);

            rowY += LAYOUT_ROW_HEIGHT;

            // Timing sub-rows for MeleeBurst bindings
            if (profile->bindings[i].behavior.type == BehaviorType::MeleeBurst) {
                const BehaviorDescriptor& desc = profile->bindings[i].behavior;
                wchar_t buf[16];
                const int editH = LAYOUT_BUTTON_HEIGHT;

                // Switch delay row
                HWND hL1 = CreateWindow(L"STATIC", L"First swing delay (ms):",
                    WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                    LAYOUT_LEFT_MARGIN + 20, rowY + 5, LAYOUT_LABEL_WIDTH - 20, editH,
                    hwnd, nullptr, nullptr, nullptr);
                SendMessage(hL1, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", desc.durationMs);
                HWND hE1 = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", buf,
                    WS_VISIBLE | WS_CHILD | ES_NUMBER | ES_CENTER,
                    buttonX, rowY + 5, LAYOUT_TIMING_EDIT_WIDTH, editH,
                    hwnd, (HMENU)(INT_PTR)(ID_TIMING_SWITCH_BASE + i), nullptr, nullptr);
                SendMessage(hE1, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                rowY += LAYOUT_TIMING_ROW_HEIGHT;

                // Return delay row
                HWND hL2 = CreateWindow(L"STATIC", L"Return to main weapon delay (ms):",
                    WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                    LAYOUT_LEFT_MARGIN + 20, rowY + 5, LAYOUT_LABEL_WIDTH - 20, editH,
                    hwnd, nullptr, nullptr, nullptr);
                SendMessage(hL2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", desc.returnDelayMs);
                HWND hE2 = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", buf,
                    WS_VISIBLE | WS_CHILD | ES_NUMBER | ES_CENTER,
                    buttonX, rowY + 5, LAYOUT_TIMING_EDIT_WIDTH, editH,
                    hwnd, (HMENU)(INT_PTR)(ID_TIMING_RETURN_BASE + i), nullptr, nullptr);
                SendMessage(hE2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                rowY += LAYOUT_TIMING_ROW_HEIGHT;
            }

            // Configurable output-key sub-rows
            const BehaviorDescriptor& beh = profile->bindings[i].behavior;
            const int subLabelY = (LAYOUT_OUTPUT_ROW_HEIGHT - LAYOUT_BUTTON_HEIGHT) / 2;

            if (beh.outputVkLabel) {
                HWND hOL = CreateWindow(L"STATIC", beh.outputVkLabel,
                    WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                    LAYOUT_LEFT_MARGIN + 20, rowY + subLabelY, LAYOUT_LABEL_WIDTH - 20, LAYOUT_BUTTON_HEIGHT,
                    hwnd, (HMENU)(INT_PTR)(ID_OUTPUT_LABEL_BASE + i), nullptr, nullptr);
                SendMessage(hOL, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                CreateWindow(L"BUTTON", L"...",
                    WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                    buttonX, rowY + subLabelY, LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                    hwnd, (HMENU)(INT_PTR)(BTN_OUTPUT_KEY_BASE + i), nullptr, nullptr);

                rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
            }

            if (beh.longOutputVkLabel) {
                HWND hLOL = CreateWindow(L"STATIC", beh.longOutputVkLabel,
                    WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                    LAYOUT_LEFT_MARGIN + 20, rowY + subLabelY, LAYOUT_LABEL_WIDTH - 20, LAYOUT_BUTTON_HEIGHT,
                    hwnd, (HMENU)(INT_PTR)(ID_LONG_OUTPUT_LABEL_BASE + i), nullptr, nullptr);
                SendMessage(hLOL, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                CreateWindow(L"BUTTON", L"...",
                    WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                    buttonX, rowY + subLabelY, LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                    hwnd, (HMENU)(INT_PTR)(BTN_LONG_OUTPUT_KEY_BASE + i), nullptr, nullptr);

                rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
            }
        }

        // Bottom buttons: Minimize | Exit (2-button row, centered) — fixed position
        int bottomY    = WINDOW_HEIGHT - LAYOUT_BOTTOM_SPACING;
        int btn2StartX = (WINDOW_WIDTH - LAYOUT_BOTTOM_BUTTON_WIDTH * 2 - LAYOUT_BOTTOM_BUTTON_GAP) / 2;

        CreateWindow(L"BUTTON", L"Minimize",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            btn2StartX, bottomY, LAYOUT_BOTTOM_BUTTON_WIDTH, LAYOUT_BOTTOM_BUTTON_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_EXIT_SETTINGS, nullptr, nullptr);

        CreateWindow(L"BUTTON", L"Exit",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            btn2StartX + LAYOUT_BOTTOM_BUTTON_WIDTH + LAYOUT_BOTTOM_BUTTON_GAP, bottomY,
            LAYOUT_BOTTOM_BUTTON_WIDTH, LAYOUT_BOTTOM_BUTTON_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_EXIT_APP, nullptr, nullptr);

        // Set initial button texts
        std::lock_guard<std::mutex> lock(g_configMutex);
        for (int i = 0; i < profile->bindingCount; i++) {
            UpdateButtonText(GetDlgItem(hwnd, BTN_BIND_BASE + i), profile->bindings[i].currentVk);
            if (profile->bindings[i].behavior.outputVkLabel)
                UpdateButtonText(GetDlgItem(hwnd, BTN_OUTPUT_KEY_BASE + i), profile->bindings[i].behavior.outputVk);
            if (profile->bindings[i].behavior.longOutputVkLabel)
                UpdateButtonText(GetDlgItem(hwnd, BTN_LONG_OUTPUT_KEY_BASE + i), profile->bindings[i].behavior.longOutputVk);
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
            // Legend: colored squares showing what each stripe color means
            int legendY = LAYOUT_TITLE_START + LAYOUT_TITLE_HEIGHT + 3;
            HFONT hOldFont = (HFONT)SelectObject(hdc, g_hFontImprint);
            SetTextColor(hdc, g_activeProfile->theme.text);
            SetBkMode(hdc, TRANSPARENT);

            HBRUSH hBrInGame = CreateSolidBrush(g_activeProfile->theme.accent);
            RECT rIG = { LAYOUT_LEFT_MARGIN, legendY + 2, LAYOUT_LEFT_MARGIN + 12, legendY + 14 };
            FillRect(hdc, &rIG, hBrInGame);
            DeleteObject(hBrInGame);
            RECT tIG = { LAYOUT_LEFT_MARGIN + 16, legendY, LAYOUT_LEFT_MARGIN + 168, legendY + LAYOUT_LEGEND_HEIGHT };
            DrawText(hdc, L"In-Game Key", -1, &tIG, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            HBRUSH hBrApp = CreateSolidBrush(RGB(70, 130, 200));
            RECT rAP = { LAYOUT_LEFT_MARGIN + 182, legendY + 2, LAYOUT_LEFT_MARGIN + 194, legendY + 14 };
            FillRect(hdc, &rAP, hBrApp);
            DeleteObject(hBrApp);
            RECT tAP = { LAYOUT_LEFT_MARGIN + 198, legendY, LAYOUT_LEFT_MARGIN + 408, legendY + LAYOUT_LEGEND_HEIGHT };
            DrawText(hdc, L"App Key (custom)", -1, &tAP, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hOldFont);

            // Binding rows
            HBRUSH hRowBrush = CreateSolidBrush(g_activeProfile->theme.rowBg);
            HPEN   hLinePen  = CreatePen(PS_SOLID, 2, g_activeProfile->theme.separator);
            HPEN   hOldPen   = (HPEN)SelectObject(hdc, hLinePen);

            int rowBaseY = LAYOUT_TITLE_START + LAYOUT_TITLE_SPACING;
            int rowY = rowBaseY;
            for (int i = 0; i < g_activeProfile->bindingCount; i++) {
                // Separator line
                MoveToEx(hdc, LAYOUT_LEFT_MARGIN, rowY, nullptr);
                LineTo(hdc, LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH, rowY);

                // Row background highlight
                RECT rowRect = {
                    LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING,
                    rowY + 6,
                    LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH + LAYOUT_ROW_PADDING,
                    rowY + 6 + LAYOUT_BUTTON_HEIGHT
                };
                FillRect(hdc, &rowRect, hRowBrush);

                // Colored left stripe: accent = in-game, blue = app-only
                COLORREF stripeColor = g_activeProfile->bindings[i].isAppOnly
                    ? RGB(70, 130, 200)
                    : g_activeProfile->theme.accent;
                HBRUSH hStripe = CreateSolidBrush(stripeColor);
                RECT stripeRect = {
                    LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING,
                    rowY + 6,
                    LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING + 5,
                    rowY + 6 + LAYOUT_BUTTON_HEIGHT
                };
                FillRect(hdc, &stripeRect, hStripe);
                DeleteObject(hStripe);

                rowY += LAYOUT_ROW_HEIGHT;
                const auto& beh = g_activeProfile->bindings[i].behavior;
                if (beh.type == BehaviorType::MeleeBurst)
                    rowY += LAYOUT_TIMING_ROWS_HEIGHT;
                if (beh.outputVkLabel)     rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
                if (beh.longOutputVkLabel) rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
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

    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        if (g_activeProfile) {
            SetTextColor(hdcEdit, g_activeProfile->theme.text);
            SetBkColor(hdcEdit, g_activeProfile->theme.button);
        }
        return (LRESULT)g_hBrushButton;
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
        if (id == BTN_CHANGE_GAME) {
            ShowChangeGameUI();
        } else if (id == BTN_EXIT_SETTINGS) {
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
        } else if (id >= BTN_OUTPUT_KEY_BASE && g_activeProfile &&
                   id < BTN_OUTPUT_KEY_BASE + g_activeProfile->bindingCount) {
            g_waitingForBindID = id;
            SetWindowText(GetDlgItem(hwnd, id), L"...");
            SetFocus(hwnd);
        } else if (id >= BTN_LONG_OUTPUT_KEY_BASE && g_activeProfile &&
                   id < BTN_LONG_OUTPUT_KEY_BASE + g_activeProfile->bindingCount) {
            g_waitingForBindID = id;
            SetWindowText(GetDlgItem(hwnd, id), L"...");
            SetFocus(hwnd);
        } else if (id >= BTN_CLEAR_BASE && g_activeProfile &&
                   id < BTN_CLEAR_BASE + g_activeProfile->bindingCount) {
            int idx = id - BTN_CLEAR_BASE;
            {
                std::lock_guard<std::mutex> lock(g_configMutex);
                g_activeProfile->bindings[idx].currentVk = 0;
            }
            UpdateButtonText(GetDlgItem(hwnd, BTN_BIND_BASE + idx), 0);
            SaveConfig(g_activeProfile);
        } else if (HIWORD(wParam) == EN_KILLFOCUS && g_activeProfile) {
            // Timing edit lost focus — validate and save
            int idx      = -1;
            bool isSwitch = false;
            if (id >= ID_TIMING_SWITCH_BASE && id < ID_TIMING_SWITCH_BASE + MAX_BINDINGS) {
                idx = id - ID_TIMING_SWITCH_BASE;  isSwitch = true;
            } else if (id >= ID_TIMING_RETURN_BASE && id < ID_TIMING_RETURN_BASE + MAX_BINDINGS) {
                idx = id - ID_TIMING_RETURN_BASE;  isSwitch = false;
            }
            if (idx >= 0 && idx < g_activeProfile->bindingCount
                && g_activeProfile->bindings[idx].behavior.type == BehaviorType::MeleeBurst) {
                wchar_t buf[16];
                GetWindowText(GetDlgItem(hwnd, id), buf, ARRAYSIZE(buf));
                int val = max(50, min(5000, _wtoi(buf)));
                // Write clamped value back to edit box
                StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", val);
                SetWindowText(GetDlgItem(hwnd, id), buf);
                {
                    std::lock_guard<std::mutex> lock(g_configMutex);
                    if (isSwitch) g_activeProfile->bindings[idx].behavior.durationMs    = val;
                    else          g_activeProfile->bindings[idx].behavior.returnDelayMs = val;
                }
                SaveConfig(g_activeProfile);
            }
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

            int idx = -1;
            if (bindID >= BTN_BIND_BASE && bindID < BTN_BIND_BASE + g_activeProfile->bindingCount) {
                idx = bindID - BTN_BIND_BASE;
                std::lock_guard<std::mutex> lock(g_configMutex);
                g_activeProfile->bindings[idx].currentVk = newKey;
                UpdateButtonText(GetDlgItem(hwnd, bindID), newKey);
            } else if (bindID >= BTN_OUTPUT_KEY_BASE && bindID < BTN_OUTPUT_KEY_BASE + g_activeProfile->bindingCount) {
                idx = bindID - BTN_OUTPUT_KEY_BASE;
                std::lock_guard<std::mutex> lock(g_configMutex);
                g_activeProfile->bindings[idx].behavior.outputVk = newKey;
                UpdateButtonText(GetDlgItem(hwnd, bindID), newKey);
            } else if (bindID >= BTN_LONG_OUTPUT_KEY_BASE && bindID < BTN_LONG_OUTPUT_KEY_BASE + g_activeProfile->bindingCount) {
                idx = bindID - BTN_LONG_OUTPUT_KEY_BASE;
                std::lock_guard<std::mutex> lock(g_configMutex);
                g_activeProfile->bindings[idx].behavior.longOutputVk = newKey;
                UpdateButtonText(GetDlgItem(hwnd, bindID), newKey);
            }

            g_waitingForBindID = 0;
            if (idx >= 0) SaveConfig(g_activeProfile);
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

    // Size the window so the CLIENT area is exactly WINDOW_WIDTH x WINDOW_HEIGHT
    RECT rcAdj = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRectEx(&rcAdj,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        FALSE, WS_EX_DLGMODALFRAME);

    g_hSettingsWnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        L"CustomControlZSettingsClass",
        profile->settingsTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rcAdj.right - rcAdj.left, rcAdj.bottom - rcAdj.top,
        nullptr, nullptr, hInstance, nullptr
    );
    return g_hSettingsWnd != nullptr;
}

// --- GAME SELECTION WINDOW ---

void OnGameSelected(int profileIndex) {
    StopGameLogicThread();

    // Capture position before hiding so settings window opens in same spot
    RECT selectRect = {};
    if (g_hGameSelectWnd) GetWindowRect(g_hGameSelectWnd, &selectRect);

    g_activeProfile = g_gameProfiles[profileIndex];
    LoadConfig(g_activeProfile);
    RebuildThemeBrushes(g_activeProfile);

    if (g_hSettingsWnd) {
        DestroyWindow(g_hSettingsWnd);
        g_hSettingsWnd = nullptr;
    }

    if (!CreateSettingsWindow(g_hInstance, g_activeProfile)) return;

    // Place settings window at same screen position as game select window
    if (selectRect.left != 0 || selectRect.top != 0) {
        SetWindowPos(g_hSettingsWnd, nullptr,
                     selectRect.left, selectRect.top, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER);
    }

    ShowWindow(g_hGameSelectWnd, SW_HIDE);
    ShowWindow(g_hSettingsWnd, SW_SHOW);
    SetForegroundWindow(g_hSettingsWnd);

    StartGameLogicThread(g_activeProfile);
}

void ShowChangeGameUI() {
    StopGameLogicThread();

    // Capture position before hiding so game select opens in same spot
    RECT settingsRect = {};
    if (g_hSettingsWnd) {
        GetWindowRect(g_hSettingsWnd, &settingsRect);
        ShowWindow(g_hSettingsWnd, SW_HIDE);
    }

    if (g_hGameSelectWnd) {
        if (settingsRect.left != 0 || settingsRect.top != 0) {
            SetWindowPos(g_hGameSelectWnd, nullptr,
                         settingsRect.left, settingsRect.top, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);
        }
        ShowWindow(g_hGameSelectWnd, SW_SHOW);
        SetForegroundWindow(g_hGameSelectWnd);
    }
}

LRESULT CALLBACK GameSelectProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        EnableDarkTitleBar(hwnd);

        HFONT hFontTitle   = CreateFont(29, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hFontGameBtn = CreateFont(24, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hFontNormal  = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        // Store fonts in GWLP_USERDATA for cleanup: [0]=title, [1]=game button bold, [2]=normal
        HFONT* fonts = new HFONT[3]{ hFontTitle, hFontGameBtn, hFontNormal };
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)fonts);

        HWND hTitle = CreateWindow(L"STATIC", L"CustomControlZ",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, SELECT_TITLE_Y, WINDOW_WIDTH, 50, hwnd,
            (HMENU)(INT_PTR)ID_SELECT_TITLE, nullptr, nullptr);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

        HWND hSub = CreateWindow(L"STATIC", L"Choose your game:",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, SELECT_SUBTITLE_Y, WINDOW_WIDTH, 34, hwnd,
            (HMENU)(INT_PTR)ID_SELECT_SUBTITLE, nullptr, nullptr);
        SendMessage(hSub, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // Game selection buttons — bold text, themed
        for (int i = 0; i < g_gameProfileCount; i++) {
            int btnY = SELECT_FIRST_BTN_Y + i * SELECT_GAME_BTN_SPACING;
            CreateWindow(L"BUTTON", g_gameProfiles[i]->displayName,
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                SELECT_BTN_MARGIN_X, btnY,
                WINDOW_WIDTH - 2 * SELECT_BTN_MARGIN_X, SELECT_GAME_BTN_HEIGHT,
                hwnd, (HMENU)(INT_PTR)(BTN_GAME_BASE + i), nullptr, nullptr);
        }

        // Bottom buttons at fixed position (same as settings window)
        int bottomY    = WINDOW_HEIGHT - LAYOUT_BOTTOM_SPACING;
        int btn2StartX = (WINDOW_WIDTH - LAYOUT_BOTTOM_BUTTON_WIDTH * 2 - LAYOUT_BOTTOM_BUTTON_GAP) / 2;

        // Credits button (bottom-left)
        CreateWindow(L"BUTTON", L"Credits",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            10, bottomY, LAYOUT_FONT_BUTTON_WIDTH, LAYOUT_BOTTOM_BUTTON_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_SELECT_CREDITS, nullptr, nullptr);

        // Minimize button
        CreateWindow(L"BUTTON", L"Minimize",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            btn2StartX, bottomY, LAYOUT_BOTTOM_BUTTON_WIDTH, LAYOUT_BOTTOM_BUTTON_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_SELECT_MINIMIZE, nullptr, nullptr);

        // Exit button
        CreateWindow(L"BUTTON", L"Exit",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            btn2StartX + LAYOUT_BOTTOM_BUTTON_WIDTH + LAYOUT_BOTTOM_BUTTON_GAP, bottomY,
            LAYOUT_BOTTOM_BUTTON_WIDTH, LAYOUT_BOTTOM_BUTTON_HEIGHT,
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

        HDC  hdc = pDIS->hDC;
        RECT rc  = pDIS->rcItem;
        HFONT* fonts = (HFONT*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

        if (pDIS->CtlID == BTN_SELECT_EXIT) {
            // Exit: dark red
            HBRUSH hBr = CreateSolidBrush(RGB(140, 30, 30));
            FillRect(hdc, &rc, hBr);
            DeleteObject(hBr);
            HPEN hPen    = CreatePen(PS_SOLID, 2, RGB(200, 70, 70));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, hOldPen); SelectObject(hdc, hOldBr);
            DeleteObject(hPen);
            if (fonts && fonts[2]) SelectObject(hdc, fonts[2]);
            SetTextColor(hdc, RGB(255, 210, 210));
            SetBkMode(hdc, TRANSPARENT);
            DrawText(hdc, L"Exit", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        } else if (pDIS->CtlID == BTN_SELECT_MINIMIZE || pDIS->CtlID == BTN_SELECT_CREDITS) {
            // Minimize / Credits: neutral dark
            HBRUSH hBr = CreateSolidBrush(RGB(40, 40, 40));
            FillRect(hdc, &rc, hBr);
            DeleteObject(hBr);
            HPEN hPen    = CreatePen(PS_SOLID, 2, RGB(100, 100, 100));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, hOldPen); SelectObject(hdc, hOldBr);
            DeleteObject(hPen);
            if (fonts && fonts[2]) SelectObject(hdc, fonts[2]);
            SetTextColor(hdc, RGB(220, 220, 220));
            SetBkMode(hdc, TRANSPARENT);
            wchar_t text[64];
            GetWindowText(pDIS->hwndItem, text, ARRAYSIZE(text));
            DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        } else {
            // Game button: themed background + bold game name
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
                if (fonts && fonts[1]) SelectObject(hdc, fonts[1]);
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
        } else if (id == BTN_SELECT_MINIMIZE) {
            ShowWindow(hwnd, SW_HIDE);
        } else if (id == BTN_SELECT_CREDITS) {
            MessageBox(hwnd,
                L"CustomControlZ v" APP_VERSION L"\n\n"
                L"Idea and development: B\u00f6rni (burni2001)\n\n"
                L"Development tools:\n"
                L"Claude Code, Visual Studio Code, GitHub Copilot",
                L"Credits \u2014 v" APP_VERSION, MB_OK | MB_ICONINFORMATION);
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

    RECT rcAdj = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRectEx(&rcAdj,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        FALSE, WS_EX_DLGMODALFRAME);

    g_hGameSelectWnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        L"CustomControlZSelectClass",
        L"CustomControlZ",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rcAdj.right - rcAdj.left, rcAdj.bottom - rcAdj.top,
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
