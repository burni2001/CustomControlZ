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
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
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
#define ID_TRAY_AUTOSTART   1004
#define ID_TRAY_TOOLTIPS    1005
#define ID_TRAY_GAME_BASE   6000  // Game quick-select items: ID_TRAY_GAME_BASE + profile index

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
#define ID_RETURN_WEAPON_BASE       2950  // Combobox: return weapon selection — 2950 + binding index
#define ID_OUTPUT_LABEL_BASE            3100  // Output key row labels: 3100 + binding index
#define ID_LONG_OUTPUT_LABEL_BASE       3200  // Long-output key row labels: 3200 + binding index
#define BTN_TERTIARY_OUTPUT_KEY_BASE    3500  // Tertiary output key bind buttons: 3500 + binding index
#define BTN_CLEAR_TERTIARY_OUTPUT_BASE  3550  // Clear tertiary output key buttons: 3550 + binding index
#define ID_INCLUDE_TERTIARY_BASE        3600  // Combobox: include tertiary in quickswitch cycle: 3600 + binding index
#define ID_TERTIARY_OUTPUT_LABEL_BASE   3700  // Tertiary output key row labels: 3700 + binding index
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

constexpr int LAYOUT_LEFT_MARGIN          = 46;
constexpr int LAYOUT_LABEL_WIDTH          = 383;
constexpr int LAYOUT_BUTTON_WIDTH         = 166;
constexpr int LAYOUT_BUTTON_GAP           = 9;
constexpr int LAYOUT_ROW_HEIGHT           = 55;
constexpr int LAYOUT_BUTTON_HEIGHT        = 37;
constexpr int LAYOUT_TITLE_START          = 47;
constexpr int LAYOUT_TITLE_HEIGHT         = 47;
constexpr int LAYOUT_TITLE_SPACING        = 107; // includes room for legend and info text below title
constexpr int LAYOUT_BOTTOM_BUTTON_HEIGHT = 46;
constexpr int LAYOUT_BOTTOM_BUTTON_WIDTH  = 185;
constexpr int LAYOUT_BOTTOM_BUTTON_GAP    = 26;
constexpr int LAYOUT_BOTTOM_SPACING       = 65;
constexpr int LAYOUT_IMPRINT_HEIGHT       = 21;
constexpr int LAYOUT_IMPRINT_SPACING      = 24;
constexpr int LAYOUT_ROW_PADDING          = 9;
constexpr int LAYOUT_LINE_WIDTH           = 568;
constexpr int WINDOW_WIDTH                = 660;
constexpr int WINDOW_HEIGHT               = 660;
constexpr int LAYOUT_FONT_BUTTON_WIDTH    = 111;
constexpr int LAYOUT_FONT_BUTTON_HEIGHT   = 32;
constexpr int LAYOUT_LEGEND_HEIGHT        = 19;  // height of app/in-game key legend below title
constexpr int LAYOUT_TIMING_ROW_HEIGHT    = 45;  // Height of each timing sub-row (ms delay inputs)
constexpr int LAYOUT_TIMING_ROWS_HEIGHT   = 90;  // Total height of both timing sub-rows (2 × 45)
constexpr int LAYOUT_TIMING_EDIT_WIDTH    = 99;  // Width of timing value edit boxes
constexpr int LAYOUT_OUTPUT_ROW_HEIGHT    = 46;  // Height of each output-key configurable sub-row
constexpr int LAYOUT_CLEAR_BUTTON_WIDTH   = 33;  // Width of the × clear button
constexpr int LAYOUT_CLEAR_BUTTON_GAP     = 6;   // Gap between bind button and its × clear button
constexpr int LAYOUT_SEPARATOR_PADDING    = 11;  // Extra vertical space above and below a separator line
constexpr int LAYOUT_LABEL_INDENT         = 10;  // Left indent on key-row labels to clear the triangle indicator

// Game select window layout
constexpr int SELECT_TITLE_Y       = 19;
constexpr int SELECT_SUBTITLE_Y    = 68;
constexpr int SELECT_FIRST_BTN_Y   = 119;
constexpr int SELECT_BTN_MARGIN_X  = 37;
constexpr int SELECT_GAME_BTN_HEIGHT  = 79;  // Height of each game selection button
constexpr int SELECT_GAME_BTN_SPACING = 90;  // Spacing between game buttons (height + 11px gap)

// --- GAME PROFILE ARCHITECTURE ---

#include "GameProfiles.h"

// --- GLOBAL STATE ---

wchar_t CONFIG_FILE[MAX_PATH] = {};
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
bool  g_tooltipsEnabled  = true;

HWND    g_hSettingsTooltip = nullptr;
HWND    g_hSelectTooltip   = nullptr;
WNDPROC g_origButtonProc   = nullptr;

GameProfile* g_activeProfile = nullptr;

// Forward declarations — defined after game headers are included (~line 788)
extern GameProfile* g_gameProfiles[];
extern const int    g_gameProfileCount;

HBRUSH g_hBrushBg     = nullptr;
HBRUSH g_hBrushButton = nullptr;
HBRUSH g_hBrushExit   = nullptr;
HFONT  g_hFontTitle   = nullptr;
HFONT  g_hFontNormal  = nullptr;
HFONT  g_hFontButton  = nullptr;
HFONT  g_hFontImprint = nullptr;

std::thread       g_logicThread;
std::atomic<bool> g_logicRunning(false);
int               g_settingsScrollY = 0;  // vertical scroll offset for the settings window

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

    g_hFontTitle   = CreateUIFont(45, FW_BOLD);
    g_hFontNormal  = CreateUIFont(26);
    g_hFontButton  = CreateUIFont(22);
    g_hFontImprint = CreateUIFont(19);
}

