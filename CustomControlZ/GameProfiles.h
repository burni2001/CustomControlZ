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

// --- GENERIC LOGIC THREAD IMPLEMENTATION ---
// Defined here (after GameProfile) so the body can dereference GameProfile members.
// Declared in BehaviorEngine.h.
inline void GenericLogicThreadFn(GameProfile* profile, std::atomic<bool>& running) {
    BindingState state[MAX_BINDINGS];    // zero-initialised via BindingState()
    KeyTracker   tracker;
    bool         lastGameState = false;

    while (running) {
        // Pause while the UI is capturing a new key binding
        if (g_waitingForBindID != 0) {
            Sleep(50);
            continue;
        }

        bool currentGameState = IsGameRunning(profile);

        if (currentGameState != lastGameState) {
            if (lastGameState && !currentGameState) {
                // Game just stopped: release all held outputs
                tracker.releaseAll();
                SetTrayIconState(false, profile);
            } else {
                // Game just started
                SetTrayIconState(true, profile);

                // Check if game is running elevated (FIX-02: UIPI detection)
                bool elevated = IsProcessRunningElevated(profile->processName1);
                if (!elevated && profile->processName2)
                    elevated = IsProcessRunningElevated(profile->processName2);
                if (elevated) {
                    MessageBox(nullptr,
                        L"Warning: The game is running as Administrator.\n\n"
                        L"CustomControlZ cannot inject inputs into elevated processes without "
                        L"also running as Administrator (UIPI restriction).\n\n"
                        L"Key remapping may not work. Run CustomControlZ as Administrator "
                        L"if you need remapping to function.",
                        L"Elevation Warning \u2014 UIPI Detected",
                        MB_OK | MB_ICONWARNING);
                }
            }
            lastGameState = currentGameState;
        }

        if (!currentGameState) {
            Sleep(1000);
            continue;
        }

        // Snapshot VKs under mutex to avoid data races with UI thread
        WORD localVk[MAX_BINDINGS] = {};
        {
            std::lock_guard<std::mutex> lock(g_configMutex);
            for (int i = 0; i < profile->bindingCount; i++) {
                localVk[i] = profile->bindings[i].currentVk;
            }
        }

        // Per-binding dispatch
        for (int i = 0; i < profile->bindingCount; i++) {
            const BehaviorDescriptor& desc = profile->bindings[i].behavior;
            bool keyDown = IsKeyDown(localVk[i]);

            switch (desc.type) {

            case BehaviorType::HoldToToggle: {
                HoldToToggleState& s = state[i].holdToggle;
                if (keyDown && !s.active) {
                    PressKey(desc.outputVk);
                    tracker.press(desc.outputVk);
                    s.active = true;
                } else if (!keyDown && s.active) {
                    ReleaseKey(desc.outputVk);
                    tracker.release(desc.outputVk);
                    s.active = false;
                }
                break;
            }

            case BehaviorType::EdgeTrigger: {
                EdgeTriggerState& s = state[i].edgeTrigger;
                if (keyDown && !s.fired) {
                    PressKey(desc.outputVk);
                    tracker.press(desc.outputVk);
                    Sleep(desc.durationMs);
                    ReleaseKey(desc.outputVk);
                    tracker.release(desc.outputVk);
                    s.fired = true;
                } else if (!keyDown) {
                    s.fired = false;
                }
                break;
            }

            case BehaviorType::LongPress: {
                LongPressState& s = state[i].longPress;
                if (keyDown) {
                    if (!s.keyDown) {
                        // Rising edge: record press start
                        s.pressTime = GetTickCount64();
                        s.keyDown   = true;
                        s.longFired = false;
                    } else if (!s.longFired) {
                        // Still held: check if threshold elapsed
                        ULONGLONG elapsed = GetTickCount64() - s.pressTime;
                        if ((int)elapsed >= desc.thresholdMs) {
                            PressKey(desc.longOutputVk);
                            tracker.press(desc.longOutputVk);
                            Sleep(30);
                            ReleaseKey(desc.longOutputVk);
                            tracker.release(desc.longOutputVk);
                            s.longFired = true;
                        }
                    }
                } else {
                    if (s.keyDown) {
                        // Falling edge
                        if (!s.longFired) {
                            // Tap: fire short output
                            PressKey(desc.outputVk);
                            tracker.press(desc.outputVk);
                            Sleep(30);
                            ReleaseKey(desc.outputVk);
                            tracker.release(desc.outputVk);
                        }
                        s.keyDown   = false;
                        s.longFired = false;
                    }
                }
                break;
            }

            case BehaviorType::WheelToKey: {
                WheelToKeyState& s = state[i].wheelToKey;
                if (keyDown && !s.pressed) {
                    INPUT inp = {};
                    inp.type         = INPUT_MOUSE;
                    inp.mi.dwFlags   = MOUSEEVENTF_WHEEL;
                    inp.mi.mouseData = desc.wheelDelta;
                    SendInput(1, &inp, sizeof(INPUT));
                    s.pressed = true;
                } else if (!keyDown) {
                    s.pressed = false;
                }
                break;
            }

            case BehaviorType::WeaponCombo: {
                WeaponComboState& s = state[i].weaponCombo;
                if (keyDown) {
                    if (!s.keyDown) {
                        // Rising edge: start timing
                        s.pressTime    = GetTickCount64();
                        s.keyDown      = true;
                        s.thresholdHit = false;
                        s.holding      = false;
                    } else if (!s.thresholdHit) {
                        // Still held: check if long-press threshold reached
                        ULONGLONG elapsed = GetTickCount64() - s.pressTime;
                        if ((int)elapsed >= desc.thresholdMs) {
                            s.thresholdHit = true;
                            // Switch to weapon (keep held), then start holding attack button.
                            // outputVk stays pressed until the falling edge cleans up.
                            PressKey(desc.outputVk);
                            Sleep(desc.durationMs);
                            PressMouse(desc.attackVk);
                            s.holding = true;
                        }
                    }
                } else {
                    if (s.keyDown) {
                        // Falling edge
                        if (s.holding) {
                            // Long press: release attack, then release weapon key, then switch back
                            ReleaseMouse(desc.attackVk);
                            Sleep(desc.durationMs);
                            ReleaseKey(desc.outputVk);
                            Sleep(desc.durationMs);
                            PressKey(desc.longOutputVk);
                            Sleep(desc.durationMs);
                            ReleaseKey(desc.longOutputVk);
                        } else if (!s.thresholdHit) {
                            // Tap: press weapon key, wait, click attack while key is still held,
                            // then release weapon key, then switch back explicitly.
                            PressKey(desc.outputVk);
                            Sleep(desc.durationMs);
                            PressMouse(desc.attackVk);
                            Sleep(50);
                            ReleaseMouse(desc.attackVk);
                            Sleep(desc.durationMs);
                            ReleaseKey(desc.outputVk);
                            Sleep(desc.durationMs);
                            PressKey(desc.longOutputVk);
                            Sleep(desc.durationMs);
                            ReleaseKey(desc.longOutputVk);
                        }
                        s.keyDown      = false;
                        s.thresholdHit = false;
                        s.holding      = false;
                    }
                }
                break;
            }

            case BehaviorType::WheelToggle: {
                WheelToggleState& s = state[i].wheelToggle;
                if (keyDown && !s.pressed) {
                    INPUT inp = {};
                    inp.type         = INPUT_MOUSE;
                    inp.mi.dwFlags   = MOUSEEVENTF_WHEEL;
                    inp.mi.mouseData = s.scrollUp ? desc.wheelDelta : static_cast<DWORD>(-(int)desc.wheelDelta);
                    SendInput(1, &inp, sizeof(INPUT));
                    s.scrollUp = !s.scrollUp;
                    s.pressed  = true;
                } else if (!keyDown) {
                    s.pressed = false;
                }
                break;
            }

            case BehaviorType::KeyToggle: {
                KeyToggleState& s = state[i].keyToggle;
                if (keyDown && !s.pressed) {
                    WORD vk = s.useAlt ? desc.longOutputVk : desc.outputVk;
                    PressKey(vk);
                    Sleep(desc.durationMs);
                    ReleaseKey(vk);
                    s.useAlt  = !s.useAlt;
                    s.pressed = true;
                } else if (!keyDown) {
                    s.pressed = false;
                }
                break;
            }

            case BehaviorType::MeleeBurst: {
                MeleeBurstState& s = state[i].meleeBurst;

                if (keyDown && !s.keyDown) {
                    // Rising edge
                    s.keyDown      = true;
                    s.thresholdHit = false;
                    s.pressTime    = GetTickCount64();

                    if (!s.inMelee) {
                        PressKey(desc.outputVk);   // press ^ — keep held throughout burst
                        Sleep(desc.durationMs);
                        s.inMelee = true;
                    }
                    s.lastPressTime = GetTickCount64();

                } else if (keyDown && s.keyDown && !s.thresholdHit) {
                    // Held: check long-press threshold
                    ULONGLONG elapsed = GetTickCount64() - s.pressTime;
                    if ((int)elapsed >= desc.thresholdMs) {
                        s.thresholdHit = true;
                        PressMouse(desc.attackVk);
                        s.holding = true;
                    }

                } else if (!keyDown && s.keyDown) {
                    // Falling edge
                    if (s.holding) {
                        ReleaseMouse(desc.attackVk);
                        s.holding = false;
                    } else if (!s.thresholdHit) {
                        // Tap: single click
                        PressMouse(desc.attackVk);
                        Sleep(50);
                        ReleaseMouse(desc.attackVk);
                    }
                    s.keyDown      = false;
                    s.thresholdHit = false;
                    s.lastPressTime = GetTickCount64();
                }

                // Return timer: auto-switch back after idle
                if (s.inMelee && !s.keyDown) {
                    ULONGLONG elapsed = GetTickCount64() - s.lastPressTime;
                    if ((int)elapsed >= desc.returnDelayMs) {
                        ReleaseKey(desc.outputVk);    // release ^
                        Sleep(30);
                        PressKey(desc.longOutputVk);  // press 2
                        Sleep(30);
                        ReleaseKey(desc.longOutputVk);
                        s.inMelee = false;
                    }
                }
                break;
            }

            } // switch
        }

        Sleep(10);
    }

    // Thread exiting: release any keys still held
    tracker.releaseAll();

    // Release held inputs for any WeaponCombo/MeleeBurst bindings mid-hold
    for (int i = 0; i < profile->bindingCount; i++) {
        const auto& b = profile->bindings[i].behavior;
        if (b.type == BehaviorType::WeaponCombo && state[i].weaponCombo.holding) {
            ReleaseMouse(b.attackVk);
            ReleaseKey(b.outputVk);
        }
        if (b.type == BehaviorType::MeleeBurst && state[i].meleeBurst.inMelee) {
            if (state[i].meleeBurst.holding)
                ReleaseMouse(b.attackVk);
            ReleaseKey(b.outputVk);
        }
    }
}
