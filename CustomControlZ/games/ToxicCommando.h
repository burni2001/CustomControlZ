#pragma once
#include "../GameProfiles.h"

// Binding indices for Toxic Commando
enum ToxicCommandoBinding {
    TC_KEY_SCROLL_UP   = 0, // Key that simulates scroll wheel up (next weapon)
    TC_KEY_SCROLL_DOWN = 1, // Key that simulates scroll wheel down (prev weapon)
    TC_BINDING_COUNT   = 2
};

constexpr int TC_LOGIC_TICK_MS       = 10;
constexpr int TC_GAME_CHECK_IDLE_MS  = 1000;
constexpr DWORD TC_SCROLL_DELTA_UP   = 120;
constexpr DWORD TC_SCROLL_DELTA_DOWN = static_cast<DWORD>(-120);

// Forward declarations of helpers defined in CustomControlZ.cpp
bool IsGameRunning(GameProfile* profile);
void SetTrayIconState(bool active, GameProfile* profile);
inline bool IsKeyDown(WORD vk);
extern std::mutex g_configMutex;
extern std::atomic<int> g_waitingForBindID;

static void ToxicCommandoLogicThread(GameProfile* profile, std::atomic<bool>& running) {
    bool scrollUpPressed   = false;
    bool scrollDownPressed = false;
    bool lastGameState     = false;

    while (running) {
        if (g_waitingForBindID != 0) {
            Sleep(TC_LOGIC_TICK_MS);
            continue;
        }

        bool currentGameState = IsGameRunning(profile);

        if (currentGameState != lastGameState) {
            SetTrayIconState(currentGameState, profile);
            lastGameState = currentGameState;
        }

        if (!currentGameState) {
            Sleep(TC_GAME_CHECK_IDLE_MS);
            continue;
        }

        WORD keyUp, keyDown;
        {
            std::lock_guard<std::mutex> lock(g_configMutex);
            keyUp   = profile->bindings[TC_KEY_SCROLL_UP].currentVk;
            keyDown = profile->bindings[TC_KEY_SCROLL_DOWN].currentVk;
        }

        // Scroll up (next weapon) - edge triggered: fires once per key press
        if (IsKeyDown(keyUp)) {
            if (!scrollUpPressed) {
                scrollUpPressed = true;
                INPUT input = {};
                input.type = INPUT_MOUSE;
                input.mi.dwFlags = MOUSEEVENTF_WHEEL;
                input.mi.mouseData = TC_SCROLL_DELTA_UP;
                SendInput(1, &input, sizeof(INPUT));
            }
        } else {
            scrollUpPressed = false;
        }

        // Scroll down (prev weapon) - edge triggered
        if (IsKeyDown(keyDown)) {
            if (!scrollDownPressed) {
                scrollDownPressed = true;
                INPUT input = {};
                input.type = INPUT_MOUSE;
                input.mi.dwFlags = MOUSEEVENTF_WHEEL;
                input.mi.mouseData = TC_SCROLL_DELTA_DOWN;
                SendInput(1, &input, sizeof(INPUT));
            }
        } else {
            scrollDownPressed = false;
        }

        Sleep(TC_LOGIC_TICK_MS);
    }
}

static GameProfile g_ToxicCommandoProfile = {
    /* id            */ L"ToxicCommando",
    /* displayName   */ L"Toxic Commando",
    /* iniSection    */ L"ToxicCommando",
    /* processName1  */ L"ToxicCommando.exe",  // UPDATE with real process name before release
    /* processName2  */ nullptr,
    /* tipActive     */ L"CustomControlZ - Toxic Commando: ACTIVE",
    /* tipIdle       */ L"CustomControlZ: Waiting...",
    /* settingsTitle */ L"Toxic Commando - Key Bindings",
    /* theme */ {
        RGB(30, 50, 20),    // bg         - dark olive
        RGB(180, 200, 140), // text        - light olive/sage
        RGB(100, 180, 50),  // accent      - bright military green
        RGB(45, 65, 30),    // button
        RGB(70, 110, 40),   // border
        RGB(40, 60, 25),    // rowBg
        RGB(85, 130, 50),   // separator
        RGB(160, 40, 40),   // exitFill
        RGB(190, 70, 70),   // exitBorder
        RGB(255, 210, 210), // exitText
        RGB(60, 90, 40),    // minimizeBorder
    },
    /* bindingCount */ TC_BINDING_COUNT,
    /* bindings */ {
        { L"ScrollUpKey",   L"Next Weapon (Scroll Up Key)",   VK_PRIOR, VK_PRIOR }, // Page Up default
        { L"ScrollDownKey", L"Prev Weapon (Scroll Down Key)", VK_NEXT,  VK_NEXT  }, // Page Down default
    },
    /* logicFn */ ToxicCommandoLogicThread,
};
