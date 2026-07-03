# CustomControlZ

A Windows system tray application for advanced game key remapping and control customization. CustomControlZ runs silently in the background, detects when a supported game is active, and applies custom input mappings in real-time.

Main reason for this app is for myself and my unwillingness to learn new button mappings in different games :)

## Features

- **System tray integration** — runs unobtrusively with idle/active icon states, and a right-click menu to switch games or quit
- **Game-specific profiles** — each game gets its own key bindings and color theme
- **Automatic game detection** — monitors running processes and activates logic when your game is open
- **Persistent settings** — key bindings are saved to `settings.ini` and restored on next launch
- **Dark mode UI** — native Windows 11 dark mode support with per-game color themes
- **Start with Windows** — optional autostart toggle from the tray menu
- **Tooltips toggle** — show or hide UI tooltips from the tray menu
- **Single-instance protection** — prevents duplicate instances from running simultaneously

## Supported Games

| Game | Features |
|------|----------|
| **Dark Souls™ III** | Walk/run/sprint remapping, custom dodge and sprint keys |
| **Elden Ring** | Sprint/dodge remapping, long-press trigger detection |
| **Toxic Commando** | Keyboard keys simulate mouse scroll wheel for weapon switching, quick melee with configurable swing/return delays |
| **Valheim** | Quick item slot select and weapon swap keys |
| **Warhammer 40,000: Darktide** | Quick melee combo with weapon auto-return, text chat suspend key |

## Screenshots

**Menu**

![Tray Menu](src/assets/Screenshot%202026-07-03%20100936.png)

**Dark Souls III — Key Bindings**

![Dark Souls III Key Bindings](src/assets/Screenshot%202026-07-03%20100946.png)

**Elden Ring — Key Bindings**

![Elden Ring Key Bindings](src/assets/Screenshot%202026-07-03%20100955.png)

**Toxic Commando — Key Bindings**

![Toxic Commando Key Bindings](src/assets/Screenshot%202026-07-03%20101002.png)

**Valheim — Key Bindings**

![Valheim Key Bindings](src/assets/Screenshot%202026-07-03%20101015.png)

**Warhammer 40,000: Darktide — Key Bindings**

![Darktide Key Bindings](src/assets/Screenshot%202026-07-03%20101025.png)

## Download

Download the latest `CustomControlZ.exe` from the [Releases](../../releases) page. No installer required — just run the executable.

**Requirements:** Windows 10 or later (Windows 11 recommended for dark mode)

## Quick Start

1. Run `CustomControlZ.exe`
2. Right-click the tray icon and select your game from the menu
3. Customize key bindings in the settings window
4. Click **Minimize** — the app moves to the system tray
5. Launch your game — CustomControlZ activates automatically

Right-click the tray icon at any time to switch games, toggle autostart/tooltips, or exit.

## Building from Source

See [MANUAL.md](MANUAL.md) for full build instructions, architecture details, and a guide to adding new game profiles.

## License

This project is provided as-is for personal use.
