# Concerns

**Analysis Date:** 2026-03-11

## Tech Debt

### 1. Monolithic single-file architecture
**File:** `CustomControlZ/CustomControlZ.cpp` (1074 lines)
All UI, config, threading, and utility code in one file. Adding more games or features will make this unwieldy.

### 2. Game select window hardcodes font to "Segoe UI"
**File:** `CustomControlZ/CustomControlZ.cpp:737-745`
`GameSelectProc` creates fonts with hardcoded `L"Segoe UI"` instead of using the user's selected font. Inconsistent with settings window behavior.

### 3. Font cycling is a fixed-size hardcoded list
**File:** `CustomControlZ/CustomControlZ.cpp:255-270`
Only 3 fonts (Palatino Linotype → Segoe UI → Comic Sans MS) are available, cycling via string comparison. Adding fonts requires code changes.

### 4. Game select window fonts leak on WM_DESTROY only
**File:** `CustomControlZ/CustomControlZ.cpp:748-749`
Fonts stored in `GWLP_USERDATA` via `new HFONT[3]`. If the window is destroyed abnormally (e.g., if `WM_DESTROY` handler is skipped), fonts are never freed.

### 5. `g_isAppRunning` is set but never read
**File:** `CustomControlZ/CustomControlZ.cpp:132, 1068`
`g_isAppRunning` is declared and set to `false` at shutdown but nothing reads it. `g_logicRunning` is the actual stop signal.

### 6. Static `classRegistered` guard in `CreateSettingsWindow`
**File:** `CustomControlZ/CustomControlZ.cpp:667`
A `static bool classRegistered` prevents re-registering the window class. If `CreateSettingsWindow` is ever called from different contexts this could silently fail registration.

## Known Bugs

### 1. Toxic Commando process name is a placeholder
**File:** `CustomControlZ/games/ToxicCommando.h:89`
```cpp
/* processName1  */ L"ToxicCommando.exe",  // UPDATE with real process name before release
```
Game detection will not work until the real process name is confirmed and updated.

### 2. Sprint key not released when game exits mid-sprint
**File:** `CustomControlZ/games/EldenRing.h:51-55`
The cleanup path checks `sprintActive` and releases the key, but only when the game *stops* while sprinting. If the app is force-closed while sprinting, the injected key may remain "pressed" in the OS until the game is restarted or focus changes.

### 3. `GetClientRect` for exit button Y in game select is called before layout is finalized
**File:** `CustomControlZ/CustomControlZ.cpp:773-775`
`GetClientRect` is called during `WM_CREATE` before the window is fully sized; the client rect may not reflect the final size at that point.

### 4. Settings window allows rebinding to the same key multiple times
No deduplication check when assigning a virtual key to a binding. A user can assign the same key to multiple bindings, which will produce undefined behavior in the logic thread.

## Security Concerns

### 1. Anti-cheat detection risk
The app injects synthetic keyboard/mouse input via `SendInput` into running game processes. Some anti-cheat systems (EAC, BattlEye) may flag this as cheating/macro behavior.

### 2. No input validation on INI file values
**File:** `CustomControlZ/CustomControlZ.cpp:297-313`
`GetPrivateProfileInt` returns attacker-controlled integer from settings.ini. `IsValidKey` checks `> 0 && <= 0xFF` but doesn't prevent exotic virtual key codes from causing unexpected input injection.

### 3. Process name matching is case-insensitive string comparison
**File:** `CustomControlZ/CustomControlZ.cpp:368-371`
Uses `_wcsicmp` — correct but a malicious process could name itself `eldenring.exe` to activate the remapper unexpectedly. Low risk in practice.

### 4. Undocumented API usage for dark mode
**File:** `CustomControlZ/CustomControlZ.cpp:27-30`
`GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135))` uses an undocumented ordinal. May break on future Windows versions without warning.

## Performance

### 1. Process enumeration every 100ms when idle
**File:** `CustomControlZ/CustomControlZ.cpp:70`, logic threads
`CreateToolhelp32Snapshot` creates a full process snapshot every 1 second when the game isn't detected, and every tick when active. This is standard Windows practice but non-trivial overhead.

### 2. Settings window redraws all controls on font change
**File:** `CustomControlZ/CustomControlZ.cpp:161-190`
`RecreateAllFonts()` + `UpdateAllControlFonts()` deletes and recreates all 4 font objects and triggers full window repaint. Acceptable for an infrequent operation.

### 3. `g_configMutex` lock in logic thread hot path
**File:** `CustomControlZ/games/EldenRing.h:65-72`
The mutex is acquired every 10ms to snapshot binding values. This is safe but adds contention when the user is actively rebinding.

## Fragile Areas

### 1. Logic thread startup/shutdown ordering
**File:** `CustomControlZ/CustomControlZ.cpp:407-420`
`StopGameLogicThread()` calls `g_logicThread.join()` — if the thread is stuck (e.g., blocked in `Sleep`), the UI will freeze. The sleep durations are short (10-1000ms) so this is acceptable in practice.

### 2. Window handle globals
`g_hSettingsWnd`, `g_hGameSelectWnd`, `g_hMainWindow` are raw globals. Destroying one can invalidate references held elsewhere. The `WM_DESTROY` handler in `WindowProc` carefully nulls these out but the order is fragile.

### 3. `WM_CLOSE` vs `WM_DESTROY` semantics
Game select and settings windows override `WM_CLOSE` to either hide the window or destroy the main window. A "close settings window" click destroys the *entire app* (via `DestroyWindow(g_hMainWindow)`), which is intentional but easy to change incorrectly.

## Scaling Limits

### 1. MAX_BINDINGS = 8 hard cap
**File:** `CustomControlZ/GameProfiles.h:7`
Array-based binding storage limits each game to 8 key bindings. Increasing requires changing the struct and recompiling.

### 2. Game profiles must be compiled in
No plugin loading or dynamic profile discovery. Every new game requires a source change and rebuild.

### 3. Single active profile at a time
Only one game's logic thread runs at a time. If a user plays multiple games simultaneously or wants global remapping, the architecture doesn't support it.

## Missing Features (noted in code/docs)

- No profile auto-detection on startup (always shows game selection)
- No way to disable a specific binding without removing the game profile
- No logging or diagnostic output for troubleshooting input injection issues
- Toxic Commando process name not confirmed (`// UPDATE with real process name`)

---

*Concerns analysis: 2026-03-11*
