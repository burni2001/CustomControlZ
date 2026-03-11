# External Integrations

**Analysis Date:** 2026-03-11

## APIs & External Services

**None detected**
- CustomControlZ operates entirely as a standalone Windows application
- No network connectivity, cloud services, or remote APIs
- No third-party SDK integration beyond Windows platform libraries

## Data Storage

**Databases:**
- None used

**File Storage:**
- Local filesystem only
- Settings file: `settings.ini` (INI format)
  - Location: Same directory as executable
  - Purpose: Persists user key bindings and font preferences across sessions
  - Content: Per-game binding sections (e.g., `[EldenRing]`, `[ToxicCommando]`) with virtual key codes

**Caching:**
- None (in-memory only)
- Game process detection: Scanned every 100ms (`SHELL_CHECK_INTERVAL_MS`)
- Configuration: Loaded on game profile selection, kept in memory for real-time access

## Authentication & Identity

**Auth Provider:**
- None required
- Application runs with user privileges (no elevation required)
- Manifest specifies `asInvoker` execution level

## Monitoring & Observability

**Error Tracking:**
- None (no error reporting or telemetry)

**Logs:**
- None (no persistent logging)
- Runtime state available only through:
  - Console window (if opened in debug builds)
  - Tray icon state (visual indicator of game detection status)

## CI/CD & Deployment

**Hosting:**
- GitHub repository (local storage visible in `.git/`, `.github/` directories)
- No hosted deployment infrastructure

**Build Pipeline:**
- Manual build via Visual Studio or MSBuild
- No CI/CD automation detected (no `.github/workflows/` found)
- Build artifacts: Single executable `CustomControlZ.exe`

## Environment Configuration

**Required env vars:**
- None required

**Settings location:**
- `settings.ini` in executable directory (not environment-based)
- Default font: "Palatino Linotype"
- Default key bindings: Defined in game profile headers

## Webhooks & Callbacks

**Incoming:**
- None

**Outgoing:**
- None

## Windows System Integration

**Process Monitoring:**
- Scans running processes for game executable names:
  - Elden Ring: `eldenring.exe` or `ersc_launcher.exe`
  - Toxic Commando: `ToxicCommando.exe`
- Uses ToolHelp32 API (CreateToolhelp32Snapshot) for process enumeration

**Input Simulation:**
- `SendInput` API to inject keyboard and mouse wheel events
- Virtual key codes (VK_*) for keyboard input
- Mouse wheel events via `INPUT_MOUSE` with `MOUSEEVENTF_WHEEL` flag

**UI Integration:**
- Windows system tray via `NOTIFYICONDATA`
  - Tray icon switching between idle/active states
  - Context menu interaction
  - Right-click menu for Settings, Change Game, Exit

**Theme Integration:**
- DWM (Desktop Window Manager) for dark mode title bar
- UxTheme `SetPreferredAppMode` for dark mode support (Windows 11+)
- Per-game theme colors defined in `GameProfile.theme` structure

## Game Profile Integrations

**Elden Ring Profile:**
- Files: `CustomControlZ/games/EldenRing.h`
- Process names: `eldenring.exe`, `ersc_launcher.exe`
- Features:
  - Sprint/dodge remapping with configurable key bindings
  - Long-press trigger detection (400ms threshold for special actions)
  - Input sequence timing controls (dodge duration, sprint pause, etc.)
  - Elden Ring-specific theme colors (gold/brown palette)

**Toxic Commando Profile:**
- Files: `CustomControlZ/games/ToxicCommando.h`
- Process names: `ToxicCommando.exe`
- Features:
  - Keyboard-to-mouse-wheel conversion
  - Scroll up/down simulation for weapon switching
  - Military-themed UI color scheme (olive/green palette)

---

*Integration audit: 2026-03-11*
