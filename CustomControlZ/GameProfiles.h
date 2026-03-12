#pragma once
#include <windows.h>
#include <functional>
#include <atomic>

// Maximum configurable key bindings per game
constexpr int MAX_BINDINGS = 8;

// BehaviorEngine.h is included here so KeyBinding can carry a BehaviorDescriptor.
// BehaviorEngine.h does NOT include GameProfiles.h (uses forward declaration only)
// to keep the include chain acyclic: GameProfiles.h -> BehaviorEngine.h.
#include "BehaviorEngine.h"

struct KeyBinding {
    const wchar_t*     iniKey;    // Key name in settings.ini, e.g. L"SprintKey"
    const wchar_t*     label;     // UI label shown next to the button
    WORD               defaultVk; // Default virtual key code
    WORD               currentVk; // Live value - written by UI, read by logic thread
    BehaviorDescriptor behavior;  // Behavior type and parameters for this binding
};

struct Theme {
    COLORREF bg;             // Window background
    COLORREF text;           // Static text color
    COLORREF accent;         // Button text / accent highlight
    COLORREF button;         // Button fill
    COLORREF border;         // Normal button border
    COLORREF rowBg;          // Row background highlight
    COLORREF separator;      // Section separator line
    COLORREF exitFill;       // Exit button background
    COLORREF exitBorder;     // Exit button border
    COLORREF exitText;       // Exit button text
    COLORREF minimizeBorder; // Minimize button border
};

struct GameProfile;
using LogicThreadFn = std::function<void(GameProfile*, std::atomic<bool>&)>;

struct GameProfile {
    const wchar_t* id;           // Short unique id, e.g. L"EldenRing"
    const wchar_t* displayName;  // UI display name, e.g. L"Elden Ring"
    const wchar_t* iniSection;   // Section name in settings.ini
    const wchar_t* processName1; // Primary process name for detection
    const wchar_t* processName2; // Secondary process name (nullable)
    const wchar_t* tipActive;    // Tray tooltip when game is running
    const wchar_t* tipIdle;      // Tray tooltip when game is not running
    const wchar_t* settingsTitle;// Settings window title bar text
    Theme          theme;
    int            bindingCount;
    KeyBinding     bindings[MAX_BINDINGS];
    LogicThreadFn  logicFn;
};
