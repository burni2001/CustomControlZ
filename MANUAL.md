# CustomControlz — Manual

## Table of Contents

1. [Overview](#overview)
2. [Installation](#installation)
3. [Using the Application](#using-the-application)
   - [Game Selection](#game-selection)
   - [Settings Window](#settings-window)
   - [System Tray](#system-tray)
   - [Configuration File](#configuration-file)
4. [Game Profiles](#game-profiles)
   - [Elden Ring](#elden-ring)
   - [Toxic Commando](#toxic-commando)
5. [Building from Source](#building-from-source)
6. [Architecture](#architecture)
7. [Adding a New Game Profile](#adding-a-new-game-profile)

---

## Overview

CustomControlz is a Windows system tray utility that intercepts and remaps keyboard and mouse input for specific games. It works by:

1. Detecting whether a supported game process is running
2. Activating a game-specific logic thread that monitors key states
3. Sending simulated input events to the game via the Windows `SendInput` API

The application uses no kernel-mode drivers or injection — it operates entirely through standard Windows APIs.

---

## Installation

1. Download `CustomControlz.exe` from the [Releases](../../releases) page.
2. Place the executable anywhere on your system (e.g., `C:\Tools\CustomControlz\`).
3. Run `CustomControlz.exe`.

No installer is required. Settings are saved to `settings.ini` in the same directory as the executable.

**System requirements:**
- Windows 10 or later
- Windows 11 is recommended for full dark mode support
- x64 processor

---

## Using the Application

### Game Selection

On first launch (or after clicking **Change Game** from the tray menu), a game selection window appears. Click the button for the game you want to configure. The application remembers your choice and opens directly to the settings window on subsequent launches.

### Settings Window

The settings window displays all configurable key bindings for the selected game.

| Control | Action |
|---------|--------|
| Binding button (shows key name) | Click, then press a new key to rebind |
| **Change Font** (top-left corner) | Cycle through available UI fonts |
| **Minimize** | Hide the window to the system tray |
| **Exit** | Close the application (prompts for confirmation) |

**Rebinding a key:**
1. Click the button next to the binding label — it will display "Press a key..."
2. Press the desired key on your keyboard.
3. The button updates to show the new key name and the binding is saved immediately.

### System Tray

The tray icon changes appearance based on game state:

| Icon | Meaning |
|------|---------|
| Idle icon (dark) | Game is not running |
| Active icon (bright) | Game is running; input remapping is active |

**Tray icon actions:**

| Action | Result |
|--------|--------|
| Double-click | Open settings window |
| Right-click | Open context menu |

**Context menu options:**

- **Settings** — Open the settings window
- **Change Game** — Return to the game selection screen
- **Exit** — Close the application

### Configuration File

Settings are stored in `settings.ini` next to the executable. The file is created automatically and updated whenever you change a binding.

Example `settings.ini`:

```ini
[EldenRing]
IngameKey=70
DodgeKey=67
SprintKey=20
TriggerKey=27

[ToxicCommando]
ScrollUpKey=33
ScrollDownKey=34

[App]
Font=Palatino Linotype
```

Values are Windows virtual key codes in decimal. You can edit this file manually if needed; changes take effect the next time the application loads.

---

## Game Profiles

### Elden Ring

**Detected process:** `eldenring.exe` or `ersc_launcher.exe`

**Theme:** Dark brown/gold

CustomControlz replaces the standard dodge-roll button with a more ergonomic sprint/dodge split across two separate keys.

| Binding | Default Key | Description |
|---------|-------------|-------------|
| **IngameKey** | F | The key your in-game dodge/backstep action is bound to |
| **DodgeKey** | C | Your custom dodge trigger |
| **SprintKey** | Caps Lock | Your custom sprint trigger |
| **TriggerKey** | Escape | Long-press trigger key |

**How the logic works:**

- **Sprinting:** Hold SprintKey → the logic thread continuously presses IngameKey (the game interprets a held press as sprinting).
- **Dodging:** Tap DodgeKey → briefly presses IngameKey, then resumes sprinting if SprintKey is still held.
- **Long-press trigger:** Hold TriggerKey for 400 ms or more → simulates pressing **Q**. A short tap passes the key through normally (for the in-game menu).

**Setup steps:**
1. In the Elden Ring in-game controls, bind dodge/backstep to **F** (or whatever you set as IngameKey).
2. Set SprintKey and DodgeKey to keys that are comfortable for you.
3. Launch the game — the remapping activates automatically.

### Toxic Commando

**Detected process:** `ToxicCommando.exe`

**Theme:** Dark olive/green

Allows weapon switching via keyboard keys instead of the scroll wheel.

| Binding | Default Key | Description |
|---------|-------------|-------------|
| **ScrollUpKey** | Page Up | Simulate scroll wheel up (next weapon) |
| **ScrollDownKey** | Page Down | Simulate scroll wheel down (previous weapon) |

**How the logic works:**

Each key press generates a single mouse scroll wheel event (delta +120 or -120). The logic is edge-triggered, so holding the key does not repeat the scroll.

---

## Building from Source

### Prerequisites

- **Visual Studio 2022** with the "Desktop development with C++" workload installed
- **Windows 10 SDK** or later (included with the Visual Studio workload)

### Build steps

**Via Visual Studio:**

1. Open `CustomControlz.slnx` in Visual Studio 2022.
2. Set the configuration to **Release** and the platform to **x64**.
3. Select **Build → Build Solution** (or press `Ctrl+Shift+B`).

**Via command line (MSBuild):**

```batch
msbuild CustomControlz.slnx /p:Configuration=Release /p:Platform=x64
```

The compiled executable is written to:

```
CustomControlz\bin\x64\Release\CustomControlz.exe
```

### Automated releases

The repository includes a GitHub Actions workflow (`.github/workflows/build-release.yml`) that builds the project and creates a GitHub Release. Trigger it manually from the **Actions** tab, providing a version number (e.g., `1.0.0`).

---

## Architecture

### Project layout

```
CustomControlz/
├── CustomControlz.cpp      # Application entry point, UI, tray, config I/O
├── GameProfiles.h          # Core data structures (KeyBinding, Theme, GameProfile)
├── games/
│   ├── EldenRing.h         # Elden Ring profile and logic thread
│   └── ToxicCommando.h     # Toxic Commando profile and logic thread
├── Resource.rc             # Icon and manifest resources
└── app.manifest            # DPI awareness, Windows 10+ compatibility
```

### Core data structures (`GameProfiles.h`)

**`KeyBinding`** — one remappable key:

```cpp
struct KeyBinding {
    const wchar_t* iniKey;    // Key name in settings.ini
    const wchar_t* label;     // Label shown in the settings window
    WORD defaultVk;           // Default virtual key code
    WORD currentVk;           // Runtime value (modified by user)
};
```

**`Theme`** — color palette for a game's UI:

```cpp
struct Theme {
    COLORREF bg, text, accent, button, border;
    COLORREF rowBg, separator, exitFill, exitBorder, exitText;
    COLORREF minimizeBorder;
};
```

**`GameProfile`** — everything that describes one game:

```cpp
struct GameProfile {
    const wchar_t* id;
    const wchar_t* displayName;
    const wchar_t* iniSection;
    const wchar_t* processName1;
    const wchar_t* processName2;   // Optional second process name
    const wchar_t* tipActive;
    const wchar_t* tipIdle;
    const wchar_t* settingsTitle;
    Theme theme;
    int bindingCount;
    KeyBinding bindings[MAX_BINDINGS];
    LogicThreadFn logicFn;         // Pointer to the logic thread function
};
```

### Threading model

The application runs two threads:

| Thread | Purpose |
|--------|---------|
| Main (UI) thread | Window messages, settings UI, tray icon, config save/load |
| Logic thread | Game process detection, key state polling, `SendInput` calls |

The logic thread is started when a game profile is selected and stopped when the user switches games or exits. Shared state (key bindings, tray icon state) is protected by a `std::mutex`.

### Input simulation

Key presses and releases are sent using `SendInput` with `INPUT_KEYBOARD`. Scroll wheel events are sent using `INPUT_MOUSE` with `MOUSEEVENTF_WHEEL`. The game must be the foreground window for these events to be received.

### Game detection

The logic thread calls `CreateToolhelp32Snapshot` + `Process32Next` in a loop to enumerate running processes and compare names against `processName1` / `processName2` in the active `GameProfile`.

---

## Adding a New Game Profile

### 1. Create a header file

Create `CustomControlz/games/YourGame.h`:

```cpp
#pragma once
#include "../GameProfiles.h"
#include <windows.h>
#include <tlhelp32.h>
#include <atomic>
#include <thread>

// Forward declarations needed from CustomControlz.cpp
extern std::atomic<bool> g_logicRunning;
extern void SetTrayIcon(bool active);
extern bool IsGameRunning(const wchar_t* name1, const wchar_t* name2);
extern void PressKey(WORD vk);
extern void ReleaseKey(WORD vk);

void YourGameLogicThread(GameProfile* profile) {
    while (g_logicRunning) {
        bool running = IsGameRunning(profile->processName1, profile->processName2);
        SetTrayIcon(running);

        if (running) {
            // Your input logic here
            // Access bindings via profile->bindings[n].currentVk
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

GameProfile YourGameProfile = {
    L"YourGame",                        // id
    L"Your Game Name",                  // displayName
    L"YourGame",                        // iniSection
    L"yourgame.exe",                    // processName1
    nullptr,                            // processName2 (or L"alt.exe")
    L"Your Game — Active",              // tipActive
    L"Your Game — Waiting",             // tipIdle
    L"Your Game Settings",              // settingsTitle
    {                                   // Theme
        RGB(20, 20, 30),  // bg
        RGB(220, 220, 220), // text
        RGB(80, 120, 200),  // accent
        RGB(40, 40, 60),    // button
        RGB(60, 60, 90),    // border
        RGB(30, 30, 45),    // rowBg
        RGB(50, 50, 80),    // separator
        RGB(180, 40, 40),   // exitFill
        RGB(220, 60, 60),   // exitBorder
        RGB(255, 255, 255), // exitText
        RGB(60, 60, 90),    // minimizeBorder
    },
    1,                                  // bindingCount
    {
        { L"ActionKey", L"Action", VK_SPACE, VK_SPACE },
    },
    YourGameLogicThread                 // logicFn
};
```

### 2. Register the profile in `CustomControlz.cpp`

At the top of `CustomControlz.cpp`, include your header and add the profile to the `g_profiles` array:

```cpp
#include "games/YourGame.h"

// In the profiles array:
GameProfile* g_profiles[] = {
    &EldenRingProfile,
    &ToxicCommandoProfile,
    &YourGameProfile,   // <-- add this
};
const int g_profileCount = _countof(g_profiles);
```

### 3. Build and test

Build the solution and verify:
- Your game appears in the game selection window
- Settings shows the correct bindings
- The tray icon updates when your game process starts/stops
- Your logic thread behaves correctly

### Tips

- Keep the logic thread tick rate at ~10 ms for responsiveness without excessive CPU usage.
- Use `std::atomic<bool> g_logicRunning` to break out of the loop cleanly on shutdown.
- Test with the game in the foreground — `SendInput` targets the foreground window.
- Use `PressKey`/`ReleaseKey` helpers for keyboard events and construct `INPUT` structs directly for mouse events.
