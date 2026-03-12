---
phase: 01-behavior-engine
plan: 03
subsystem: behavior-engine
tags: [cpp, windows, input, security, uipi, integrity-level, elevation-detection]

# Dependency graph
requires:
  - phase: 01-behavior-engine/01-01
    provides: GenericLogicThreadFn game-start transition block, KeyTracker.releaseAll()
  - phase: 01-behavior-engine/01-02
    provides: EldenRing.h and ToxicCommando.h as pure data profiles
provides:
  - IsProcessRunningElevated function (OpenProcessToken + GetTokenInformation(TokenIntegrityLevel))
  - UIPI elevation warning MessageBox in GenericLogicThreadFn game-start transition
  - Forward declaration of IsProcessRunningElevated in BehaviorEngine.h
affects:
  - Phase 2 (JSON profile system) if process name lookup changes

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "UIPI proactive check: OpenProcessToken + TokenIntegrityLevel, not GetLastError after SendInput"
    - "One-shot elevation warning: checked at game-start transition (lastGameState false -> true), not per tick"

key-files:
  created: []
  modified:
    - CustomControlZ/CustomControlZ.cpp
    - CustomControlZ/BehaviorEngine.h

key-decisions:
  - "IsProcessRunningElevated is non-static (free function) in CustomControlZ.cpp so BehaviorEngine.h can forward-declare and call it from GenericLogicThreadFn"
  - "UIPI check placed in game-start transition block (else branch of lastGameState/currentGameState diff) — fires exactly once per game activation"
  - "No new #includes needed: all TOKEN_MANDATORY_LABEL, GetTokenInformation, GetSidSubAuthority types available via existing #include <windows.h>"
  - "FIX-01 already satisfied by Plan 01: tracker.releaseAll() at line 240 of BehaviorEngine.h runs after while(running) loop exits via StopGameLogicThread join path"

patterns-established:
  - "Elevation detection: enumerate processes via CreateToolhelp32Snapshot, open with PROCESS_QUERY_LIMITED_INFORMATION, query TokenIntegrityLevel, compare sub-authority to SECURITY_MANDATORY_HIGH_RID"

requirements-completed: [FIX-01, FIX-02]

# Metrics
duration: 1min
completed: 2026-03-12
---

# Phase 01 Plan 03: UIPI Detection Summary

**IsProcessRunningElevated() added to CustomControlZ.cpp using OpenProcessToken + TokenIntegrityLevel; UIPI warning MessageBox wired into GenericLogicThreadFn game-start transition for FIX-02**

## Performance

- **Duration:** ~10 min (Task 1 automated + human smoke test checkpoint)
- **Started:** 2026-03-12T10:51:03Z
- **Completed:** 2026-03-12T11:05:00Z
- **Tasks:** 2 (1 auto + 1 checkpoint:human-verify, approved)
- **Files modified:** 2

## Accomplishments
- Implemented `IsProcessRunningElevated(const wchar_t* processName)` in `CustomControlZ.cpp` as a non-static free function in the SYSTEM HELPERS section
- Function uses `OpenProcessToken` + `GetTokenInformation(TokenIntegrityLevel)` to detect High integrity (elevated/Administrator) processes — the only reliable way to detect UIPI risk (FIX-02)
- Added forward declaration of `IsProcessRunningElevated` in `BehaviorEngine.h` alongside existing helper forward declarations
- Added UIPI elevation check in `GenericLogicThreadFn` at the game-start state transition (fires once per game activation, not per tick)
- `MessageBox` warning displayed when game process runs at High integrity level, explaining UIPI restriction and suggesting running as Administrator
- Confirmed FIX-01 (no stuck keys on exit) is satisfied by the existing `tracker.releaseAll()` call after the `while (running)` loop in `GenericLogicThreadFn` — no changes needed to `StopGameLogicThread`
- All 6 Phase 1 smoke tests human-verified and approved: BEH-01 (HoldToToggle), BEH-02 (EdgeTrigger), BEH-03 (LongPress), BEH-04 (WheelToKey), FIX-01 (no stuck keys on exit), FIX-02 (UIPI warning when elevated)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add IsProcessRunningElevated and UIPI warning in OnGameSelected** - `c0b28ad` (feat)
2. **Task 2: checkpoint:human-verify** - _(no code commit; all 6 smoke tests human-approved)_

**Plan metadata:** _(committed with docs commit after summary)_

## Files Created/Modified
- `CustomControlZ/CustomControlZ.cpp` - Added `IsProcessRunningElevated()` free function (non-static) in SYSTEM HELPERS section before GAME LOGIC THREAD MANAGEMENT
- `CustomControlZ/BehaviorEngine.h` - Added forward declaration of `IsProcessRunningElevated` and UIPI check block in `GenericLogicThreadFn` game-start transition

## Decisions Made
- `IsProcessRunningElevated` is a non-static free function (not `static`) so it can be forward-declared in `BehaviorEngine.h` and called from `GenericLogicThreadFn` in a different translation unit
- The UIPI check is placed in the `else` branch of the `lastGameState != currentGameState` diff (game-start path), not outside it — this guarantees it fires exactly once per game activation
- `_alloca` used for `TOKEN_MANDATORY_LABEL` buffer (matches plan verbatim) — stack allocation appropriate for small fixed-size security descriptor
- No new headers needed: all required Win32 token types already available via `#include <windows.h>`

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Phase 1 behavior engine is complete: all four behavior types implemented, RAII key release verified, UIPI detection wired, all 6 smoke tests passed
- Phase 2 (Profile Persistence) can begin: JSON profile schema, auto-discovery, Elden Ring and Toxic Commando ported from .h files to JSON
- Blocker carried forward: Toxic Commando actual process name is still a placeholder (`ToxicCommando.exe`) — must be confirmed before Phase 2 JSON profile is authoritative
- Owner-drawn combobox dark-mode handling (Phase 3) — potential spike needed before settings window layout is committed

## Self-Check: PASSED

- CustomControlZ/CustomControlZ.cpp: FOUND (IsProcessRunningElevated at line 409, non-static)
- CustomControlZ/BehaviorEngine.h: FOUND (forward decl at line 20, UIPI check at lines 112-125)
- .planning/phases/01-behavior-engine/01-03-SUMMARY.md: FOUND (this file)
- Commit c0b28ad (Task 1): FOUND

---
*Phase: 01-behavior-engine*
*Completed: 2026-03-12*
