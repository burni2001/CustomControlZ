# CustomControlZ

## What This Is

CustomControlZ is a Windows system-tray application that intercepts keyboard and mouse input and remaps it to in-game actions for games that are missing certain controls. The app detects when a target game is running and activates its custom remapping profile automatically. The next milestone transforms it from a hardcoded-profile tool into a data-driven platform where any game profile can be created and configured entirely within the app UI — no source code changes or recompilation required.

## Core Value

All remapping behavior types (hold-to-toggle, combos/sequences, mouse wheel → key, timing/delays) work reliably for any game profile the user creates.

## Requirements

### Validated

- ✓ System tray icon with game selection menu — existing
- ✓ Per-game settings window with configurable key bindings — existing
- ✓ Game process detection via ToolHelp32 (auto-activates when game launches) — existing
- ✓ Key remapping via SendInput (keyboard + mouse wheel injection) — existing
- ✓ Bindings persisted to settings.ini — existing
- ✓ Dark-mode UI with per-game color themes — existing
- ✓ Single-instance enforcement — existing
- ✓ Elden Ring profile: hold-to-sprint, dodge on key release — existing
- ✓ Toxic Commando profile: mouse scroll wheel → keyboard action — existing

### Active

- [ ] Flexible remapping engine with preset behavior types (hold-to-toggle, combo, mouse-wheel-to-key, timed/long-press)
- [ ] In-app "Add Game" UI to create new game profiles without writing C++
- [ ] In-app profile editor to configure process name, key bindings, and behavior type per binding
- [ ] Profiles stored in a data format (JSON or similar) outside the compiled binary
- [ ] Elden Ring and Toxic Commando rebuilt as data-driven profiles using the new system
- [ ] Behavior presets: each binding picks a logic type and configures its parameters (e.g. hold threshold ms, output key, scroll direction)

### Out of Scope

- Visual rule builder / conditional logic scripting — start simple with presets; can expand later
- Multiple simultaneous active profiles — single-game-at-a-time is the intended model
- Plugin loading at runtime from DLLs — data-driven profiles are sufficient
- Cloud sync or profile sharing — local-only is fine
- Mobile or non-Windows platforms — Windows-only by design

## Context

The existing codebase (~1100 lines, single .cpp file) has two hardcoded game profiles implemented as header-only C++ files. The architecture already has a clean `GameProfile` struct + `LogicThreadFn` abstraction — the path forward is to replace the compiled-in logic functions with a generic engine that interprets behavior descriptors at runtime. The `settings.ini` persistence layer will be extended or replaced by a richer profile format that stores behavior type and parameters alongside key assignments.

Key known issues to address during the rebuild:
- Toxic Commando process name is a placeholder (needs real value)
- No deduplication check for duplicate key bindings
- Game select window hardcodes "Segoe UI" instead of the user's chosen font

## Constraints

- **Tech stack**: C++20, Win32 API only — no new runtime dependencies
- **Portability**: Single executable, no installer — profile data files live next to the .exe
- **Build system**: MSVC / MSBuild — no CMake or other build system changes
- **UI style**: Existing dark-mode Win32 UI patterns — custom-drawn buttons, COLORREF themes

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Data-driven profiles over plugin DLLs | Avoids security/complexity of runtime code loading; presets cover all needed logic types | — Pending |
| In-app UI editor over config file only | User explicitly prefers UI; config file can still exist under the hood as the storage format | — Pending |
| Start with preset behaviors, not scripting | User confirmed "start simple" — presets cover all 4 required logic types | — Pending |
| Rebuild Elden Ring + Toxic Commando as data profiles | Clean slate using new system; existing hardcoded implementations removed | — Pending |

---
*Last updated: 2026-03-11 after initialization*
