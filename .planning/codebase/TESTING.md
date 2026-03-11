# Testing

**Analysis Date:** 2026-03-11

## Framework

**None.** CustomControlZ has no automated test framework.

- No unit test project in the solution
- No test files anywhere in the repository
- No testing dependencies or test runner configuration

## Current Testing Approach

All testing is **manual and integration-based**:

1. Build the project in Visual Studio (Debug or Release)
2. Launch the executable
3. Manually verify:
   - Game selection window appears with correct game buttons
   - Game can be selected and settings window opens with correct key bindings
   - Key rebinding works (click button → press key → persists to settings.ini)
   - Game detection: start the target game, verify tray icon changes to "active" state
   - Input remapping: in-game, verify custom keys trigger the correct actions
   - Font cycling: "Change Font" button cycles through Palatino → Segoe UI → Comic Sans
   - System tray: right-click menu, Settings/Change Game/Exit items work
   - Single-instance: launching a second instance does nothing

## CI/CD

`.github/workflows/build-release.yml` runs on push/tag but does **not** run any tests — it only builds and packages the release binary.

## Testability Assessment

The codebase has **low automated testability** due to:

- All logic tightly coupled to Win32 HWND and Windows API calls
- Logic thread functions (`EldenRingLogicThread`, etc.) take `GameProfile*` and call Win32 directly (`GetAsyncKeyState`, `SendInput`, `CreateToolhelp32Snapshot`)
- No dependency injection or abstraction over input/process APIs
- No separation between "game logic state machine" and "Win32 I/O"

## Adding Tests (if desired)

If automated tests were to be added, the recommended approach would be:

1. **Extract key state polling and input injection into abstract interfaces** — mock them in unit tests
2. **Test state machines in isolation** (e.g., sprint/dodge state in EldenRing logic)
3. **Use Google Test or Catch2** — both integrate with MSVC/MSBuild
4. Add a separate `CustomControlZTests` project to the solution

## Coverage Gaps

| Area | Risk | Notes |
|---|---|---|
| EldenRing sprint/dodge state machine | High | Complex edge cases (sprint + dodge interaction) not tested |
| Long-press trigger timing | Medium | Timing-dependent behavior |
| Config save/load | Medium | INI file parsing not validated |
| Thread shutdown / join | Medium | Race conditions possible |
| Single-instance mutex | Low | Manual verification only |
| Key name lookup (`GetKeyName`) | Low | Switch table completeness |

---

*Testing analysis: 2026-03-11*
