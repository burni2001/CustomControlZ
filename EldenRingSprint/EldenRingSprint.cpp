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

// Undocumented APIs for Windows 11 Dark Mode
enum PreferredAppMode { Default, AllowDark, ForceDark, ForceLight, Max };
typedef PreferredAppMode(WINAPI* fnSetPreferredAppMode)(PreferredAppMode appMode);

// Dark Mode Window Attribute
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

void EnableWindows11DarkMode() {
    HMODULE hUxtheme = LoadLibraryEx(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUxtheme) {
        // Ordinal 135 is SetPreferredAppMode
        auto SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(
            GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135)));

        if (SetPreferredAppMode) {
            SetPreferredAppMode(AllowDark);
        }
        // Don't free library - needed for app lifetime
    }
}

void EnableDarkTitleBar(HWND hwnd) {
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
}

#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup")

// --- WINDOW IDS ---
#define ICON_ID_EXE     108 
#define ICON_ID_IDLE    109 
#define ICON_ID_ACTIVE  110 

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_SETTINGS 1001
#define ID_TRAY_EXIT     1002

#define BTN_BIND_INGAME  2001
#define BTN_BIND_DODGE   2002
#define BTN_BIND_SPRINT  2003
#define BTN_BIND_TRIGGER 2004
#define BTN_EXIT_SETTINGS 2005
#define BTN_EXIT_APP      2006
#define BTN_FONT_SETTINGS 2007

// Static control IDs
#define ID_TITLE_STATIC     2100
#define ID_SECTION1_STATIC  2101
#define ID_SECTION2_STATIC  2102
#define ID_SECTION3_STATIC  2103
#define ID_IMPRINT1_STATIC  2104
#define ID_IMPRINT2_STATIC  2105
#define ID_LABEL1_STATIC    2106
#define ID_LABEL2_STATIC    2107
#define ID_LABEL3_STATIC    2108
#define ID_LABEL4_STATIC    2109

// Font menu IDs
#define ID_FONT_PALATINO     3001
#define ID_FONT_SEGOE_UI     3002
#define ID_FONT_COMIC_SANS   3003

// --- TIMING CONSTANTS ---
constexpr int LONG_PRESS_DELAY_MS = 400;
constexpr int LOGIC_TICK_MS = 10;
constexpr int BIND_CHECK_MS = 50;
constexpr int GAME_CHECK_IDLE_MS = 1000;
constexpr int DODGE_DURATION_MS = 50;
constexpr int SPRINT_DODGE_PAUSE_MS = 20;
constexpr int ACTION_DURATION_MS = 30;
constexpr int SHELL_CHECK_INTERVAL_MS = 100;
constexpr int TRAY_RETRY_INTERVAL_MS = 500;
constexpr int SHELL_TIMEOUT_SEC = 30;
constexpr int TRAY_MAX_RETRIES = 10;

// --- BUFFER SIZES ---
constexpr size_t KEY_NAME_BUFFER = 64;
constexpr size_t BUTTON_TEXT_BUFFER = 256;
constexpr size_t CONFIG_BUFFER = 32;
constexpr size_t FONT_NAME_BUFFER = 64;

// --- UI LAYOUT CONSTANTS ---
constexpr int LAYOUT_LEFT_MARGIN = 50;
constexpr int LAYOUT_LABEL_WIDTH = 420;
constexpr int LAYOUT_BUTTON_WIDTH = 180;
constexpr int LAYOUT_BUTTON_GAP = 10;
constexpr int LAYOUT_ROW_HEIGHT = 50;
constexpr int LAYOUT_BUTTON_HEIGHT = 40;
constexpr int LAYOUT_SECTION_SPACING = 45;
constexpr int LAYOUT_TITLE_START = 30;
constexpr int LAYOUT_TITLE_HEIGHT = 55;
constexpr int LAYOUT_TITLE_SPACING = 75;
constexpr int LAYOUT_SECTION_HEIGHT = 35;
constexpr int LAYOUT_SECTION_GAP = 20;
constexpr int LAYOUT_BOTTOM_BUTTON_HEIGHT = 50;
constexpr int LAYOUT_BOTTOM_BUTTON_WIDTH = 200;
constexpr int LAYOUT_BOTTOM_BUTTON_GAP = 30;
constexpr int LAYOUT_BOTTOM_SPACING = 70;
constexpr int LAYOUT_IMPRINT_HEIGHT = 22;
constexpr int LAYOUT_IMPRINT_SPACING = 24;
constexpr int LAYOUT_ROW_PADDING = 10;
constexpr int LAYOUT_LINE_WIDTH = 620;
constexpr int WINDOW_WIDTH = 720;
constexpr int LAYOUT_FONT_BUTTON_WIDTH = 120;
constexpr int LAYOUT_FONT_BUTTON_HEIGHT = 35;

