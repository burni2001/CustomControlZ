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
    BehaviorDescriptor behavior;       // Behavior type and parameters for this binding
    bool               isAppOnly;      // true = user picks freely (no in-game counterpart); false = must match in-game key
    bool               separatorAbove; // Draw a separator line above this row in the settings UI
};

struct Theme {
    COLORREF bg;             // Window background
    COLORREF text;           // Static text color
    COLORREF accent;         // Button text / accent highlight
    COLORREF button;         // Button fill (settings window)
    COLORREF selectBg;       // Game-select card background
    COLORREF border;         // Normal button border
    COLORREF rowBg;          // Row background highlight
    COLORREF separator;      // Section separator line
    COLORREF exitFill;       // Exit button background
    COLORREF exitBorder;     // Exit button border
    COLORREF exitText;       // Exit button text
    COLORREF minimizeBorder; // Minimize button border
    bool     scanlines;      // Draw CRT scanline overlay on settings background
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
    const wchar_t* posterFile;   // Relative path to game poster PNG, e.g. L"assets\\eldenring.png"
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
    bool         lastGameState      = false;
    bool         lastForegroundState = false;
    int          missCount          = 0;   // consecutive IsGameRunning()==false count
    bool         elevationWarnShown = false; // only warn once per thread lifetime
    constexpr int MISS_THRESHOLD = 5;    // require 5 consecutive misses (~50ms) before treating game as stopped