void UpdateAllControlFonts(HWND hwnd) {
    SendMessage(GetDlgItem(hwnd, ID_TITLE_STATIC), WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

    if (g_activeProfile) {
        for (int i = 0; i < g_activeProfile->bindingCount; i++) {
            SendMessage(GetDlgItem(hwnd, ID_LABEL_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            InvalidateRect(GetDlgItem(hwnd, BTN_BIND_BASE + i), nullptr, TRUE);
            InvalidateRect(GetDlgItem(hwnd, BTN_CLEAR_BASE + i), nullptr, TRUE);
            InvalidateRect(GetDlgItem(hwnd, BTN_CLEAR_OUTPUT_BASE + i), nullptr, TRUE);
            InvalidateRect(GetDlgItem(hwnd, BTN_CLEAR_LONG_OUTPUT_BASE + i), nullptr, TRUE);
            if (g_activeProfile->bindings[i].behavior.type == BehaviorType::MeleeBurst) {
                SendMessage(GetDlgItem(hwnd, ID_TIMING_SWITCH_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
                SendMessage(GetDlgItem(hwnd, ID_TIMING_RETURN_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
                if (g_activeProfile->bindings[i].behavior.returnAltVk)
                    SendMessage(GetDlgItem(hwnd, ID_RETURN_WEAPON_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
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
            if (beh.tertiaryOutputVkLabel) {
                SendMessage(GetDlgItem(hwnd, ID_TERTIARY_OUTPUT_LABEL_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
                InvalidateRect(GetDlgItem(hwnd, BTN_TERTIARY_OUTPUT_KEY_BASE + i), nullptr, TRUE);
                SendMessage(GetDlgItem(hwnd, ID_INCLUDE_TERTIARY_BASE + i), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            }
        }
    }

    InvalidateRect(GetDlgItem(hwnd, BTN_FONT_SETTINGS), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, BTN_CHANGE_GAME),   nullptr, TRUE);
    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
}

struct ButtonStyle {
    COLORREF fillColor;   // base fill colour used for hover computation
    HBRUSH   brush;
    COLORREF borderColor;
    COLORREF textColor;
    HFONT    font;
};

ButtonStyle GetButtonStyle(UINT ctlID) {
    if (!g_activeProfile) return { RGB(40,40,40), g_hBrushButton, RGB(100,100,100), RGB(200,200,200), g_hFontButton };
    const Theme& t = g_activeProfile->theme;
    if (ctlID == BTN_EXIT_APP)      return { t.exitFill, g_hBrushExit,   t.exitBorder,     t.exitText, g_hFontNormal };
    if (ctlID == BTN_EXIT_SETTINGS) return { t.button,   g_hBrushButton, t.minimizeBorder, t.text,     g_hFontNormal };
    if (ctlID == BTN_FONT_SETTINGS) return { t.button,   g_hBrushButton, t.border,         t.accent,   g_hFontButton };
    // × clear buttons: reddish border to signal destructive action
    if ((ctlID >= BTN_CLEAR_BASE                    && ctlID < BTN_CLEAR_BASE                    + MAX_BINDINGS) ||
        (ctlID >= BTN_CLEAR_OUTPUT_BASE             && ctlID < BTN_CLEAR_OUTPUT_BASE             + MAX_BINDINGS) ||
        (ctlID >= BTN_CLEAR_LONG_OUTPUT_BASE        && ctlID < BTN_CLEAR_LONG_OUTPUT_BASE        + MAX_BINDINGS) ||
        (ctlID >= BTN_CLEAR_TERTIARY_OUTPUT_BASE    && ctlID < BTN_CLEAR_TERTIARY_OUTPUT_BASE    + MAX_BINDINGS))
        return { t.button, g_hBrushButton, t.exitBorder, t.exitText, g_hFontButton };
    return { t.button, g_hBrushButton, t.border, t.accent, g_hFontButton };
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

// --- AUTOSTART ---

static const wchar_t* AUTOSTART_KEY  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* AUTOSTART_NAME = L"CustomControlZ";

bool IsAutostartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    wchar_t value[MAX_PATH] = {};
    DWORD size = sizeof(value);
    DWORD type = REG_SZ;
    bool enabled = (RegQueryValueExW(hKey, AUTOSTART_NAME, nullptr, &type,
                        (LPBYTE)value, &size) == ERROR_SUCCESS)
                   && (_wcsicmp(value, exePath) == 0);
    RegCloseKey(hKey);
    return enabled;
}

void SetAutostart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return;

    if (enable) {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        RegSetValueExW(hKey, AUTOSTART_NAME, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(exePath),
            static_cast<DWORD>((wcslen(exePath) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, AUTOSTART_NAME);
    }
    RegCloseKey(hKey);
}

// --- TRAY MENU ---

HMENU CreateTrayMenu() {
    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
        // Quick game selection
        for (int i = 0; i < g_gameProfileCount; i++) {
            UINT flags = MF_STRING;
            if (g_activeProfile == g_gameProfiles[i]) flags |= MF_CHECKED;
            AppendMenu(hMenu, flags, ID_TRAY_GAME_BASE + i, g_gameProfiles[i]->displayName);
        }
        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
        if (g_activeProfile && g_hSettingsWnd)
            AppendMenu(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Settings");
        AppendMenu(hMenu, MF_STRING, ID_TRAY_CHANGE_GAME, L"Select Game");
        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(hMenu, MF_STRING | (IsAutostartEnabled() ? MF_CHECKED : MF_UNCHECKED),
                   ID_TRAY_AUTOSTART, L"Start with Windows");
        AppendMenu(hMenu, MF_STRING | (g_tooltipsEnabled ? MF_CHECKED : MF_UNCHECKED),
                   ID_TRAY_TOOLTIPS, L"Show tooltips");
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
    WORD vals[MAX_BINDINGS]                 = {};
    int  switchMs[MAX_BINDINGS]             = {};
    int  returnMs[MAX_BINDINGS]             = {};
    WORD outputVk[MAX_BINDINGS]             = {};
    WORD longOutputVk[MAX_BINDINGS]         = {};
    WORD tertiaryOutputVk[MAX_BINDINGS]     = {};
    bool includeTertiaryInCycle[MAX_BINDINGS] = {};
    ReturnWeapon returnWeapon[MAX_BINDINGS] = {};
    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        StringCchCopy(localFontName, ARRAYSIZE(localFontName), g_fontName);
        for (int i = 0; i < profile->bindingCount; i++) {
            vals[i] = profile->bindings[i].currentVk;
            if (profile->bindings[i].behavior.type == BehaviorType::MeleeBurst) {
                switchMs[i]     = profile->bindings[i].behavior.durationMs;
                returnMs[i]     = profile->bindings[i].behavior.returnDelayMs;
                returnWeapon[i] = profile->bindings[i].behavior.returnWeapon;
            }
            if (profile->bindings[i].behavior.outputVkLabel)
                outputVk[i] = profile->bindings[i].behavior.outputVk;
            if (profile->bindings[i].behavior.longOutputVkLabel)
                longOutputVk[i] = profile->bindings[i].behavior.longOutputVk;
            if (profile->bindings[i].behavior.tertiaryOutputVkLabel) {
                tertiaryOutputVk[i]       = profile->bindings[i].behavior.tertiaryOutputVk;
                includeTertiaryInCycle[i] = profile->bindings[i].behavior.includeTertiaryInCycle;
            }
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

            if (profile->bindings[i].behavior.returnAltVk) {
                StringCchPrintf(key, ARRAYSIZE(key), L"%s_ReturnWeapon", profile->bindings[i].iniKey);
                StringCchPrintf(buf, ARRAYSIZE(buf), L"%d",
                    returnWeapon[i] == ReturnWeapon::Secondary ? 1 :
                    returnWeapon[i] == ReturnWeapon::Auto      ? 2 : 0);
                WritePrivateProfileString(profile->iniSection, key, buf, CONFIG_FILE);
            }
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
        if (profile->bindings[i].behavior.tertiaryOutputVkLabel) {
            StringCchPrintf(key, ARRAYSIZE(key), L"%s_HeavyWeaponKey", profile->bindings[i].iniKey);
            StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", tertiaryOutputVk[i]);
            WritePrivateProfileString(profile->iniSection, key, buf, CONFIG_FILE);

            StringCchPrintf(key, ARRAYSIZE(key), L"%s_IncludeHeavy", profile->bindings[i].iniKey);
            StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", includeTertiaryInCycle[i] ? 1 : 0);
            WritePrivateProfileString(profile->iniSection, key, buf, CONFIG_FILE);
        }
    }
    WritePrivateProfileString(profile->iniSection, L"FontName", localFontName, CONFIG_FILE);
}

void LoadConfig(GameProfile* profile) {
    WORD vals[MAX_BINDINGS]                  = {};
    int  switchMs[MAX_BINDINGS]              = {};
    int  returnMs[MAX_BINDINGS]              = {};
    WORD outputVk[MAX_BINDINGS]              = {};
    WORD longOutputVk[MAX_BINDINGS]          = {};
    WORD tertiaryOutputVk[MAX_BINDINGS]      = {};
    bool includeTertiaryInCycle[MAX_BINDINGS]= {};
    bool hasOutputVk[MAX_BINDINGS]           = {};
    bool hasLongOutputVk[MAX_BINDINGS]       = {};
    bool hasTertiaryOutputVk[MAX_BINDINGS]   = {};
    bool hasIncludeTertiary[MAX_BINDINGS]    = {};
    ReturnWeapon returnWeapon[MAX_BINDINGS]  = {};
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

            if (profile->bindings[i].behavior.returnAltVk) {
                StringCchPrintf(key, ARRAYSIZE(key), L"%s_ReturnWeapon", profile->bindings[i].iniKey);
                int rwVal = GetPrivateProfileInt(profile->iniSection, key, 0, CONFIG_FILE);
                returnWeapon[i] = (rwVal == 1) ? ReturnWeapon::Secondary :
                                  (rwVal == 2) ? ReturnWeapon::Auto       : ReturnWeapon::Primary;
            }
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
        if (profile->bindings[i].behavior.tertiaryOutputVkLabel) {
            StringCchPrintf(key, ARRAYSIZE(key), L"%s_HeavyWeaponKey", profile->bindings[i].iniKey);
            UINT raw = (UINT)GetPrivateProfileInt(profile->iniSection, key, -1, CONFIG_FILE);
            if (raw != (UINT)-1) { tertiaryOutputVk[i] = static_cast<WORD>(raw); hasTertiaryOutputVk[i] = true; }

            StringCchPrintf(key, ARRAYSIZE(key), L"%s_IncludeHeavy", profile->bindings[i].iniKey);
            int inc = GetPrivateProfileInt(profile->iniSection, key, -1, CONFIG_FILE);
            if (inc != -1) { includeTertiaryInCycle[i] = (inc != 0); hasIncludeTertiary[i] = true; }
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
            if (profile->bindings[i].behavior.returnAltVk)
                profile->bindings[i].behavior.returnWeapon = returnWeapon[i];
        }
        if (hasOutputVk[i])          profile->bindings[i].behavior.outputVk               = outputVk[i];
        if (hasLongOutputVk[i])      profile->bindings[i].behavior.longOutputVk           = longOutputVk[i];
        if (hasTertiaryOutputVk[i])  profile->bindings[i].behavior.tertiaryOutputVk       = tertiaryOutputVk[i];
        if (hasIncludeTertiary[i])   profile->bindings[i].behavior.includeTertiaryInCycle = includeTertiaryInCycle[i];
    }
    StringCchCopy(g_fontName, ARRAYSIZE(g_fontName), tempFont);
}

// --- BUTTON HOVER & TOOLTIP HELPERS ---

static inline COLORREF LightenColor(COLORREF c, int amt = 28) {
    return RGB(min(255, GetRValue(c) + amt),
               min(255, GetGValue(c) + amt),
               min(255, GetBValue(c) + amt));
}

LRESULT CALLBACK ButtonHoverProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SETCURSOR) {
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
    }
    if (msg == WM_MOUSEMOVE && !GetProp(hwnd, L"CCZ_Hov")) {
        SetProp(hwnd, L"CCZ_Hov", (HANDLE)1);
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        InvalidateRect(hwnd, nullptr, TRUE);
    }
    if (msg == WM_MOUSELEAVE) {
        RemoveProp(hwnd, L"CCZ_Hov");
        InvalidateRect(hwnd, nullptr, TRUE);
    }
    return CallWindowProc(g_origButtonProc, hwnd, msg, wp, lp);
}

static void SubclassButton(HWND hBtn) {
    if (!hBtn) return;
    if (!g_origButtonProc)
        g_origButtonProc = (WNDPROC)(LONG_PTR)GetWindowLongPtr(hBtn, GWLP_WNDPROC);
    SetWindowLongPtr(hBtn, GWLP_WNDPROC, (LONG_PTR)ButtonHoverProc);
}

static HWND CreateTooltipWnd(HWND hParent) {
    HWND hTT = CreateWindowEx(WS_EX_TOPMOST, L"tooltips_class32", nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hParent, nullptr, g_hInstance, nullptr);
    if (hTT) {
        SendMessage(hTT, TTM_SETMAXTIPWIDTH, 0, 220);
        SendMessage(hTT, TTM_ACTIVATE, g_tooltipsEnabled ? TRUE : FALSE, 0);
    }
    return hTT;
}

static void AddTooltip(HWND hTT, HWND hCtrl, LPCWSTR text) {
    if (!hTT || !hCtrl) return;
    TOOLINFO ti  = {};
    ti.cbSize    = sizeof(TOOLINFO);
    ti.uFlags    = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd      = GetParent(hCtrl);
    ti.uId       = (UINT_PTR)hCtrl;
    ti.lpszText  = const_cast<LPWSTR>(text);
    SendMessage(hTT, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

// --- WINDOW POSITION PERSISTENCE ---

static void SaveWindowPos(LPCWSTR keyPrefix, HWND hwnd) {
    RECT rc;
    if (!GetWindowRect(hwnd, &rc)) return;
    wchar_t buf[32], key[64];
    StringCchPrintf(key, ARRAYSIZE(key), L"%sX", keyPrefix);
    StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", rc.left);
    WritePrivateProfileString(L"App", key, buf, CONFIG_FILE);
    StringCchPrintf(key, ARRAYSIZE(key), L"%sY", keyPrefix);
    StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", rc.top);
    WritePrivateProfileString(L"App", key, buf, CONFIG_FILE);
}

// Positions hwnd at saved location, or centered on screen if none saved / off-screen.
static void RestoreOrCenterWindow(LPCWSTR keyPrefix, HWND hwnd) {
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    wchar_t key[64];
    StringCchPrintf(key, ARRAYSIZE(key), L"%sX", keyPrefix);
    int x = GetPrivateProfileInt(L"App", key, INT_MIN, CONFIG_FILE);
    StringCchPrintf(key, ARRAYSIZE(key), L"%sY", keyPrefix);
    int y = GetPrivateProfileInt(L"App", key, INT_MIN, CONFIG_FILE);

    bool useSaved = (x != INT_MIN && y != INT_MIN) &&
                    (MonitorFromPoint({ x + w / 2, y + h / 2 }, MONITOR_DEFAULTTONULL) != nullptr);

    if (!useSaved) {
        // Center on primary monitor
        x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    }
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
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

    // For OEM keys, prefer the actual character over the locale-specific scan-code name
    // (e.g. German "ZIRKUMFLEX" → "^")
    if ((vk >= VK_OEM_1 && vk <= VK_OEM_8) || vk == VK_OEM_102) {
        UINT ch = MapVirtualKey(vk, MAPVK_VK_TO_CHAR) & 0x7FFF; // mask dead-key flag
        if (ch >= 0x21 && ch <= 0x7E) {  // printable ASCII
            buffer[0] = static_cast<wchar_t>(ch);
            buffer[1] = L'\0';
            return;
        }
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
#include "games/Darktide.h"

GameProfile* g_gameProfiles[] = {
    &g_EldenRingProfile,
    &g_ToxicCommandoProfile,
    &g_DarktideProfile,
};
const int g_gameProfileCount = static_cast<int>(ARRAYSIZE(g_gameProfiles));

// --- SETTINGS WINDOW ---

void ShowChangeGameUI(); // Forward declaration

inline bool IsModifierOnlyKey(WORD vk) {
    return vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU ||
           vk == VK_LSHIFT || vk == VK_RSHIFT ||
           vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_LMENU || vk == VK_RMENU ||
           vk == VK_LWIN || vk == VK_RWIN;
}

static int ComputeSettingsContentHeight(const GameProfile* profile) {
    int rowY = LAYOUT_TITLE_START + LAYOUT_TITLE_SPACING;
    for (int i = 0; i < profile->bindingCount; i++) {
        if (profile->bindings[i].separatorAbove)
            rowY += 2 * LAYOUT_SEPARATOR_PADDING;
        const BehaviorDescriptor& beh = profile->bindings[i].behavior;
        if (beh.outputVkLabel)         rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
        if (beh.longOutputVkLabel)     rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
        if (beh.tertiaryOutputVkLabel) rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
        rowY += LAYOUT_ROW_HEIGHT;
        if (beh.type == BehaviorType::KeyToggle && beh.tertiaryOutputVk)
            rowY += LAYOUT_TIMING_ROW_HEIGHT;
        if (beh.type == BehaviorType::MeleeBurst) {
            rowY += LAYOUT_TIMING_ROWS_HEIGHT;
            if (beh.returnAltVk) rowY += LAYOUT_TIMING_ROW_HEIGHT;
        }
    }
    return rowY;
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        EnableDarkTitleBar(hwnd);
        RecreateAllFonts();

        GameProfile* profile = g_activeProfile;
        const int buttonX = LAYOUT_LEFT_MARGIN + LAYOUT_LABEL_WIDTH + LAYOUT_BUTTON_GAP;
        int rowBaseY = LAYOUT_TITLE_START + LAYOUT_TITLE_SPACING;

        g_hSettingsTooltip = CreateTooltipWnd(hwnd);

        // Top buttons: Select Game (left) | Font (right corner)
        HWND hBtnChangeGame = CreateWindow(L"BUTTON", L"Select Game",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            11, 11, LAYOUT_FONT_BUTTON_WIDTH, LAYOUT_FONT_BUTTON_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_CHANGE_GAME, nullptr, nullptr);
        SubclassButton(hBtnChangeGame);
        AddTooltip(g_hSettingsTooltip, hBtnChangeGame, L"Switch to a different game profile");

        HWND hBtnFont = CreateWindow(L"BUTTON", L"Font",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            WINDOW_WIDTH - LAYOUT_FONT_BUTTON_WIDTH - 11, 11, LAYOUT_FONT_BUTTON_WIDTH, LAYOUT_FONT_BUTTON_HEIGHT,
            hwnd, (HMENU)(INT_PTR)BTN_FONT_SETTINGS, nullptr, nullptr);
        SubclassButton(hBtnFont);
        AddTooltip(g_hSettingsTooltip, hBtnFont, L"Cycle display font");

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
            if (profile->bindings[i].separatorAbove)
                rowY += 2 * LAYOUT_SEPARATOR_PADDING;

            // Configurable output-key sub-rows (rendered before the custom-key row)
            const BehaviorDescriptor& beh = profile->bindings[i].behavior;
            const int subLabelY = (LAYOUT_OUTPUT_ROW_HEIGHT - LAYOUT_BUTTON_HEIGHT) / 2;

            if (beh.outputVkLabel) {
                HWND hOL = CreateWindow(L"STATIC", beh.outputVkLabel,
                    WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                    LAYOUT_LEFT_MARGIN + LAYOUT_LABEL_INDENT, rowY + subLabelY, LAYOUT_LABEL_WIDTH - LAYOUT_LABEL_INDENT, LAYOUT_BUTTON_HEIGHT,
                    hwnd, (HMENU)(INT_PTR)(ID_OUTPUT_LABEL_BASE + i), nullptr, nullptr);
                SendMessage(hOL, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                {
                    HWND hB = CreateWindow(L"BUTTON", L"...",
                        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                        buttonX, rowY + subLabelY, LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                        hwnd, (HMENU)(INT_PTR)(BTN_OUTPUT_KEY_BASE + i), nullptr, nullptr);
                    SubclassButton(hB);
                    AddTooltip(g_hSettingsTooltip, hB, L"Set the in-game key to press");
                }
                {
                    HWND hB = CreateWindow(L"BUTTON", L"\u00d7",
                        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                        clearX, rowY + subLabelY, LAYOUT_CLEAR_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                        hwnd, (HMENU)(INT_PTR)(BTN_CLEAR_OUTPUT_BASE + i), nullptr, nullptr);
                    SubclassButton(hB);
                    AddTooltip(g_hSettingsTooltip, hB, L"Clear in-game key");
                }

                rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
            }

            if (beh.longOutputVkLabel) {
                HWND hLOL = CreateWindow(L"STATIC", beh.longOutputVkLabel,
                    WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                    LAYOUT_LEFT_MARGIN + LAYOUT_LABEL_INDENT, rowY + subLabelY, LAYOUT_LABEL_WIDTH - LAYOUT_LABEL_INDENT, LAYOUT_BUTTON_HEIGHT,
                    hwnd, (HMENU)(INT_PTR)(ID_LONG_OUTPUT_LABEL_BASE + i), nullptr, nullptr);
                SendMessage(hLOL, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                {
                    HWND hB = CreateWindow(L"BUTTON", L"...",
                        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                        buttonX, rowY + subLabelY, LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                        hwnd, (HMENU)(INT_PTR)(BTN_LONG_OUTPUT_KEY_BASE + i), nullptr, nullptr);
                    SubclassButton(hB);
                    AddTooltip(g_hSettingsTooltip, hB, L"Set the secondary in-game key");
                }
                {
                    HWND hB = CreateWindow(L"BUTTON", L"\u00d7",
                        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                        clearX, rowY + subLabelY, LAYOUT_CLEAR_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                        hwnd, (HMENU)(INT_PTR)(BTN_CLEAR_LONG_OUTPUT_BASE + i), nullptr, nullptr);
                    SubclassButton(hB);
                    AddTooltip(g_hSettingsTooltip, hB, L"Clear secondary key");
                }

                rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
            }

            if (beh.tertiaryOutputVkLabel) {
                HWND hTOL = CreateWindow(L"STATIC", beh.tertiaryOutputVkLabel,
                    WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                    LAYOUT_LEFT_MARGIN + LAYOUT_LABEL_INDENT, rowY + subLabelY, LAYOUT_LABEL_WIDTH - LAYOUT_LABEL_INDENT, LAYOUT_BUTTON_HEIGHT,
                    hwnd, (HMENU)(INT_PTR)(ID_TERTIARY_OUTPUT_LABEL_BASE + i), nullptr, nullptr);
                SendMessage(hTOL, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                {
                    HWND hB = CreateWindow(L"BUTTON", L"...",
                        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                        buttonX, rowY + subLabelY, LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                        hwnd, (HMENU)(INT_PTR)(BTN_TERTIARY_OUTPUT_KEY_BASE + i), nullptr, nullptr);
                    SubclassButton(hB);
                    AddTooltip(g_hSettingsTooltip, hB, L"Set the heavy weapon in-game key");
                }
                {
                    HWND hB = CreateWindow(L"BUTTON", L"\u00d7",
                        WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                        clearX, rowY + subLabelY, LAYOUT_CLEAR_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                        hwnd, (HMENU)(INT_PTR)(BTN_CLEAR_TERTIARY_OUTPUT_BASE + i), nullptr, nullptr);
                    SubclassButton(hB);
                    AddTooltip(g_hSettingsTooltip, hB, L"Clear heavy weapon key");
                }

                rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
            }

            int labelY = rowY + 9; // small gap above custom-key row

            HWND hLabel = CreateWindow(L"STATIC", profile->bindings[i].label,
                WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                LAYOUT_LEFT_MARGIN + LAYOUT_LABEL_INDENT, labelY, LAYOUT_LABEL_WIDTH - LAYOUT_LABEL_INDENT, LAYOUT_BUTTON_HEIGHT,
                hwnd, (HMENU)(INT_PTR)(ID_LABEL_BASE + i), nullptr, nullptr);
            SendMessage(hLabel, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            {
                HWND hB = CreateWindow(L"BUTTON", L"...",
                    WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                    buttonX, labelY, LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                    hwnd, (HMENU)(INT_PTR)(BTN_BIND_BASE + i), nullptr, nullptr);
                SubclassButton(hB);
                AddTooltip(g_hSettingsTooltip, hB, L"Click to bind a new key");
            }

            // × clear button to unset this input binding
            {
                HWND hB = CreateWindow(L"BUTTON", L"\u00d7",
                    WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                    clearX, labelY, LAYOUT_CLEAR_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT,
                    hwnd, (HMENU)(INT_PTR)(BTN_CLEAR_BASE + i), nullptr, nullptr);
                SubclassButton(hB);
                AddTooltip(g_hSettingsTooltip, hB, L"Clear key binding");
            }

            rowY += LAYOUT_ROW_HEIGHT;

            // Include-in-quickswitch toggle for KeyToggle bindings that have a tertiary key
            if (beh.type == BehaviorType::KeyToggle && beh.tertiaryOutputVk) {
                const int editH = LAYOUT_BUTTON_HEIGHT;
                HWND hL = CreateWindow(L"STATIC", L"Include Heavy Weapon in Quickswap:",
                    WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                    LAYOUT_LEFT_MARGIN + 22, rowY + 6, LAYOUT_LABEL_WIDTH - 22, editH,
                    hwnd, nullptr, nullptr, nullptr);
                SendMessage(hL, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                HWND hCB = CreateWindow(L"COMBOBOX", nullptr,
                    WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                    buttonX, rowY + 6, LAYOUT_TIMING_EDIT_WIDTH, editH * 5,
                    hwnd, (HMENU)(INT_PTR)(ID_INCLUDE_TERTIARY_BASE + i), nullptr, nullptr);
                SendMessage(hCB, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
                SendMessage(hCB, CB_ADDSTRING, 0, (LPARAM)L"No");
                SendMessage(hCB, CB_ADDSTRING, 0, (LPARAM)L"Yes");
                SendMessage(hCB, CB_SETCURSEL, (WPARAM)(beh.includeTertiaryInCycle ? 1 : 0), 0);

                rowY += LAYOUT_TIMING_ROW_HEIGHT;
            }

            // Timing sub-rows for MeleeBurst bindings
            if (profile->bindings[i].behavior.type == BehaviorType::MeleeBurst) {
                const BehaviorDescriptor& desc = profile->bindings[i].behavior;
                wchar_t buf[16];
                const int editH = LAYOUT_BUTTON_HEIGHT;

                // Switch delay row
                HWND hL1 = CreateWindow(L"STATIC", L"First swing delay (ms):",
                    WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                    LAYOUT_LEFT_MARGIN + 22, rowY + 6, LAYOUT_LABEL_WIDTH - 22, editH,
                    hwnd, nullptr, nullptr, nullptr);
                SendMessage(hL1, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", desc.durationMs);
                HWND hE1 = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", buf,
                    WS_VISIBLE | WS_CHILD | ES_NUMBER | ES_CENTER,
                    buttonX, rowY + 6, LAYOUT_TIMING_EDIT_WIDTH, editH,
                    hwnd, (HMENU)(INT_PTR)(ID_TIMING_SWITCH_BASE + i), nullptr, nullptr);
                SendMessage(hE1, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                rowY += LAYOUT_TIMING_ROW_HEIGHT;

                // Return weapon selection row (only when a secondary weapon key is defined)
                if (desc.returnAltVk) {
                    HWND hL3 = CreateWindow(L"STATIC", L"Return to last used weapon:",
                        WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                        LAYOUT_LEFT_MARGIN + 22, rowY + 6, LAYOUT_LABEL_WIDTH - 22, editH,
                        hwnd, nullptr, nullptr, nullptr);
                    SendMessage(hL3, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                    HWND hCB = CreateWindow(L"COMBOBOX", nullptr,
                        WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                        buttonX, rowY + 6, LAYOUT_TIMING_EDIT_WIDTH, editH * 5,
                        hwnd, (HMENU)(INT_PTR)(ID_RETURN_WEAPON_BASE + i), nullptr, nullptr);
                    SendMessage(hCB, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
                    SendMessage(hCB, CB_ADDSTRING, 0, (LPARAM)L"Primary");
                    SendMessage(hCB, CB_ADDSTRING, 0, (LPARAM)L"Secondary");
                    SendMessage(hCB, CB_ADDSTRING, 0, (LPARAM)L"Auto");
                    SendMessage(hCB, CB_SETCURSEL,
                        (WPARAM)(desc.returnWeapon == ReturnWeapon::Secondary ? 1 :
                                 desc.returnWeapon == ReturnWeapon::Auto      ? 2 : 0), 0);

                    rowY += LAYOUT_TIMING_ROW_HEIGHT;
                }

                // Return delay row
                HWND hL2 = CreateWindow(L"STATIC", L"Return delay (ms):",
                    WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
                    LAYOUT_LEFT_MARGIN + 22, rowY + 6, LAYOUT_LABEL_WIDTH - 22, editH,
                    hwnd, nullptr, nullptr, nullptr);
                SendMessage(hL2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                StringCchPrintf(buf, ARRAYSIZE(buf), L"%d", desc.returnDelayMs);
                HWND hE2 = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", buf,
                    WS_VISIBLE | WS_CHILD | ES_NUMBER | ES_CENTER,
                    buttonX, rowY + 6, LAYOUT_TIMING_EDIT_WIDTH, editH,
                    hwnd, (HMENU)(INT_PTR)(ID_TIMING_RETURN_BASE + i), nullptr, nullptr);
                SendMessage(hE2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

                rowY += LAYOUT_TIMING_ROW_HEIGHT;
            }
        }


        // Set initial button texts
        {
            std::lock_guard<std::mutex> lock(g_configMutex);
            for (int i = 0; i < profile->bindingCount; i++) {
                UpdateButtonText(GetDlgItem(hwnd, BTN_BIND_BASE + i), profile->bindings[i].currentVk);
                if (profile->bindings[i].behavior.outputVkLabel)
                    UpdateButtonText(GetDlgItem(hwnd, BTN_OUTPUT_KEY_BASE + i), profile->bindings[i].behavior.outputVk);
                if (profile->bindings[i].behavior.longOutputVkLabel)
                    UpdateButtonText(GetDlgItem(hwnd, BTN_LONG_OUTPUT_KEY_BASE + i), profile->bindings[i].behavior.longOutputVk);
                if (profile->bindings[i].behavior.tertiaryOutputVkLabel)
                    UpdateButtonText(GetDlgItem(hwnd, BTN_TERTIARY_OUTPUT_KEY_BASE + i), profile->bindings[i].behavior.tertiaryOutputVk);
            }
        }

        break;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_hBrushBg);
        if (g_activeProfile && g_activeProfile->theme.scanlines) {
            COLORREF bg = g_activeProfile->theme.bg;
            COLORREF scanC = RGB(GetRValue(bg) / 3, GetGValue(bg) / 3, GetBValue(bg) / 3);
            HBRUSH hScan = CreateSolidBrush(scanC);
            for (int y = 0; y < rc.bottom; y += 2) {
                RECT lr = { rc.left, y, rc.right, y + 1 };
                FillRect(hdc, &lr, hScan);
            }
            DeleteObject(hScan);
        }
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        SetViewportOrgEx(hdc, 0, -g_settingsScrollY, nullptr);

        if (g_activeProfile) {
            // Legend: colored squares showing what each stripe color means
            int legendY = LAYOUT_TITLE_START + LAYOUT_TITLE_HEIGHT + 3;
            HFONT hOldFont = (HFONT)SelectObject(hdc, g_hFontImprint);
            SetTextColor(hdc, g_activeProfile->theme.text);
            SetBkMode(hdc, TRANSPARENT);

            // Helper: draw a 15×13 yellow warning triangle with a black "!" at (x, y)
            auto DrawWarningTri = [&](int x, int y) {
                constexpr int W = 15, H = 13;
                POINT pts[3] = { { x + W / 2, y }, { x, y + H }, { x + W, y + H } };
                HBRUSH hY    = CreateSolidBrush(RGB(255, 210, 0));
                HPEN   hBk   = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
                HBRUSH hOBr  = (HBRUSH)SelectObject(hdc, hY);
                HPEN   hOPn  = (HPEN)SelectObject(hdc, hBk);
                Polygon(hdc, pts, 3);
                SelectObject(hdc, hOBr);
                SelectObject(hdc, hOPn);
                DeleteObject(hY);
                DeleteObject(hBk);
                HFONT hF    = CreateFont(10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                COLORREF old = SetTextColor(hdc, RGB(0, 0, 0));
                HFONT hOF   = (HFONT)SelectObject(hdc, hF);
                RECT rE     = { x, y + 1, x + W, y + H };
                DrawText(hdc, L"!", -1, &rE, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, hOF);
                DeleteObject(hF);
                SetTextColor(hdc, old);
            };

            // Legend: in-game key = yellow warning triangle
            DrawWarningTri(LAYOUT_LEFT_MARGIN, legendY + 2);
            RECT tIG = { LAYOUT_LEFT_MARGIN + 18, legendY, LAYOUT_LEFT_MARGIN + 168, legendY + LAYOUT_LEGEND_HEIGHT };
            DrawText(hdc, L"In-Game Key", -1, &tIG, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            HBRUSH hBrApp = CreateSolidBrush(RGB(70, 130, 200));
            RECT rAP = { LAYOUT_LEFT_MARGIN + 200, legendY + 2, LAYOUT_LEFT_MARGIN + 213, legendY + 15 };
            FillRect(hdc, &rAP, hBrApp);
            DeleteObject(hBrApp);
            RECT tAP = { LAYOUT_LEFT_MARGIN + 218, legendY, LAYOUT_LEFT_MARGIN + 449, legendY + LAYOUT_LEGEND_HEIGHT };
            DrawText(hdc, L"Custom Key", -1, &tAP, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Info hint below legend
            COLORREF dimColor = RGB(
                (GetRValue(g_activeProfile->theme.text) + GetRValue(g_activeProfile->theme.bg)) / 2,
                (GetGValue(g_activeProfile->theme.text) + GetGValue(g_activeProfile->theme.bg)) / 2,
                (GetBValue(g_activeProfile->theme.text) + GetBValue(g_activeProfile->theme.bg)) / 2);
            SetTextColor(hdc, dimColor);
            RECT tHint = { LAYOUT_LEFT_MARGIN, legendY + LAYOUT_LEGEND_HEIGHT + 3,
                           LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH, legendY + LAYOUT_LEGEND_HEIGHT + 3 + LAYOUT_LEGEND_HEIGHT };
            DrawText(hdc, L"Make sure to match 'In-Game Keys' with your actual setting in the game.", -1, &tHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hOldFont);

            // Binding rows
            HBRUSH hRowBrush = CreateSolidBrush(g_activeProfile->theme.rowBg);
            HPEN   hLinePen  = CreatePen(PS_SOLID, 2, g_activeProfile->theme.separator);
            HPEN   hOldPen   = (HPEN)SelectObject(hdc, hLinePen);

            int rowBaseY = LAYOUT_TITLE_START + LAYOUT_TITLE_SPACING;
            int rowY = rowBaseY;
            for (int i = 0; i < g_activeProfile->bindingCount; i++) {
                // Separator line with extra breathing room above and below
                if (g_activeProfile->bindings[i].separatorAbove) {
                    rowY += LAYOUT_SEPARATOR_PADDING;
                    MoveToEx(hdc, LAYOUT_LEFT_MARGIN, rowY, nullptr);
                    LineTo(hdc, LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH, rowY);
                    rowY += LAYOUT_SEPARATOR_PADDING;
                }

                const auto& beh = g_activeProfile->bindings[i].behavior;

                // Output sub-row highlights (rendered before the custom-key row)
                const int subOffset = (LAYOUT_OUTPUT_ROW_HEIGHT - LAYOUT_BUTTON_HEIGHT) / 2;
                if (beh.outputVkLabel) {
                    RECT subRect = {
                        LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING, rowY + subOffset,
                        LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH + LAYOUT_ROW_PADDING, rowY + subOffset + LAYOUT_BUTTON_HEIGHT
                    };
                    FillRect(hdc, &subRect, hRowBrush);
                    DrawWarningTri(LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING,
                                   rowY + subOffset + (LAYOUT_BUTTON_HEIGHT - 13) / 2);
                    rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
                }
                if (beh.longOutputVkLabel) {
                    RECT subRect = {
                        LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING, rowY + subOffset,
                        LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH + LAYOUT_ROW_PADDING, rowY + subOffset + LAYOUT_BUTTON_HEIGHT
                    };
                    FillRect(hdc, &subRect, hRowBrush);
                    DrawWarningTri(LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING,
                                   rowY + subOffset + (LAYOUT_BUTTON_HEIGHT - 13) / 2);
                    rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
                }
                if (beh.tertiaryOutputVkLabel) {
                    RECT subRect = {
                        LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING, rowY + subOffset,
                        LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH + LAYOUT_ROW_PADDING, rowY + subOffset + LAYOUT_BUTTON_HEIGHT
                    };
                    FillRect(hdc, &subRect, hRowBrush);
                    DrawWarningTri(LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING,
                                   rowY + subOffset + (LAYOUT_BUTTON_HEIGHT - 13) / 2);
                    rowY += LAYOUT_OUTPUT_ROW_HEIGHT;
                }

                // Row background highlight
                RECT rowRect = {
                    LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING,
                    rowY + 7,
                    LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH + LAYOUT_ROW_PADDING,
                    rowY + 7 + LAYOUT_BUTTON_HEIGHT
                };
                FillRect(hdc, &rowRect, hRowBrush);

                // Left indicator: warning triangle for in-game keys, blue stripe for custom keys
                if (g_activeProfile->bindings[i].isAppOnly) {
                    HBRUSH hStripe = CreateSolidBrush(RGB(70, 130, 200));
                    RECT stripeRect = {
                        LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING,
                        rowY + 7,
                        LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING + 5,
                        rowY + 7 + LAYOUT_BUTTON_HEIGHT
                    };
                    FillRect(hdc, &stripeRect, hStripe);
                    DeleteObject(hStripe);
                } else {
                    // Center the 15×13 triangle vertically in the row button
                    DrawWarningTri(LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING,
                                   rowY + 7 + (LAYOUT_BUTTON_HEIGHT - 13) / 2);
                }

                rowY += LAYOUT_ROW_HEIGHT;
                if (beh.type == BehaviorType::KeyToggle && beh.tertiaryOutputVk)
                    rowY += LAYOUT_TIMING_ROW_HEIGHT;
                if (beh.type == BehaviorType::MeleeBurst) {
                    rowY += LAYOUT_TIMING_ROWS_HEIGHT;
                    if (beh.returnAltVk) rowY += LAYOUT_TIMING_ROW_HEIGHT;
                }
            }

            SelectObject(hdc, hOldPen);
            DeleteObject(hLinePen);
            DeleteObject(hRowBrush);
        }

        // "More content below" indicator — drawn in physical coords, unaffected by scroll
        SetViewportOrgEx(hdc, 0, 0, nullptr);
        if (g_activeProfile) {
            RECT cr;
            GetClientRect(hwnd, &cr);
            int contentH  = ComputeSettingsContentHeight(g_activeProfile);
            int maxScroll = max(0, contentH - cr.bottom);
            if (maxScroll > 0 && g_settingsScrollY < maxScroll) {
                constexpr int TW = 22, TH = 11;
                int cx = cr.right / 2;
                int by = cr.bottom - 10;
                POINT pts[3] = {
                    { cx - TW / 2, by - TH },
                    { cx + TW / 2, by - TH },
                    { cx,          by      }
                };
                HPEN   hPen    = CreatePen(PS_SOLID, 2, g_activeProfile->theme.accent);
                HPEN   hOldPen = (HPEN)SelectObject(hdc, hPen);
                HBRUSH hOldBr  = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Polygon(hdc, pts, 3);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBr);
                DeleteObject(hPen);
            }
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
            {
                bool hovered = (GetProp(pDIS->hwndItem, L"CCZ_Hov") != nullptr);
                COLORREF fill = hovered ? LightenColor(style.fillColor) : style.fillColor;
                HBRUSH hFill = CreateSolidBrush(fill);
                FillRect(hdc, &rc, hFill);
                DeleteObject(hFill);
            }

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
        } else if (id >= BTN_TERTIARY_OUTPUT_KEY_BASE && g_activeProfile &&
                   id < BTN_TERTIARY_OUTPUT_KEY_BASE + g_activeProfile->bindingCount) {
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
        } else if (id >= BTN_CLEAR_OUTPUT_BASE && g_activeProfile &&
                   id < BTN_CLEAR_OUTPUT_BASE + g_activeProfile->bindingCount) {
            int idx = id - BTN_CLEAR_OUTPUT_BASE;
            {
                std::lock_guard<std::mutex> lock(g_configMutex);
                g_activeProfile->bindings[idx].behavior.outputVk = 0;
            }
            UpdateButtonText(GetDlgItem(hwnd, BTN_OUTPUT_KEY_BASE + idx), 0);
            SaveConfig(g_activeProfile);
        } else if (id >= BTN_CLEAR_LONG_OUTPUT_BASE && g_activeProfile &&
                   id < BTN_CLEAR_LONG_OUTPUT_BASE + g_activeProfile->bindingCount) {
            int idx = id - BTN_CLEAR_LONG_OUTPUT_BASE;
            {
                std::lock_guard<std::mutex> lock(g_configMutex);
                g_activeProfile->bindings[idx].behavior.longOutputVk = 0;
            }
            UpdateButtonText(GetDlgItem(hwnd, BTN_LONG_OUTPUT_KEY_BASE + idx), 0);
            SaveConfig(g_activeProfile);
        } else if (id >= BTN_CLEAR_TERTIARY_OUTPUT_BASE && g_activeProfile &&
                   id < BTN_CLEAR_TERTIARY_OUTPUT_BASE + g_activeProfile->bindingCount) {
            int idx = id - BTN_CLEAR_TERTIARY_OUTPUT_BASE;
            {
                std::lock_guard<std::mutex> lock(g_configMutex);
                g_activeProfile->bindings[idx].behavior.tertiaryOutputVk = 0;
            }
            UpdateButtonText(GetDlgItem(hwnd, BTN_TERTIARY_OUTPUT_KEY_BASE + idx), 0);
            SaveConfig(g_activeProfile);
        } else if (HIWORD(wParam) == CBN_SELCHANGE && g_activeProfile) {
            // Include-heavy combobox selection changed
            if (id >= ID_INCLUDE_TERTIARY_BASE && id < ID_INCLUDE_TERTIARY_BASE + MAX_BINDINGS) {
                int idx = id - ID_INCLUDE_TERTIARY_BASE;
                if (idx < g_activeProfile->bindingCount) {
                    int sel = (int)SendMessage(GetDlgItem(hwnd, id), CB_GETCURSEL, 0, 0);
                    {
                        std::lock_guard<std::mutex> lock(g_configMutex);
                        g_activeProfile->bindings[idx].behavior.includeTertiaryInCycle = (sel == 1);
                    }
                    SaveConfig(g_activeProfile);
                }
            // Return weapon combobox selection changed
            } else if (id >= ID_RETURN_WEAPON_BASE && id < ID_RETURN_WEAPON_BASE + MAX_BINDINGS) {
                int idx = id - ID_RETURN_WEAPON_BASE;
                if (idx < g_activeProfile->bindingCount
                    && g_activeProfile->bindings[idx].behavior.type == BehaviorType::MeleeBurst) {
                    int sel = (int)SendMessage(GetDlgItem(hwnd, id), CB_GETCURSEL, 0, 0);
                    {
                        std::lock_guard<std::mutex> lock(g_configMutex);
                        g_activeProfile->bindings[idx].behavior.returnWeapon =
                            (sel == 1) ? ReturnWeapon::Secondary :
                            (sel == 2) ? ReturnWeapon::Auto      : ReturnWeapon::Primary;
                    }
                    SaveConfig(g_activeProfile);
                }
            }
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
            } else if (bindID >= BTN_TERTIARY_OUTPUT_KEY_BASE && bindID < BTN_TERTIARY_OUTPUT_KEY_BASE + g_activeProfile->bindingCount) {
                idx = bindID - BTN_TERTIARY_OUTPUT_KEY_BASE;
                std::lock_guard<std::mutex> lock(g_configMutex);
                g_activeProfile->bindings[idx].behavior.tertiaryOutputVk = newKey;
                UpdateButtonText(GetDlgItem(hwnd, bindID), newKey);
            }

            g_waitingForBindID = 0;
            if (idx >= 0) SaveConfig(g_activeProfile);
        }
        break;
    }

    case WM_MOUSEWHEEL: {
        int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        int lines = max(1, abs(wheelDelta) / WHEEL_DELTA);
        int step  = lines * LAYOUT_ROW_HEIGHT * ((wheelDelta > 0) ? -1 : 1);
        RECT cr;
        GetClientRect(hwnd, &cr);
        int contentH  = g_activeProfile ? ComputeSettingsContentHeight(g_activeProfile) : cr.bottom;
        int maxScroll = max(0, contentH - cr.bottom);
        int newY   = max(0, min(g_settingsScrollY + step, maxScroll));
        int wDelta = newY - g_settingsScrollY;
        if (wDelta != 0) {
            g_settingsScrollY = newY;
            // Move all child windows by -delta (avoids pixel-copy artefacts from ScrollWindowEx)
            HWND hChild = GetWindow(hwnd, GW_CHILD);
            while (hChild) {
                RECT wr;
                GetWindowRect(hChild, &wr);
                POINT pt = { wr.left, wr.top };
                ScreenToClient(hwnd, &pt);
                SetWindowPos(hChild, nullptr, pt.x, pt.y - wDelta, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                hChild = GetWindow(hChild, GW_HWNDNEXT);
            }
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
        }
        return 0;
    }

    case WM_MOVE:
        SaveWindowPos(L"SettingsWin", hwnd);
        break;

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_MINIMIZE) {
            ShowWindow(hwnd, SW_HIDE);
            g_waitingForBindID = 0;
            return 0;
        }
        break;

    case WM_CLOSE:
        if (MessageBox(hwnd, L"Are you sure you want to exit?", L"Exit",
                       MB_YESNO | MB_ICONQUESTION) == IDYES) {
            DestroyWindow(g_hMainWindow);
        }
        return 0;

    case WM_DESTROY:
        CleanupFonts();
        g_hSettingsTooltip = nullptr; // child destroyed with the window
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

    g_settingsScrollY = 0;

    // Fixed client area: WINDOW_WIDTH × WINDOW_HEIGHT
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
    if (g_hSettingsWnd) RestoreOrCenterWindow(L"SettingsWin", g_hSettingsWnd);
    return g_hSettingsWnd != nullptr;
}

// --- GAME SELECTION WINDOW ---

void OnGameSelected(int profileIndex, bool silent = false) {
    StopGameLogicThread();

    // Persist last game for silent autostart on next launch
    wchar_t idxBuf[8];
    StringCchPrintf(idxBuf, ARRAYSIZE(idxBuf), L"%d", profileIndex);
    WritePrivateProfileString(L"App", L"LastGame", idxBuf, CONFIG_FILE);

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

    ShowWindow(g_hGameSelectWnd, SW_HIDE);

    if (!silent) {
        // Place settings window at same screen position as game select window
        if (selectRect.left != 0 || selectRect.top != 0) {
            SetWindowPos(g_hSettingsWnd, nullptr,
                         selectRect.left, selectRect.top, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);
        }
        ShowWindow(g_hSettingsWnd, SW_SHOW);
        SetForegroundWindow(g_hSettingsWnd);
    }

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

        HFONT hFontTitle   = CreateFont(32, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hFontGameBtn = CreateFont(40, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hFontNormal  = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        // Store fonts in GWLP_USERDATA for cleanup: [0]=title, [1]=game button bold, [2]=normal
        HFONT* fonts = new HFONT[3]{ hFontTitle, hFontGameBtn, hFontNormal };
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)fonts);

        HWND hTitle = CreateWindow(L"STATIC", L"CustomControlZ",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, SELECT_TITLE_Y, WINDOW_WIDTH, 55, hwnd,
            (HMENU)(INT_PTR)ID_SELECT_TITLE, nullptr, nullptr);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

        HWND hSub = CreateWindow(L"STATIC", L"Game Selector",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, SELECT_SUBTITLE_Y, WINDOW_WIDTH, 37, hwnd,
            (HMENU)(INT_PTR)ID_SELECT_SUBTITLE, nullptr, nullptr);
        SendMessage(hSub, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        g_hSelectTooltip = CreateTooltipWnd(hwnd);

        // Game selection buttons — bold text, themed
        for (int i = 0; i < g_gameProfileCount; i++) {
            int btnY = SELECT_FIRST_BTN_Y + i * SELECT_GAME_BTN_SPACING;
            HWND hB = CreateWindow(L"BUTTON", g_gameProfiles[i]->displayName,
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                SELECT_BTN_MARGIN_X, btnY,
                WINDOW_WIDTH - 2 * SELECT_BTN_MARGIN_X, SELECT_GAME_BTN_HEIGHT,
                hwnd, (HMENU)(INT_PTR)(BTN_GAME_BASE + i), nullptr, nullptr);
            SubclassButton(hB);
            AddTooltip(g_hSelectTooltip, hB, g_gameProfiles[i]->displayName);
        }

        // Credits button (bottom-left)
        int bottomY = WINDOW_HEIGHT - LAYOUT_BOTTOM_SPACING;
        {
            HWND hB = CreateWindow(L"BUTTON", L"Credits",
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                11, bottomY, LAYOUT_FONT_BUTTON_WIDTH, LAYOUT_BOTTOM_BUTTON_HEIGHT,
                hwnd, (HMENU)(INT_PTR)BTN_SELECT_CREDITS, nullptr, nullptr);
            SubclassButton(hB);
            AddTooltip(g_hSelectTooltip, hB, L"About CustomControlZ");
        }
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

        bool hovered = (GetProp(pDIS->hwndItem, L"CCZ_Hov") != nullptr);

        if (pDIS->CtlID == BTN_SELECT_EXIT) {
            // Exit: dark red
            COLORREF fillC = hovered ? LightenColor(RGB(140, 30, 30)) : RGB(140, 30, 30);
            HBRUSH hBr = CreateSolidBrush(fillC);
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
            COLORREF fillC = hovered ? LightenColor(RGB(40, 40, 40)) : RGB(40, 40, 40);
            HBRUSH hBr = CreateSolidBrush(fillC);
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
                COLORREF fillC = hovered ? LightenColor(gp->theme.selectBg) : gp->theme.selectBg;
                HBRUSH hBr = CreateSolidBrush(fillC);
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
                wchar_t name[64];
                wcsncpy_s(name, gp->displayName, ARRAYSIZE(name) - 1);
                CharUpperW(name);
                DrawText(hdc, name, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
        return TRUE;
    }

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_MINIMIZE) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == BTN_SELECT_CREDITS) {
            MessageBox(hwnd,
                L"Idea and development: B\u00f6rni (burni2001)\n\n"
                L"Development tools:\n"
                L"Claude Code, Visual Studio Code, GitHub Copilot\n\n"
                L"Version: " APP_VERSION,
                L"Credits", MB_OK | MB_ICONINFORMATION);
        } else if (id >= BTN_GAME_BASE && id < BTN_GAME_BASE + g_gameProfileCount) {
            OnGameSelected(id - BTN_GAME_BASE);
        }
        break;
    }

    case WM_MOVE:
        SaveWindowPos(L"SelectWin", hwnd);
        break;

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
        g_hSelectTooltip = nullptr; // child destroyed with the window
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
    if (g_hGameSelectWnd) RestoreOrCenterWindow(L"SelectWin", g_hGameSelectWnd);
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

                if (cmd >= ID_TRAY_GAME_BASE && cmd < ID_TRAY_GAME_BASE + g_gameProfileCount) {
                    OnGameSelected(cmd - ID_TRAY_GAME_BASE);
                } else if (cmd == ID_TRAY_SETTINGS) {
                    if (g_activeProfile && g_hSettingsWnd) {
                        ShowWindow(g_hSettingsWnd, SW_SHOW);
                        SetForegroundWindow(g_hSettingsWnd);
                    } else if (g_hGameSelectWnd) {
                        ShowWindow(g_hGameSelectWnd, SW_SHOW);
                        SetForegroundWindow(g_hGameSelectWnd);
                    }
                } else if (cmd == ID_TRAY_CHANGE_GAME) {
                    ShowChangeGameUI();
                } else if (cmd == ID_TRAY_AUTOSTART) {
                    SetAutostart(!IsAutostartEnabled());
                } else if (cmd == ID_TRAY_TOOLTIPS) {
                    g_tooltipsEnabled = !g_tooltipsEnabled;
                    WritePrivateProfileString(L"App", L"Tooltips",
                        g_tooltipsEnabled ? L"1" : L"0", CONFIG_FILE);
                    if (g_hSettingsTooltip) SendMessage(g_hSettingsTooltip, TTM_ACTIVATE, g_tooltipsEnabled, 0);
                    if (g_hSelectTooltip)   SendMessage(g_hSelectTooltip,   TTM_ACTIVATE, g_tooltipsEnabled, 0);
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

    // Build absolute config path from exe location so autostart works correctly
    {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        wchar_t* lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash) lastSlash[1] = L'\0';
        StringCchCopy(CONFIG_FILE, MAX_PATH, exePath);
        StringCchCat(CONFIG_FILE, MAX_PATH, L"settings.ini");
    }

    g_tooltipsEnabled = (GetPrivateProfileInt(L"App", L"Tooltips", 1, CONFIG_FILE) != 0);

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

    // If a game was previously selected, resume it silently; otherwise show game selector
    int lastGame = GetPrivateProfileInt(L"App", L"LastGame", -1, CONFIG_FILE);
    if (lastGame >= 0 && lastGame < g_gameProfileCount) {
        OnGameSelected(lastGame, true);
    } else {
        ShowWindow(g_hGameSelectWnd, SW_SHOW);
        SetForegroundWindow(g_hGameSelectWnd);
    }

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