// --- THEME COLORS ---
constexpr COLORREF COLOR_ELDEN_BG = RGB(25, 20, 15);
constexpr COLORREF COLOR_ELDEN_TEXT = RGB(220, 195, 145);
constexpr COLORREF COLOR_ELDEN_ACCENT = RGB(255, 215, 120);
constexpr COLORREF COLOR_ELDEN_BUTTON = RGB(40, 35, 25);
constexpr COLORREF COLOR_ELDEN_BORDER = RGB(120, 100, 60);
constexpr COLORREF COLOR_ELDEN_EXIT = RGB(180, 50, 50);
constexpr COLORREF COLOR_ELDEN_EXIT_BORDER = RGB(200, 80, 80);
constexpr COLORREF COLOR_ELDEN_EXIT_TEXT = RGB(255, 220, 220);
constexpr COLORREF COLOR_ELDEN_MINIMIZE_BORDER = RGB(80, 70, 55);
constexpr COLORREF COLOR_ELDEN_ROW_BG = RGB(45, 38, 28);
constexpr COLORREF COLOR_ELDEN_SEPARATOR = RGB(140, 115, 70);

// --- GLOBAL VARIABLES ---
const wchar_t* PROCESS_1 = L"eldenring.exe";
const wchar_t* PROCESS_2 = L"ersc_launcher.exe";
const wchar_t* MUTEX_NAME = L"EldenRingSprintTool_Unique_ID";
const wchar_t* CONFIG_FILE = L".\\settings.ini";

// Thread-safe config variables
std::mutex g_configMutex;
WORD g_keyIngame = 'F';
WORD g_keyDodge = 'C';
WORD g_keySprint = VK_CAPITAL;
WORD g_keyTrigger = VK_ESCAPE;
WORD g_keyAction = 'Q';
wchar_t g_fontName[FONT_NAME_BUFFER] = L"Palatino Linotype";

// UI & State
NOTIFYICONDATA g_nid = {};
HWND g_hMainWindow = nullptr;
HWND g_hSettingsWnd = nullptr;
std::atomic<bool> g_isAppRunning(true);
std::atomic<int> g_waitingForBindID(0);
HICON g_hIconIdle = nullptr;
HICON g_hIconActive = nullptr;
HICON g_hIconExe = nullptr;
bool g_customIconsLoaded = false;

// UI Theme
HBRUSH g_hBrushBg = nullptr;
HBRUSH g_hBrushButton = nullptr;
HBRUSH g_hBrushExit = nullptr;
HFONT g_hFontTitle = nullptr;
HFONT g_hFontNormal = nullptr;
HFONT g_hFontSection = nullptr;
HFONT g_hFontImprint = nullptr;
HFONT g_hFontButton = nullptr;

// --- HELPER FUNCTIONS ---

inline HFONT CreateUIFont(int height, int weight = FW_NORMAL) {
    std::lock_guard<std::mutex> lock(g_configMutex);
    return CreateFont(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, g_fontName);
}

void RecreateAllFonts() {
    // Clean up old fonts
    if (g_hFontTitle) DeleteObject(g_hFontTitle);
    if (g_hFontSection) DeleteObject(g_hFontSection);
    if (g_hFontNormal) DeleteObject(g_hFontNormal);
    if (g_hFontButton) DeleteObject(g_hFontButton);
    if (g_hFontImprint) DeleteObject(g_hFontImprint);

    // Create new fonts
    g_hFontTitle = CreateUIFont(48, FW_BOLD);
    g_hFontSection = CreateUIFont(34, FW_SEMIBOLD);
    g_hFontNormal = CreateUIFont(28);
    g_hFontButton = CreateUIFont(24);
    g_hFontImprint = CreateUIFont(16);
}

void UpdateAllControlFonts(HWND hwnd) {
    // Update title
    SendMessage(GetDlgItem(hwnd, ID_TITLE_STATIC), WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

    // Update section headers
    SendMessage(GetDlgItem(hwnd, ID_SECTION1_STATIC), WM_SETFONT, (WPARAM)g_hFontSection, TRUE);
    SendMessage(GetDlgItem(hwnd, ID_SECTION2_STATIC), WM_SETFONT, (WPARAM)g_hFontSection, TRUE);
    SendMessage(GetDlgItem(hwnd, ID_SECTION3_STATIC), WM_SETFONT, (WPARAM)g_hFontSection, TRUE);

    // Update description labels
    SendMessage(GetDlgItem(hwnd, ID_LABEL1_STATIC), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SendMessage(GetDlgItem(hwnd, ID_LABEL2_STATIC), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SendMessage(GetDlgItem(hwnd, ID_LABEL3_STATIC), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SendMessage(GetDlgItem(hwnd, ID_LABEL4_STATIC), WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    // Update imprints
    SendMessage(GetDlgItem(hwnd, ID_IMPRINT1_STATIC), WM_SETFONT, (WPARAM)g_hFontImprint, TRUE);
    SendMessage(GetDlgItem(hwnd, ID_IMPRINT2_STATIC), WM_SETFONT, (WPARAM)g_hFontImprint, TRUE);

    // Force buttons to redraw
    InvalidateRect(GetDlgItem(hwnd, BTN_FONT_SETTINGS), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, BTN_BIND_INGAME), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, BTN_BIND_DODGE), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, BTN_BIND_SPRINT), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, BTN_BIND_TRIGGER), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, BTN_EXIT_SETTINGS), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, BTN_EXIT_APP), nullptr, TRUE);
    
    // Force complete window redraw
    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
}

