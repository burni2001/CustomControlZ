# Directory Structure

**Analysis Date:** 2026-03-11

## Top-Level Layout

```
CustomControlz/                    # Repository root
├── CustomControlZ/                # MSVC project directory
│   ├── CustomControlZ.cpp         # Entire application (~1074 lines)
│   ├── GameProfiles.h             # GameProfile/KeyBinding/Theme structs
│   ├── resource.h                 # Resource ID constants
│   ├── Resource.rc                # Resource script (icons, manifest)
│   ├── app.manifest               # DPI awareness, UAC, Windows compat
│   ├── CustomControlZ.vcxproj     # MSVC project file
│   ├── CustomControlZ.vcxproj.filters  # Solution Explorer grouping
│   ├── CustomControlZ.vcxproj.user     # User-specific settings (gitignored)
│   ├── CustomControlZ.ico         # App icon (exe)
│   ├── idle.ico / idle.png        # Tray icon: waiting state
│   ├── active.ico / active.png    # Tray icon: game detected state
│   ├── CustomControlZ.png         # App logo/branding image
│   └── games/                     # Game profile implementations
│       ├── EldenRing.h            # Elden Ring profile + logic thread
│       └── ToxicCommando.h        # Toxic Commando profile + logic thread
├── CustomControlZ.slnx            # Visual Studio solution file
├── README.md                      # Project readme
├── MANUAL.md                      # User manual
├── .github/
│   └── workflows/
│       └── build-release.yml      # GitHub Actions CI/CD pipeline
├── .claude/
│   └── settings.local.json        # Claude Code permissions
└── .planning/
    └── codebase/                  # GSD codebase map documents
```

## Key File Locations

| Purpose | File |
|---|---|
| Application entry point | `CustomControlZ/CustomControlZ.cpp:994` |
| Game profile contract | `CustomControlZ/GameProfiles.h` |
| Profile registration | `CustomControlZ/CustomControlZ.cpp:427-431` |
| Elden Ring logic | `CustomControlZ/games/EldenRing.h:34` |
| Toxic Commando logic | `CustomControlZ/games/ToxicCommando.h:23` |
| INI config path | `.\settings.ini` (relative to exe, runtime) |
| Build pipeline | `.github/workflows/build-release.yml` |

## Code Organization Within `CustomControlZ.cpp`

The single `.cpp` file is organized by section comments:

```
Lines 1-13    — Includes and pragma directives
Lines 15-36   — Dark mode support (EnableWindows11DarkMode, EnableDarkTitleBar)
Lines 38-77   — Resource IDs, window/control IDs, timing constants
Lines 79-113  — Layout constants (settings window, game select window)
Lines 115-148 — Global state (g_hInstance, g_hMainWindow, g_activeProfile, etc.)
Lines 153-234 — Font helpers and GDI brush management
Lines 236-251 — Tray menu creation
Lines 253-270 — Font cycling (Palatino → Segoe UI → Comic Sans)
Lines 272-354 — Config management (SaveConfig, LoadConfig) and key naming
Lines 356-420 — System helpers (IsGameRunning, SendKeyInput, tray state)
Lines 406-431 — Thread management + game profile registration
Lines 433-698 — Settings window (SettingsProc, CreateSettingsWindow)
Lines 700-899 — Game selection window (GameSelectProc, CreateGameSelectionWindow)
Lines 901-962 — Main tray window (WindowProc)
Lines 964-990 — Startup helpers (WaitForShellReady, AddTrayIconWithRetry)
Lines 994-1073 — Entry point (wWinMain)
```

## Adding a New Game

To add a new game profile:
1. Create `CustomControlZ/games/YourGame.h` following `EldenRing.h` or `ToxicCommando.h` as template
2. Add `#include "games/YourGame.h"` in `CustomControlZ.cpp` after the other game includes (~line 425)
3. Add `&g_YourGameProfile` to `g_gameProfiles[]` array (~line 428)
4. Add the `.vcxproj` filter entry if desired (cosmetic only)

## Naming Conventions

| Pattern | Example |
|---|---|
| Global handles/state | `g_hMainWindow`, `g_activeProfile`, `g_fontName` |
| Window class names | `L"CustomControlZSettingsClass"` |
| Control ID macros | `BTN_BIND_BASE`, `ID_LABEL_BASE`, `ID_TRAY_EXIT` |
| Profile static vars | `g_EldenRingProfile`, `g_ToxicCommandoProfile` |
| Binding enum values | `ER_KEY_INGAME`, `TC_KEY_SCROLL_UP` |
| Timing constants | `ER_LONG_PRESS_DELAY_MS`, `TC_LOGIC_TICK_MS` |
| Layout constants | `LAYOUT_LEFT_MARGIN`, `SELECT_WIN_WIDTH` |
| Buffer size constants | `KEY_NAME_BUFFER`, `BUTTON_TEXT_BUFFER` |

---

*Structure analysis: 2026-03-11*
