#pragma once
#include "../GameProfiles.h"

// Binding indices for Elden Ring
enum EldenRingBinding {
    ER_KEY_INGAME  = 0, // The actual in-game dodge/dash key (remapped output)
    ER_KEY_DODGE   = 1, // Custom dodge trigger key
    ER_KEY_SPRINT  = 2, // Sprint hold key
    ER_KEY_TRIGGER = 3, // Long-press trigger key (holds -> fires action)
    ER_BINDING_COUNT = 4
};

// Action key sent after long-press (not user-configurable)
constexpr WORD ER_ACTION_VK = 'Q';

// Timing constants
constexpr int ER_LONG_PRESS_DELAY_MS  = 400;
constexpr int ER_LOGIC_TICK_MS        = 10;
constexpr int ER_BIND_CHECK_MS        = 50;
constexpr int ER_GAME_CHECK_IDLE_MS   = 1000;
constexpr int ER_DODGE_DURATION_MS    = 50;
constexpr int ER_SPRINT_DODGE_PAUSE_MS = 20;
constexpr int ER_ACTION_DURATION_MS   = 30;

// Forward declarations of helpers defined in CustomControlZ.cpp
bool IsGameRunning(GameProfile* profile);
void SetTrayIconState(bool active, GameProfile* profile);
inline void PressKey(WORD vk);
inline void ReleaseKey(WORD vk);
inline bool IsKeyDown(WORD vk);
extern std::mutex g_configMutex;
extern std::atomic<int> g_waitingForBindID;

static void EldenRingLogicThread(GameProfile* profile, std::atomic<bool>& running) {
    bool sprintActive    = false;
    bool dodgePressed    = false;
    bool triggerDown     = false;
    bool actionTriggered = false;
    ULONGLONG triggerStartTime = 0;
    bool lastGameState = false;

    while (running) {
        if (g_waitingForBindID != 0) {
            Sleep(ER_BIND_CHECK_MS);
            continue;
        }

        bool currentGameState = IsGameRunning(profile);

        if (currentGameState != lastGameState) {
            if (!currentGameState && lastGameState && sprintActive) {
                std::lock_guard<std::mutex> lock(g_configMutex);
                ReleaseKey(profile->bindings[ER_KEY_INGAME].currentVk);
                sprintActive = false;
            }
            SetTrayIconState(currentGameState, profile);
            lastGameState = currentGameState;
        }

        if (!currentGameState) {
            Sleep(ER_GAME_CHECK_IDLE_MS);
            continue;
        }

        WORD localIngame, localDodge, localSprint, localTrigger;
        {
            std::lock_guard<std::mutex> lock(g_configMutex);
            localIngame  = profile->bindings[ER_KEY_INGAME].currentVk;
            localDodge   = profile->bindings[ER_KEY_DODGE].currentVk;
            localSprint  = profile->bindings[ER_KEY_SPRINT].currentVk;
            localTrigger = profile->bindings[ER_KEY_TRIGGER].currentVk;
        }

        if (IsKeyDown(localDodge)) {
            if (!dodgePressed) {
                bool sprintWasActive = sprintActive;
                if (sprintActive) {
                    ReleaseKey(localIngame);
                    Sleep(ER_SPRINT_DODGE_PAUSE_MS);
                }

                PressKey(localIngame);
                Sleep(ER_DODGE_DURATION_MS);
                ReleaseKey(localIngame);

                if (sprintWasActive && IsKeyDown(localSprint)) {
                    Sleep(ER_SPRINT_DODGE_PAUSE_MS);
                    PressKey(localIngame);
                    sprintActive = true;
                } else {
                    sprintActive = false;
                }

                dodgePressed = true;
            }
        } else {
            dodgePressed = false;

            if (IsKeyDown(localSprint)) {
                if (!sprintActive) {
                    PressKey(localIngame);
                    sprintActive = true;
                }
            } else {
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
            } else if (!actionTriggered) {
                ULONGLONG duration = GetTickCount64() - triggerStartTime;
                if (duration >= ER_LONG_PRESS_DELAY_MS) {
                    PressKey(ER_ACTION_VK);
                    Sleep(ER_ACTION_DURATION_MS);
                    ReleaseKey(ER_ACTION_VK);
                    actionTriggered = true;
                }
            }
        } else {
            triggerDown     = false;
            actionTriggered = false;
        }

        Sleep(ER_LOGIC_TICK_MS);
    }

    if (sprintActive) {
        std::lock_guard<std::mutex> lock(g_configMutex);
        ReleaseKey(profile->bindings[ER_KEY_INGAME].currentVk);
    }
}

static GameProfile g_EldenRingProfile = {
    /* id            */ L"EldenRing",
    /* displayName   */ L"Elden Ring",
    /* iniSection    */ L"EldenRing",
    /* processName1  */ L"eldenring.exe",
    /* processName2  */ L"ersc_launcher.exe",
    /* tipActive     */ L"CustomControlZ - Elden Ring: ACTIVE",
    /* tipIdle       */ L"CustomControlZ: Waiting...",
    /* settingsTitle */ L"Elden Ring - Key Bindings",
    /* theme */ {
        RGB(25, 20, 15),    // bg
        RGB(220, 195, 145), // text
        RGB(255, 215, 120), // accent
        RGB(40, 35, 25),    // button
        RGB(120, 100, 60),  // border
        RGB(45, 38, 28),    // rowBg
        RGB(140, 115, 70),  // separator
        RGB(180, 50, 50),   // exitFill
        RGB(200, 80, 80),   // exitBorder
        RGB(255, 220, 220), // exitText
        RGB(80, 70, 55),    // minimizeBorder
    },
    /* bindingCount */ ER_BINDING_COUNT,
    /* bindings */ {
        { L"IngameKey",  L"Backstep, Dodge Roll, Dash",      'F',        'F'        },
        { L"DodgeKey",   L"Dodge Roll (Custom Key)",          'C',        'C'        },
        { L"SprintKey",  L"Dash, Sprint (Custom Key)",        VK_CAPITAL, VK_CAPITAL },
        { L"TriggerKey", L"Close, Back (Long Press Trigger)", VK_ESCAPE,  VK_ESCAPE  },
    },
    /* logicFn */ EldenRingLogicThread,
};