struct ButtonStyle {
    HBRUSH brush;
    COLORREF borderColor;
    COLORREF textColor;
    HFONT font;
};

ButtonStyle GetButtonStyle(UINT ctlID) {
    if (ctlID == BTN_EXIT_APP) {
        return { g_hBrushExit, COLOR_ELDEN_EXIT_BORDER, COLOR_ELDEN_EXIT_TEXT, g_hFontSection };
    }
    if (ctlID == BTN_EXIT_SETTINGS) {
        return { g_hBrushButton, COLOR_ELDEN_MINIMIZE_BORDER, COLOR_ELDEN_TEXT, g_hFontSection };
    }
    if (ctlID == BTN_FONT_SETTINGS) {
        return { g_hBrushButton, COLOR_ELDEN_BORDER, COLOR_ELDEN_ACCENT, g_hFontButton };
    }
    return { g_hBrushButton, COLOR_ELDEN_BORDER, COLOR_ELDEN_ACCENT, g_hFontButton };
}

void CleanupIcons() {
    if (g_customIconsLoaded) {
        if (g_hIconIdle) DestroyIcon(g_hIconIdle);
        if (g_hIconActive) DestroyIcon(g_hIconActive);
        if (g_hIconExe) DestroyIcon(g_hIconExe);
    }
}

void CleanupFonts() {
    if (g_hFontTitle) DeleteObject(g_hFontTitle);
    if (g_hFontSection) DeleteObject(g_hFontSection);
    if (g_hFontNormal) DeleteObject(g_hFontNormal);
    if (g_hFontButton) DeleteObject(g_hFontButton);
    if (g_hFontImprint) DeleteObject(g_hFontImprint);
}

void CleanupBrushes() {
    if (g_hBrushBg) DeleteObject(g_hBrushBg);
    if (g_hBrushButton) DeleteObject(g_hBrushButton);
    if (g_hBrushExit) DeleteObject(g_hBrushExit);
}

HMENU CreateTrayMenu() {
    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
        AppendMenu(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Settings");
        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
    }
    return hMenu;
}

void CycleFontToNext() {
    wchar_t currentFont[FONT_NAME_BUFFER];
    const wchar_t* newFontName = nullptr;

    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        StringCchCopy(currentFont, ARRAYSIZE(currentFont), g_fontName);
    }

    // Cycle through fonts
    if (_wcsicmp(currentFont, L"Palatino Linotype") == 0) {
        newFontName = L"Segoe UI";
    }
    else if (_wcsicmp(currentFont, L"Segoe UI") == 0) {
        newFontName = L"Comic Sans MS";
    }
    else {
        newFontName = L"Palatino Linotype";
    }

    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        StringCchCopy(g_fontName, ARRAYSIZE(g_fontName), newFontName);
    }
}

// --- CONFIG MANAGEMENT ---

void SaveConfig() {
    WORD localIngame, localDodge, localSprint, localTrigger, localAction;
    wchar_t localFontName[FONT_NAME_BUFFER];
    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        localIngame = g_keyIngame;
        localDodge = g_keyDodge;
        localSprint = g_keySprint;
        localTrigger = g_keyTrigger;
        localAction = g_keyAction;
        StringCchCopy(localFontName, ARRAYSIZE(localFontName), g_fontName);
    }

    wchar_t buffer[CONFIG_BUFFER];

    StringCchPrintf(buffer, ARRAYSIZE(buffer), L"%d", localIngame);
    WritePrivateProfileString(L"Keys", L"IngameKey", buffer, CONFIG_FILE);

    StringCchPrintf(buffer, ARRAYSIZE(buffer), L"%d", localDodge);
    WritePrivateProfileString(L"Keys", L"DodgeKey", buffer, CONFIG_FILE);

    StringCchPrintf(buffer, ARRAYSIZE(buffer), L"%d", localSprint);
    WritePrivateProfileString(L"Keys", L"SprintKey", buffer, CONFIG_FILE);

    StringCchPrintf(buffer, ARRAYSIZE(buffer), L"%d", localTrigger);
    WritePrivateProfileString(L"Keys", L"TriggerKey", buffer, CONFIG_FILE);

    StringCchPrintf(buffer, ARRAYSIZE(buffer), L"%d", localAction);
    WritePrivateProfileString(L"Keys", L"ActionKey", buffer, CONFIG_FILE);

    WritePrivateProfileString(L"UI", L"FontName", localFontName, CONFIG_FILE);
}

inline bool IsValidKey(WORD key) {
    return key > 0 && key <= 0xFF;
}

