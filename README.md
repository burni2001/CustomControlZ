# CustomControlz

A Windows system tray application for advanced game key remapping and control customization. CustomControlz runs silently in the background, detects when a supported game is active, and applies custom input mappings in real-time.

## Features

- **System tray integration** — runs unobtrusively with idle/active icon states
- **Game-specific profiles** — each game gets its own key bindings and color theme
- **Automatic game detection** — monitors running processes and activates logic when your game is open
- **Persistent settings** — key bindings are saved to `settings.ini` and restored on next launch
- **Dark mode UI** — native Windows 11 dark mode support with per-game color themes
- **Single-instance protection** — prevents duplicate instances from running simultaneously

## Supported Games

| Game | Features |
|------|----------|
| **Elden Ring** | Sprint/dodge remapping, long-press trigger detection |
| **Toxic Commando** | Keyboard keys simulate mouse scroll wheel for weapon switching |

## Download

Download the latest `CustomControlz.exe` from the [Releases](../../releases) page. No installer required — just run the executable.

**Requirements:** Windows 10 or later (Windows 11 recommended for dark mode)

## Quick Start

1. Run `CustomControlz.exe`
2. Select your game from the game selection screen
3. Customize key bindings in the settings window
4. Click **Minimize** — the app moves to the system tray
5. Launch your game — CustomControlz activates automatically

Right-click the tray icon at any time to access settings, change game, or exit.

## Building from Source

See [MANUAL.md](MANUAL.md) for full build instructions, architecture details, and a guide to adding new game profiles.

## License

This project is provided as-is for personal use.
