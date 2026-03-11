# Technology Stack

**Analysis Date:** 2026-03-11

## Languages

**Primary:**
- C++ (C++20) - Entire application codebase
  - Configured in `CustomControlZ/CustomControlZ.vcxproj` with `LanguageStandard=stdcpp20`
  - Used for core input remapping logic, UI, and Windows integration

## Runtime

**Environment:**
- Windows 10 or later (Windows 11 recommended)
- x64 processor architecture
- Runs as native Windows executable (no .NET or managed runtime)

**Platform Toolset:**
- Microsoft Visual C++ Platform Toolset v143 (Visual Studio 2022)
- MSVC compiler configured for:
  - Debug: `UseDebugLibraries=true`, `ConformanceMode=true`
  - Release: `FunctionLevelLinking=true`, `IntrinsicFunctions=true`, `WholeProgramOptimization=true`
  - All configurations: `SDLCheck=true` (Security Development Lifecycle checks enabled)

**Build System:**
- MSBuild (Visual C++ project system)
- Project file: `CustomControlZ/CustomControlZ.vcxproj`
- Solution file: `CustomControlZ.slnx`
- Targets: Win32 and x64 architectures with Debug/Release configurations

## Frameworks

**Core:**
- Windows API - All UI, input handling, and system integration
  - Desktop Windows applications using HWND message pump
  - System tray integration via `NOTIFYICONDATA`
  - Dark mode support via DWM (Desktop Window Manager) and UxTheme

**Threading:**
- C++ Standard Library `<thread>` - Game detection and input remapping runs on dedicated thread
- Synchronization: `<atomic>`, `<mutex>` for thread-safe state management

**Input Handling:**
- Windows `SendInput` API - Simulates keyboard and mouse wheel events
- `GetAsyncKeyState` / key state polling - Monitors user input in real-time
- Timing: Precise delay/sleep controls for input sequence timing

**Process Monitoring:**
- ToolHelp32 API (`<tlhelp32.h>`) - Detects running game processes
- Scans process list every 100ms in logic thread

## Key Dependencies

**Critical:**

- `uxtheme.lib` - Windows theming for dark mode support
  - Linked in: `#pragma comment(lib, "uxtheme.lib")`
  - Used by: Dark mode enabling, theme attribute setting

- `dwmapi.lib` - Desktop Window Manager API for visual effects
  - Linked in: `#pragma comment(lib, "dwmapi.lib")`
  - Used by: Windows 11 dark title bar support via `DwmSetWindowAttribute`
  - DWMA constant: `DWMWA_USE_IMMERSIVE_DARK_MODE` (20)

- `shell32.lib` (implicit via `<shellapi.h>`) - Shell operations
  - Includes: `ShellExecuteW`, tray icon management

**Standard Library:**
- `<windows.h>` - Core Windows API declarations
- `<shellapi.h>` - Shell API (tray icons, etc.)
- `<tlhelp32.h>` - Process enumeration
- `<strsafe.h>` - Safe string operations
- `<string>` - C++ standard strings
- `<atomic>` - Atomic operations for thread-safe flags
- `<mutex>` - Mutual exclusion for config access
- `<thread>` - Threading support

## Configuration

**Environment:**
- Settings stored in `.\\settings.ini` relative to executable location
- INI format with per-game sections (e.g., `[EldenRing]`, `[ToxicCommando]`)
- Sections defined in game profiles: `GameProfile.iniSection`

**Build Configuration:**
- Debug builds:
  - Preprocessor: `WIN32;_DEBUG;_CONSOLE` (Win32) or `_DEBUG;_CONSOLE` (x64)
  - Subsystem: Windows
  - Debug info: Full

- Release builds:
  - Preprocessor: `WIN32;NDEBUG;_CONSOLE` (Win32) or `NDEBUG;_CONSOLE` (x64)
  - Subsystem: Windows
  - Runtime Library (x64): MultiThreaded (static linking)
  - Manifest generation: Disabled for x64 release builds
  - Debug info: Enabled for diagnostics

**Manifest Requirements:**
- Application manifest: `CustomControlZ/app.manifest`
- DPI awareness: PerMonitorV2
- Common Controls v6.0.0.0 dependency declared
- Execution level: asInvoker (no admin required)
- Windows 10+ compatibility declared

## Platform Requirements

**Development:**
- Visual Studio 2022 (or compatible C++ toolchain)
- Windows 10/11 development environment
- MSVC v143 platform toolset
- SDK: Windows 10.0 (configurable via `WindowsTargetPlatformVersion`)

**Production:**
- Windows 10 or later
- x64 or x86 processor
- No additional runtime dependencies beyond Windows system libraries
- Single executable file (portable - no installation required)
- Can run from any directory; `settings.ini` created in execution directory

---

*Stack analysis: 2026-03-11*