void LoadConfig() {
    WORD tempIngame = static_cast<WORD>(GetPrivateProfileInt(L"Keys", L"IngameKey", 'F', CONFIG_FILE));
    WORD tempDodge = static_cast<WORD>(GetPrivateProfileInt(L"Keys", L"DodgeKey", 'C', CONFIG_FILE));
    WORD tempSprint = static_cast<WORD>(GetPrivateProfileInt(L"Keys", L"SprintKey", VK_CAPITAL, CONFIG_FILE));
    WORD tempTrigger = static_cast<WORD>(GetPrivateProfileInt(L"Keys", L"TriggerKey", VK_ESCAPE, CONFIG_FILE));
    WORD tempAction = static_cast<WORD>(GetPrivateProfileInt(L"Keys", L"ActionKey", 'Q', CONFIG_FILE));
    
    wchar_t tempFontName[FONT_NAME_BUFFER];
    GetPrivateProfileString(L"UI", L"FontName", L"Palatino Linotype", tempFontName, ARRAYSIZE(tempFontName), CONFIG_FILE);

    std::lock_guard<std::mutex> lock(g_configMutex);
    if (IsValidKey(tempIngame)) g_keyIngame = tempIngame;
    if (IsValidKey(tempDodge)) g_keyDodge = tempDodge;
    if (IsValidKey(tempSprint)) g_keySprint = tempSprint;
    if (IsValidKey(tempTrigger)) g_keyTrigger = tempTrigger;
    if (IsValidKey(tempAction)) g_keyAction = tempAction;
    StringCchCopy(g_fontName, ARRAYSIZE(g_fontName), tempFontName);
}

// --- KEY NAMING ---

void GetKeyName(WORD vk, wchar_t* buffer, size_t size) {
    switch (vk) {
    case VK_LEFT:    StringCchCopy(buffer, size, L"Left Arrow"); return;
    case VK_RIGHT:   StringCchCopy(buffer, size, L"Right Arrow"); return;
    case VK_UP:      StringCchCopy(buffer, size, L"Up Arrow"); return;
    case VK_DOWN:    StringCchCopy(buffer, size, L"Down Arrow"); return;
    case VK_CAPITAL: StringCchCopy(buffer, size, L"Caps Lock"); return;
    case VK_ESCAPE:  StringCchCopy(buffer, size, L"Escape"); return;
    case VK_SPACE:   StringCchCopy(buffer, size, L"Space"); return;
    case VK_RETURN:  StringCchCopy(buffer, size, L"Enter"); return;
    case VK_TAB:     StringCchCopy(buffer, size, L"Tab"); return;
    case VK_SHIFT:   StringCchCopy(buffer, size, L"Shift"); return;
    case VK_CONTROL: StringCchCopy(buffer, size, L"Ctrl"); return;
    case VK_MENU:    StringCchCopy(buffer, size, L"Alt"); return;
    }

    UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    if (vk >= VK_PRIOR && vk <= VK_HELP) {
        scanCode |= 0x100;
    }

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

bool IsGameRunning() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool found = false;
    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, PROCESS_1) == 0 || _wcsicmp(pe.szExeFile, PROCESS_2) == 0) {
                found = true;
                break;
            }
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
        input.ki.wVk = vk;
        input.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
    }
    else {
        input.ki.wScan = static_cast<WORD>(scanCode);
        input.ki.dwFlags = KEYEVENTF_SCANCODE | (keyUp ? KEYEVENTF_KEYUP : 0);
    }

    SendInput(1, &input, sizeof(INPUT));
}

inline void PressKey(WORD vk) {
    SendKeyInput(vk, false);
}

inline void ReleaseKey(WORD vk) {
    SendKeyInput(vk, true);
}

