# Architecture

**Analysis Date:** 2026-03-11

## Pattern

**Plugin-based Modular Monolith with Multi-threaded Input Remapping**

CustomControlZ is a Windows system-tray application that intercepts keyboard input and remaps it to in-game actions for specific game profiles. The architecture separates:
- UI/Window management (main thread)
- Input remapping logic (per-game background thread)
- Game profiles (statically registered plugins)

## Layers

```
┌─────────────────────────────────────────────────────┐
│  UI Layer (Main Thread)                              │
│  - System tray icon + menu                           │
│  - Game selection window (GameSelectProc)            │
│  - Settings/keybind window (SettingsProc)            │
│  - Windows message pump (wWinMain)                   │
├─────────────────────────────────────────────────────┤
│  Profile Registry (Compile-time)                     │
│  - g_gameProfiles[] array in CustomControlZ.cpp      │
│  - Each profile: metadata + theme + bindings + logicFn│
├─────────────────────────────────────────────────────┤
│  Logic Thread (Per-active-game)                      │
│  - Game detection via ToolHelp32 process enumeration │
│  - Key state polling via GetAsyncKeyState             │
│  - Input injection via SendInput (keyboard/mouse)    │
│  - Tray icon state sync                              │
├─────────────────────────────────────────────────────┤
│  Persistence Layer                                   │
│  - INI file (settings.ini next to .exe)              │
│  - WritePrivateProfileString / GetPrivateProfileInt  │
└─────────────────────────────────────────────────────┘
```

## Entry Points

- **`wWinMain`** (`CustomControlZ/CustomControlZ.cpp:994`) — application entry point
  - Enforces single-instance via named mutex `CustomControlZ_Unique_ID`
  - Waits for shell ready (tray availability)
  - Creates game selection window, hidden tray window, tray icon
  - Runs standard Windows message pump

## Data Flow

### Startup
```
wWinMain
  → EnableWindows11DarkMode()
  → CreateMutex (single-instance guard)
  → WaitForShellReady()
  → CreateGameSelectionWindow() → shows g_hGameSelectWnd
  → AddTrayIconWithRetry()
  → GetMessage loop
```

### Game Selection
```
User clicks game button
  → OnGameSelected(profileIndex)
  → StopGameLogicThread()
  → g_activeProfile = g_gameProfiles[profileIndex]
  → LoadConfig(profile)  ← reads settings.ini
  → RebuildThemeBrushes(profile)
  → DestroyWindow(old settings wnd) if exists
  → CreateSettingsWindow()
  → StartGameLogicThread(profile)
```

### Logic Thread (per game)
```
GameProfile.logicFn(profile, running)
  → loop while running:
      if g_waitingForBindID != 0: sleep (user is rebinding)
      IsGameRunning() → ToolHelp32 process scan
      if game state changed: SetTrayIconState()
      if game not running: sleep 1s, continue
      read bindings under g_configMutex (snapshot)
      poll key states via GetAsyncKeyState
      inject input via SendInput
      sleep tick interval (10ms)
```

### Key Rebinding
```
User clicks bind button
  → g_waitingForBindID = buttonID
  → WM_KEYDOWN in SettingsProc
  → validate key (not modifier-only)
  → update binding under g_configMutex
  → SaveConfig(profile)  ← writes settings.ini
  → g_waitingForBindID = 0
```

## Key Abstractions

### `GameProfile` struct (`CustomControlZ/GameProfiles.h:33`)
Central plugin contract. Each game provides:
- Identity metadata (id, displayName, iniSection)
- Process names for detection (up to 2)
- Tray tooltip strings
- `Theme` — full color palette (11 COLORREF values)
- `KeyBinding[MAX_BINDINGS]` — up to 8 configurable keys
- `LogicThreadFn logicFn` — `std::function<void(GameProfile*, std::atomic<bool>&)>`

### `KeyBinding` struct (`CustomControlZ/GameProfiles.h:9`)
- `iniKey` — persisted key name in settings.ini
- `label` — UI label text
- `defaultVk` / `currentVk` — virtual key codes; `currentVk` is the live mutable value shared between UI and logic thread

### Thread Synchronization
- `g_configMutex` (`std::mutex`) — protects `g_fontName` and all `binding.currentVk` values
- `g_waitingForBindID` (`std::atomic<int>`) — signals logic thread to pause during rebind
- `g_logicRunning` (`std::atomic<bool>`) — signals logic thread to stop
- `g_isAppRunning` (`std::atomic<bool>`) — global app state flag

## Window Hierarchy

```
g_hMainWindow (hidden tray window, WindowProc)
  ├── g_hGameSelectWnd (game picker, GameSelectProc)
  └── g_hSettingsWnd (key bindings, SettingsProc)
```

The tray window is the owner; destroying it triggers full cleanup via `WM_DESTROY`.

## Cross-Cutting Concerns

- **Dark mode:** Applied at app launch (`EnableWindows11DarkMode`) and per-window (`EnableDarkTitleBar`) via undocumented `uxtheme.dll` ordinal 135
- **Single instance:** Named mutex prevents multiple simultaneous instances
- **Resource management:** All GDI objects (brushes, fonts, icons, pens) manually managed with explicit `DeleteObject`/`DestroyIcon` calls

---

*Architecture analysis: 2026-03-11*