    while (running) {
        // Pause while the UI is capturing a new key binding
        if (g_waitingForBindID != 0) {
            Sleep(50);
            continue;
        }

        bool rawGameState = IsGameRunning(profile);

        // Debounce: only flip to "stopped" after MISS_THRESHOLD consecutive misses.
        // A single transient CreateToolhelp32Snapshot failure won't disrupt operation.
        bool currentGameState;
        if (rawGameState) {
            missCount = 0;
            currentGameState = true;
        } else {
            if (missCount < MISS_THRESHOLD) ++missCount;
            currentGameState = (missCount < MISS_THRESHOLD) ? lastGameState : false;
        }

        if (currentGameState != lastGameState) {
            if (lastGameState && !currentGameState) {
                // Game just stopped: release all held outputs
                tracker.releaseAll();
                SetTrayIconState(false, profile);
            } else {
                // Game just started
                SetTrayIconState(true, profile);

                // Check if game is running elevated (FIX-02: UIPI detection).
                // Guard with elevationWarnShown so a false stopped→started transition
                // never re-shows a MessageBox that would block this thread invisibly.
                if (!elevationWarnShown) {
                    bool elevated = IsProcessRunningElevated(profile->processName1);
                    if (!elevated && profile->processName2)
                        elevated = IsProcessRunningElevated(profile->processName2);
                    if (elevated) {
                        elevationWarnShown = true;
                        MessageBox(nullptr,
                            L"Warning: The game is running as Administrator.\n\n"
                            L"CustomControlZ cannot inject inputs into elevated processes without "
                            L"also running as Administrator (UIPI restriction).\n\n"
                            L"Key remapping may not work. Run CustomControlZ as Administrator "
                            L"if you need remapping to function.",
                            L"Elevation Warning \u2014 UIPI Detected",
                            MB_OK | MB_ICONWARNING);
                    } else {
                        elevationWarnShown = true; // no need to recheck after first clean start
                    }
                }
            }
            lastGameState = currentGameState;
        }

        if (!currentGameState) {
            if (lastForegroundState) {
                // Game stopped while we thought it was in foreground — reset foreground tracking
                lastForegroundState = false;
            }
            Sleep(1000);
            continue;
        }

        // Auto-pause when game window loses foreground (e.g. user alt-tabs to Discord)
        bool currentForeground = IsGameWindowForeground(profile);
        if (currentForeground != lastForegroundState) {
            if (lastForegroundState && !currentForeground) {
                tracker.releaseAll();
                SetTrayIconState(false, profile);
            } else if (!lastForegroundState && currentForeground) {
                SetTrayIconState(true, profile);
            }
            lastForegroundState = currentForeground;
        }

        if (!currentForeground) {
            Sleep(50);
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

        // Pre-pass: handle GlobalSuspend bindings first (always active regardless of suspend state)
        bool suspended = false;
        for (int i = 0; i < profile->bindingCount; i++) {
            if (profile->bindings[i].behavior.type != BehaviorType::GlobalSuspend) continue;
            GlobalSuspendState& s = state[i].globalSuspend;
            bool keyDown = IsKeyDown(localVk[i]);
            if (keyDown && !s.pressed) {
                bool wasSuspended = s.suspended;
                s.suspended = !s.suspended;
                s.pressed   = true;
                if (s.suspended && !wasSuspended) {
                    tracker.releaseAll();              // release any held keys on entering suspend
                    SetTrayIconState(false, profile);  // idle icon = bindings off
                } else if (!s.suspended && wasSuspended) {
                    SetTrayIconState(true, profile);   // active icon = bindings back on
                }
            } else if (!keyDown) {
                s.pressed = false;
            }
            if (s.suspended) suspended = true;
        }

        // Per-binding dispatch
        for (int i = 0; i < profile->bindingCount; i++) {
            const BehaviorDescriptor& desc = profile->bindings[i].behavior;
            if (desc.type == BehaviorType::GlobalSuspend) continue; // handled above
            if (suspended) continue;                                 // all others paused
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
                bool hasTertiary = desc.includeTertiaryInCycle && desc.tertiaryOutputVk;

                // Clamp cycleIndex if tertiary was removed from cycle mid-session
                if (s.cycleIndex == 2 && !hasTertiary)
                    s.cycleIndex = 0;

                // Sync cycleIndex when the player manually presses a weapon key directly.
                // Rising edge on a weapon key → set cycleIndex to the NEXT weapon in the cycle.
                if (desc.outputVk && desc.longOutputVk) {
                    bool outDown      = IsKeyDown(desc.outputVk);
                    bool altDown      = IsKeyDown(desc.longOutputVk);
                    bool tertiaryDown = hasTertiary ? IsKeyDown(desc.tertiaryOutputVk) : false;
                    if (outDown      && !s.outWasDown)      s.cycleIndex = 1;                   // on primary → next = secondary
                    if (altDown      && !s.altWasDown)      s.cycleIndex = hasTertiary ? 2 : 0; // on secondary → next = heavy or primary
                    if (tertiaryDown && !s.tertiaryWasDown) s.cycleIndex = 0;                   // on heavy → next = primary
                    s.outWasDown      = outDown;
                    s.altWasDown      = altDown;
                    s.tertiaryWasDown = tertiaryDown;
                }

                if (keyDown && !s.pressed) {
                    WORD vk = (s.cycleIndex == 2) ? desc.tertiaryOutputVk :
                              (s.cycleIndex == 1) ? desc.longOutputVk : desc.outputVk;
                    PressKey(vk);
                    Sleep(desc.durationMs);
                    ReleaseKey(vk);
                    s.pressed = true;
                    // Prevent the key we just sent from being counted as a manual press on the next tick.
                    if (vk == desc.outputVk)         s.outWasDown      = true;
                    if (vk == desc.longOutputVk)     s.altWasDown      = true;
                    if (vk == desc.tertiaryOutputVk) s.tertiaryWasDown = true;
                    // Sync sibling MeleeBurst's lastUsedWeaponVk — the injected key press happens
                    // inside Sleep so IsKeyDown never sees it, leaving lastUsedWeaponVk stale.
                    for (int j = 0; j < profile->bindingCount; j++) {
                        if (profile->bindings[j].behavior.type != BehaviorType::MeleeBurst) continue;
                        if (profile->bindings[j].behavior.returnWeapon == ReturnWeapon::Auto)
                            state[j].meleeBurst.lastUsedWeaponVk = vk;
                        break;
                    }
                    // Advance to the next weapon in cycle
                    s.cycleIndex = hasTertiary ? (s.cycleIndex + 1) % 3
                                               : (s.cycleIndex == 0 ? 1 : 0);
                } else if (!keyDown) {
                    s.pressed = false;
                }
                break;
            }

            case BehaviorType::MeleeBurst: {
                MeleeBurstState& s = state[i].meleeBurst;

                // Auto-mode weapon tracking: detect rising edges of weapon keys when not in melee
                if (desc.returnWeapon == ReturnWeapon::Auto && !s.inMelee) {
                    bool primaryDown   = desc.longOutputVk ? IsKeyDown(desc.longOutputVk) : false;
                    bool secondaryDown = desc.returnAltVk  ? IsKeyDown(desc.returnAltVk)  : false;
                    if (primaryDown   && !s.primaryWasDown)   s.lastUsedWeaponVk = desc.longOutputVk;
                    if (secondaryDown && !s.secondaryWasDown) s.lastUsedWeaponVk = desc.returnAltVk;
                    s.primaryWasDown   = primaryDown;
                    s.secondaryWasDown = secondaryDown;
                    // Also track heavy weapon key (defined on the sibling KeyToggle binding)
                    WORD heavyVk = 0;
                    for (int j = 0; j < profile->bindingCount; j++) {
                        if (profile->bindings[j].behavior.type == BehaviorType::KeyToggle &&
                            profile->bindings[j].behavior.tertiaryOutputVk) {
                            heavyVk = profile->bindings[j].behavior.tertiaryOutputVk;
                            break;
                        }
                    }
                    if (heavyVk) {
                        bool heavyDown = IsKeyDown(heavyVk);
                        if (heavyDown && !s.heavyWasDown) s.lastUsedWeaponVk = heavyVk;
                        s.heavyWasDown = heavyDown;
                    }
                }

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
                        WORD returnVk;
                        if (desc.returnWeapon == ReturnWeapon::Auto)
                            returnVk = s.lastUsedWeaponVk ? s.lastUsedWeaponVk : desc.longOutputVk;
                        else
                            returnVk = (desc.returnWeapon == ReturnWeapon::Secondary && desc.returnAltVk)
                                       ? desc.returnAltVk : desc.longOutputVk;
                        ReleaseKey(desc.outputVk);    // release melee key
                        Sleep(30);
                        PressKey(returnVk);           // press selected return weapon key
                        Sleep(desc.durationMs);       // match forward-switch hold time so game reliably registers weapon change
                        ReleaseKey(returnVk);
                        s.inMelee = false;

                        // Sync sibling KeyToggle cycleIndex to the weapon we just returned to.
                        // The return key was pressed+released inside a Sleep, so the rising-edge
                        // detector in KeyToggle never sees it — cycleIndex would be stale otherwise.
                        for (int j = 0; j < profile->bindingCount; j++) {
                            if (profile->bindings[j].behavior.type != BehaviorType::KeyToggle) continue;
                            const BehaviorDescriptor& kd = profile->bindings[j].behavior;
                            KeyToggleState& ks = state[j].keyToggle;
                            bool hasTert = kd.includeTertiaryInCycle && kd.tertiaryOutputVk;
                            if      (returnVk == kd.outputVk)                   ks.cycleIndex = 1;
                            else if (returnVk == kd.longOutputVk)               ks.cycleIndex = hasTert ? 2 : 0;
                            else if (hasTert && returnVk == kd.tertiaryOutputVk) ks.cycleIndex = 0;
                            break;
                        }
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