inline bool IsKeyDown(WORD vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void SetTrayIconState(bool active) {
    g_nid.hIcon = active ? g_hIconActive : g_hIconIdle;
    StringCchCopy(g_nid.szTip, ARRAYSIZE(g_nid.szTip),
        active ? L"Elden Ring Helper: ACTIVE" : L"Elden Ring Helper: Waiting...");
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
}

// --- GAME LOGIC THREAD ---

void GameLogicThread() {
    bool sprintActive = false;
    bool dodgePressed = false;
    bool triggerDown = false;
    bool actionTriggered = false;
    ULONGLONG triggerStartTime = 0;
    bool lastGameState = false;

    while (g_isAppRunning) {
        if (g_waitingForBindID != 0) {
            Sleep(BIND_CHECK_MS);
            continue;
        }

        bool currentGameState = IsGameRunning();

        if (currentGameState != lastGameState) {
            if (!currentGameState && lastGameState && sprintActive) {
                std::lock_guard<std::mutex> lock(g_configMutex);
                ReleaseKey(g_keyIngame);
                sprintActive = false;
            }
            SetTrayIconState(currentGameState);
            lastGameState = currentGameState;
        }

        if (!currentGameState) {
            Sleep(GAME_CHECK_IDLE_MS);
            continue;
        }

        WORD localIngame, localDodge, localSprint, localTrigger, localAction;
        {
            std::lock_guard<std::mutex> lock(g_configMutex);
            localIngame = g_keyIngame;
            localDodge = g_keyDodge;
            localSprint = g_keySprint;
            localTrigger = g_keyTrigger;
            localAction = g_keyAction;
        }

        if (IsKeyDown(localDodge)) {
            if (!dodgePressed) {
                bool sprintWasActive = sprintActive;
                if (sprintActive) {
                    ReleaseKey(localIngame);
                    Sleep(SPRINT_DODGE_PAUSE_MS);
                }

                PressKey(localIngame);
                Sleep(DODGE_DURATION_MS);
                ReleaseKey(localIngame);

                if (sprintWasActive && IsKeyDown(localSprint)) {
                    Sleep(SPRINT_DODGE_PAUSE_MS);
                    PressKey(localIngame);
                    sprintActive = true;
                }
                else {
                    sprintActive = false;
                }

                dodgePressed = true;
            }
        }
        else {
            dodgePressed = false;

            if (IsKeyDown(localSprint)) {
                if (!sprintActive) {
                    PressKey(localIngame);
                    sprintActive = true;
                }
            }
            else {
                if (sprintActive) {
                    ReleaseKey(localIngame);
                    sprintActive = false;
                }
            }
        }

        if (IsKeyDown(localTrigger)) {
            if (!triggerDown) {
                triggerDown = true;
                triggerStartTime = GetTickCount64();
                actionTriggered = false;
            }
            else if (!actionTriggered) {
                ULONGLONG duration = GetTickCount64() - triggerStartTime;
                if (duration >= LONG_PRESS_DELAY_MS) {
                    PressKey(localAction);
                    Sleep(ACTION_DURATION_MS);
                    ReleaseKey(localAction);
                    actionTriggered = true;
                }
            }
        }
        else {
            triggerDown = false;
            actionTriggered = false;
        }

        Sleep(LOGIC_TICK_MS);
    }

    if (sprintActive) {
        std::lock_guard<std::mutex> lock(g_configMutex);
        ReleaseKey(g_keyIngame);
    }
}

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

        // Create fonts
        g_hFontTitle = CreateUIFont(48, FW_BOLD);
        g_hFontSection = CreateUIFont(34, FW_SEMIBOLD);
        g_hFontNormal = CreateUIFont(28);
        g_hFontButton = CreateUIFont(24);
        g_hFontImprint = CreateUIFont(16);

        int yPos = LAYOUT_TITLE_START;
        const int buttonX = LAYOUT_LEFT_MARGIN + LAYOUT_LABEL_WIDTH + LAYOUT_BUTTON_GAP;

        // Font selection button (top-left)
        CreateWindow(L"BUTTON", L"Change Font",  // Changed back from "Palatino"
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            10, 10, LAYOUT_FONT_BUTTON_WIDTH, LAYOUT_FONT_BUTTON_HEIGHT, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(BTN_FONT_SETTINGS)), nullptr, nullptr);
        
        // Title (with ID for easy updating)
        HWND hTitle = CreateWindow(L"STATIC", L"Key Bindings",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, yPos, WINDOW_WIDTH, LAYOUT_TITLE_HEIGHT, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TITLE_STATIC)), nullptr, nullptr);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);
        yPos += LAYOUT_TITLE_SPACING;

        // Section 1: Game Settings
        HWND hSection1 = CreateWindow(L"STATIC", L"Game Settings",
            WS_VISIBLE | WS_CHILD,
            LAYOUT_LEFT_MARGIN, yPos, LAYOUT_LABEL_WIDTH, LAYOUT_SECTION_HEIGHT, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SECTION1_STATIC)), nullptr, nullptr);
        SendMessage(hSection1, WM_SETFONT, (WPARAM)g_hFontSection, TRUE);
        yPos += LAYOUT_SECTION_SPACING;

        HWND hLabel1 = CreateWindow(L"STATIC", L"Backstep, Dodge Roll, Dash",
            WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
            LAYOUT_LEFT_MARGIN, yPos, LAYOUT_LABEL_WIDTH, LAYOUT_BUTTON_HEIGHT, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LABEL1_STATIC)), nullptr, nullptr);
        SendMessage(hLabel1, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        HWND btn1 = CreateWindow(L"BUTTON", L"...",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            buttonX, yPos, LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(BTN_BIND_INGAME)), nullptr, nullptr);
        yPos += LAYOUT_ROW_HEIGHT + LAYOUT_SECTION_GAP;

        // Section 2: User Overrides
        HWND hSection2 = CreateWindow(L"STATIC", L"User Overrides",
            WS_VISIBLE | WS_CHILD,
            LAYOUT_LEFT_MARGIN, yPos, LAYOUT_LABEL_WIDTH, LAYOUT_SECTION_HEIGHT, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SECTION2_STATIC)), nullptr, nullptr);
        SendMessage(hSection2, WM_SETFONT, (WPARAM)g_hFontSection, TRUE);
        yPos += LAYOUT_SECTION_SPACING;

        HWND hLabel2 = CreateWindow(L"STATIC", L"Dodge Roll (Custom Key)",
            WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
            LAYOUT_LEFT_MARGIN, yPos, LAYOUT_LABEL_WIDTH, LAYOUT_BUTTON_HEIGHT, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LABEL2_STATIC)), nullptr, nullptr);
        SendMessage(hLabel2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        HWND btn2 = CreateWindow(L"BUTTON", L"...",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            buttonX, yPos, LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(BTN_BIND_DODGE)), nullptr, nullptr);
        yPos += LAYOUT_ROW_HEIGHT;

        HWND hLabel3 = CreateWindow(L"STATIC", L"Dash, Sprint (Custom Key)",
            WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
            LAYOUT_LEFT_MARGIN, yPos, LAYOUT_LABEL_WIDTH, LAYOUT_BUTTON_HEIGHT, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LABEL3_STATIC)), nullptr, nullptr);
        SendMessage(hLabel3, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        HWND btn3 = CreateWindow(L"BUTTON", L"...",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            buttonX, yPos, LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(BTN_BIND_SPRINT)), nullptr, nullptr);
        yPos += LAYOUT_ROW_HEIGHT + LAYOUT_SECTION_GAP;

        // Section 3: Advanced
        HWND hSection3 = CreateWindow(L"STATIC", L"Advanced",
            WS_VISIBLE | WS_CHILD,
            LAYOUT_LEFT_MARGIN, yPos, LAYOUT_LABEL_WIDTH, LAYOUT_SECTION_HEIGHT, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SECTION3_STATIC)), nullptr, nullptr);
        SendMessage(hSection3, WM_SETFONT, (WPARAM)g_hFontSection, TRUE);
        yPos += LAYOUT_SECTION_SPACING;

        HWND hLabel4 = CreateWindow(L"STATIC", L"Close, Back (Long Press Trigger)",
            WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
            LAYOUT_LEFT_MARGIN, yPos, LAYOUT_LABEL_WIDTH, LAYOUT_BUTTON_HEIGHT, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LABEL4_STATIC)), nullptr, nullptr);
        SendMessage(hLabel4, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        HWND btn4 = CreateWindow(L"BUTTON", L"...",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            buttonX, yPos, LAYOUT_BUTTON_WIDTH, LAYOUT_BUTTON_HEIGHT, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(BTN_BIND_TRIGGER)), nullptr, nullptr);
        yPos += LAYOUT_ROW_HEIGHT + LAYOUT_SECTION_GAP;

        // Bottom buttons
        const int totalButtonWidth = LAYOUT_BOTTOM_BUTTON_WIDTH * 2;
        const int buttonsStartX = (WINDOW_WIDTH - totalButtonWidth - LAYOUT_BOTTOM_BUTTON_GAP) / 2;

        CreateWindow(L"BUTTON", L"Minimize",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            buttonsStartX, yPos, LAYOUT_BOTTOM_BUTTON_WIDTH, LAYOUT_BOTTOM_BUTTON_HEIGHT, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(BTN_EXIT_SETTINGS)), nullptr, nullptr);

        CreateWindow(L"BUTTON", L"Exit",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            buttonsStartX + LAYOUT_BOTTOM_BUTTON_WIDTH + LAYOUT_BOTTOM_BUTTON_GAP, yPos, 
            LAYOUT_BOTTOM_BUTTON_WIDTH, LAYOUT_BOTTOM_BUTTON_HEIGHT, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(BTN_EXIT_APP)), nullptr, nullptr);

        yPos += LAYOUT_BOTTOM_SPACING;

        // Imprint
        HWND hImprint1 = CreateWindow(L"STATIC", L"Idea and development: Börni (burni2001)",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, yPos, WINDOW_WIDTH, LAYOUT_IMPRINT_HEIGHT, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_IMPRINT1_STATIC)), nullptr, nullptr);
        SendMessage(hImprint1, WM_SETFONT, (WPARAM)g_hFontImprint, TRUE);

        yPos += LAYOUT_IMPRINT_SPACING;
        HWND hImprint2 = CreateWindow(L"STATIC", L"Development tools: Visual Studio, GitHub Copilot",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, yPos, WINDOW_WIDTH, LAYOUT_IMPRINT_HEIGHT, hwnd, 
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_IMPRINT2_STATIC)), nullptr, nullptr);
        SendMessage(hImprint2, WM_SETFONT, (WPARAM)g_hFontImprint, TRUE);

        // Update button texts
        std::lock_guard<std::mutex> lock(g_configMutex);
        UpdateButtonText(btn1, g_keyIngame);
        UpdateButtonText(btn2, g_keyDodge);
        UpdateButtonText(btn3, g_keySprint);
        UpdateButtonText(btn4, g_keyTrigger);
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HBRUSH hRowBrush = CreateSolidBrush(COLOR_ELDEN_ROW_BG);
        HPEN hLinePen = CreatePen(PS_SOLID, 2, COLOR_ELDEN_SEPARATOR);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hLinePen);

        // Calculate positions based on layout constants
        int ySection1 = LAYOUT_TITLE_START + LAYOUT_TITLE_SPACING;
        int yRow1 = ySection1 + LAYOUT_SECTION_SPACING;
        int ySection2 = yRow1 + LAYOUT_ROW_HEIGHT + LAYOUT_SECTION_GAP;
        int yRow2 = ySection2 + LAYOUT_SECTION_SPACING;
        int yRow3 = yRow2 + LAYOUT_ROW_HEIGHT;
        int ySection3 = yRow3 + LAYOUT_ROW_HEIGHT + LAYOUT_SECTION_GAP;
        int yRow4 = ySection3 + LAYOUT_SECTION_SPACING;

        // Section 1 separator
        MoveToEx(hdc, LAYOUT_LEFT_MARGIN, ySection1 + LAYOUT_SECTION_HEIGHT, nullptr);
        LineTo(hdc, LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH, ySection1 + LAYOUT_SECTION_HEIGHT);

        // Row 1 background
        RECT row1 = { 
            LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING, 
            yRow1, 
            LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH + LAYOUT_ROW_PADDING, 
            yRow1 + LAYOUT_BUTTON_HEIGHT 
        };
        FillRect(hdc, &row1, hRowBrush);

        // Section 2 separator
        MoveToEx(hdc, LAYOUT_LEFT_MARGIN, ySection2 + LAYOUT_SECTION_HEIGHT, nullptr);
        LineTo(hdc, LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH, ySection2 + LAYOUT_SECTION_HEIGHT);

        // Row 2 background
        RECT row2 = { 
            LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING, 
            yRow2, 
            LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH + LAYOUT_ROW_PADDING, 
            yRow2 + LAYOUT_BUTTON_HEIGHT 
        };
        FillRect(hdc, &row2, hRowBrush);

        // Row 3 background
        RECT row3 = { 
            LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING, 
            yRow3, 
            LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH + LAYOUT_ROW_PADDING, 
            yRow3 + LAYOUT_BUTTON_HEIGHT 
        };
        FillRect(hdc, &row3, hRowBrush);

        // Section 3 separator
        MoveToEx(hdc, LAYOUT_LEFT_MARGIN, ySection3 + LAYOUT_SECTION_HEIGHT, nullptr);
        LineTo(hdc, LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH, ySection3 + LAYOUT_SECTION_HEIGHT);

        // Row 4 background
        RECT row4 = { 
            LAYOUT_LEFT_MARGIN - LAYOUT_ROW_PADDING, 
            yRow4, 
            LAYOUT_LEFT_MARGIN + LAYOUT_LINE_WIDTH + LAYOUT_ROW_PADDING, 
            yRow4 + LAYOUT_BUTTON_HEIGHT 
        };
        FillRect(hdc, &row4, hRowBrush);

        SelectObject(hdc, hOldPen);
        DeleteObject(hLinePen);
        DeleteObject(hRowBrush);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, COLOR_ELDEN_TEXT);
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (pDIS->CtlType == ODT_BUTTON) {
            HDC hdc = pDIS->hDC;
            RECT rc = pDIS->rcItem;

            // Get button style
            ButtonStyle style = GetButtonStyle(pDIS->CtlID);

            // Fill background
            FillRect(hdc, &rc, style.brush);

            // Draw border
            HPEN hPen = CreatePen(PS_SOLID, 2, style.borderColor);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPen);

            // Draw text
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
        }
        else if (id == BTN_EXIT_APP) {
            if (MessageBox(hwnd, L"Are you sure you want to exit?", L"Exit", 
                          MB_YESNO | MB_ICONQUESTION) == IDYES) {
                DestroyWindow(g_hMainWindow);
            }
        }
        else if (id == BTN_FONT_SETTINGS) {
            // Cycle to next font
            CycleFontToNext();
            RecreateAllFonts();
            UpdateAllControlFonts(hwnd);
            SaveConfig();
        }
        else if (id >= BTN_BIND_INGAME && id <= BTN_BIND_TRIGGER) {
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
        
        if (bindID != 0) {
            WORD newKey = static_cast<WORD>(wParam);

            if (IsModifierOnlyKey(newKey)) {
                MessageBox(hwnd, L"Modifier keys alone are not allowed!",
                    L"Invalid Key", MB_OK | MB_ICONWARNING);
                g_waitingForBindID = 0;
                break;
            }

            {
                std::lock_guard<std::mutex> lock(g_configMutex);
                HWND hBtn = GetDlgItem(hwnd, bindID);

                switch (bindID) {
                case BTN_BIND_INGAME:
                    g_keyIngame = newKey;
                    UpdateButtonText(hBtn, g_keyIngame);
                    break;
                case BTN_BIND_DODGE:
                    g_keyDodge = newKey;
                    UpdateButtonText(hBtn, g_keyDodge);
                    break;
                case BTN_BIND_SPRINT:
                    g_keySprint = newKey;
                    UpdateButtonText(hBtn, g_keySprint);
                    break;
                case BTN_BIND_TRIGGER:
                    g_keyTrigger = newKey;
                    UpdateButtonText(hBtn, g_keyTrigger);
                    break;
                }
            }

            g_waitingForBindID = 0;
            SaveConfig();
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

bool CreateSettingsWindow(HINSTANCE hInstance) {
    g_hBrushBg = CreateSolidBrush(COLOR_ELDEN_BG);
    g_hBrushButton = CreateSolidBrush(COLOR_ELDEN_BUTTON);
    g_hBrushExit = CreateSolidBrush(COLOR_ELDEN_EXIT);

    WNDCLASS wc = {};
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"EldenSettingsClass";
    wc.hbrBackground = g_hBrushBg;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(ICON_ID_EXE));

    if (!RegisterClass(&wc)) {
        return false;
    }

    g_hSettingsWnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        L"EldenSettingsClass",
        L"Elden Ring Helper - Key Bindings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, 700,
        nullptr, nullptr, hInstance, nullptr
    );

    return g_hSettingsWnd != nullptr;
}

// --- MAIN WINDOW & TRAY ---

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_TRAYICON:
        if (lParam == WM_LBUTTONDBLCLK) {
            if (g_hSettingsWnd) {
                ShowWindow(g_hSettingsWnd, SW_SHOW);
                SetForegroundWindow(g_hSettingsWnd);
            }
        }
        else if (lParam == WM_RBUTTONUP) {
            POINT curPoint;
            GetCursorPos(&curPoint);

            SetForegroundWindow(hwnd);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

            HMENU hMenu = CreateTrayMenu();
            if (hMenu) {
                UINT uFlags = TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_BOTTOMALIGN;
                if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0) {
                    uFlags |= TPM_RIGHTALIGN;
                }
                else {
                    uFlags |= TPM_LEFTALIGN;
                }

                int cmd = TrackPopupMenuEx(hMenu, uFlags,
                    curPoint.x, curPoint.y, hwnd, nullptr);

                DestroyMenu(hMenu);
                SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

                if (cmd == ID_TRAY_SETTINGS) {
                    if (g_hSettingsWnd) {
                        ShowWindow(g_hSettingsWnd, SW_SHOW);
                        SetForegroundWindow(g_hSettingsWnd);
                    }
                }
                else if (cmd == ID_TRAY_EXIT) {
                    DestroyWindow(hwnd);
                }

                PostMessage(hwnd, WM_NULL, 0, 0);
            }
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT) {
            DestroyWindow(hwnd);
        }
        else if (LOWORD(wParam) == ID_TRAY_SETTINGS) {
            if (g_hSettingsWnd) {
                ShowWindow(g_hSettingsWnd, SW_SHOW);
                SetForegroundWindow(g_hSettingsWnd);
            }
        }
        break;

    case WM_DESTROY:
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

// --- SYSTEM HELPER FUNCTIONS ---

bool WaitForShellReady(int maxWaitSeconds = SHELL_TIMEOUT_SEC) {
    ULONGLONG startTime = GetTickCount64();
    ULONGLONG timeout = static_cast<ULONGLONG>(maxWaitSeconds) * 1000ULL;

    while ((GetTickCount64() - startTime) < timeout) {
        if (FindWindow(L"Shell_TrayWnd", nullptr) != nullptr) {
            return true;
        }
        Sleep(SHELL_CHECK_INTERVAL_MS);
    }
    return false;
}

bool AddTrayIconWithRetry(PNOTIFYICONDATA pnid, int maxRetries = TRAY_MAX_RETRIES) {
    for (int i = 0; i < maxRetries; i++) {
        if (Shell_NotifyIcon(NIM_ADD, pnid)) {
            return true;
        }
        Sleep(TRAY_RETRY_INTERVAL_MS);
    }
    return false;
}

inline void SafeCloseMutex(HANDLE& hMutex) {
    if (hMutex != nullptr) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        hMutex = nullptr;
    }
}

// --- MAIN ENTRY POINT ---

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    EnableWindows11DarkMode();

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (hMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex != nullptr) {
            CloseHandle(hMutex);
        }
        return 0;
    }

    LoadConfig();
    WaitForShellReady();

    // Load all icons
    HICON g_hIconExe = LoadIcon(hInstance, MAKEINTRESOURCE(ICON_ID_EXE));
    g_hIconIdle = LoadIcon(hInstance, MAKEINTRESOURCE(ICON_ID_IDLE));
    g_hIconActive = LoadIcon(hInstance, MAKEINTRESOURCE(ICON_ID_ACTIVE));

    if (g_hIconIdle && g_hIconActive) {
        g_customIconsLoaded = true;
    }
    else {
        g_hIconIdle = LoadIcon(nullptr, IDI_APPLICATION);
        g_hIconActive = LoadIcon(nullptr, IDI_SHIELD);
    }

    if (!CreateSettingsWindow(hInstance)) {
        SafeCloseMutex(hMutex);
        return -1;
    }

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"EldenRingTrayClass";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(ICON_ID_EXE));

    if (!RegisterClass(&wc)) {
        SafeCloseMutex(hMutex);
        return -1;
    }

    g_hMainWindow = CreateWindowEx(
        0,
        wc.lpszClassName,
        L"EldenRingHelper",
        0, 0, 0, 0, 0,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hMainWindow) {
        SafeCloseMutex(hMutex);
        return -1;
    }

    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = g_hMainWindow;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_hIconIdle;
    StringCchCopy(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"Elden Ring Helper");

    AddTrayIconWithRetry(&g_nid);

    std::thread worker(GameLogicThread);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_isAppRunning = false;
    if (worker.joinable()) {
        worker.join();
    }

    SafeCloseMutex(hMutex);

    return static_cast<int>(msg.wParam);
}