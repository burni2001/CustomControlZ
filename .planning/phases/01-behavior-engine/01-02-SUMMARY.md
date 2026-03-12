---
phase: 01-behavior-engine
plan: 02
subsystem: behavior-engine
tags: [cpp, windows, input, behavior, game-profiles, key-injection]

# Dependency graph
requires:
  - phase: 01-behavior-engine/01-01
    provides: BehaviorDescriptor struct, BehaviorType enum, GenericLogicThreadFn, KeyBinding.behavior field
provides:
  - EldenRing.h as pure data file with BehaviorDescriptor per binding, logicFn=GenericLogicThreadFn
  - ToxicCommando.h as pure data file with WheelToKey BehaviorDescriptors, logicFn=GenericLogicThreadFn
  - Hardcoded game-specific logic threads removed from both profile headers
affects:
  - 01-behavior-engine/01-03 (final wiring and integration)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Game profile as pure data: KeyBinding initializers carry BehaviorDescriptor; no logic thread code in profile headers"
    - "No-op binding: HoldToToggle with outputVk=0 is a sentinel meaning the binding drives nothing directly"
    - "Phase 1 output key hardcoding: outputVk='F' hardcoded in sprint/dodge descriptors; Phase 2 JSON will reference bindings[ER_KEY_INGAME].currentVk dynamically"

key-files:
  created: []
  modified:
    - CustomControlZ/games/EldenRing.h
    - CustomControlZ/games/ToxicCommando.h

key-decisions:
  - "ER_KEY_INGAME uses HoldToToggle/outputVk=0 as a no-op sentinel so the generic loop polls it without injecting anything; its currentVk remains the rebindable output key for other bindings"
  - "ER_KEY_TRIGGER LongPress: tap fires VK_ESCAPE (close/back), hold fires 'Q' (action) — new behavior vs original (old code only fired Q on hold, did nothing on tap)"
  - "outputVk for SPRINT and DODGE hardcoded to 'F' in Phase 1; dynamic binding-reference deferred to Phase 2 JSON profiles"
  - "TC thread-only constants (TC_LOGIC_TICK_MS, TC_GAME_CHECK_IDLE_MS) removed; GenericLogicThreadFn owns those values internally"
  - "ER thread-only constants (ER_LOGIC_TICK_MS, ER_BIND_CHECK_MS, ER_GAME_CHECK_IDLE_MS, ER_SPRINT_DODGE_PAUSE_MS, ER_ACTION_DURATION_MS) removed; kept ER_ACTION_VK, ER_LONG_PRESS_DELAY_MS, ER_DODGE_DURATION_MS as descriptor parameters"

patterns-established:
  - "Profile header = enum + constants + static GameProfile initializer only — zero logic code"
  - "BehaviorDescriptor aggregate init: specify type + only the fields relevant to that type; defaults handle the rest"

requirements-completed: [BEH-01, BEH-02, BEH-03, BEH-04]

# Metrics
duration: 3min
completed: 2026-03-12
---

# Phase 01 Plan 02: Game Profile Migration Summary

**EldenRing.h and ToxicCommando.h converted from code-bearing logic threads to pure data files — BehaviorDescriptors on each binding, GenericLogicThreadFn as logicFn for both profiles**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-12T10:42:02Z
- **Completed:** 2026-03-12T10:45:00Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Removed `EldenRingLogicThread` (105 lines of logic) from `EldenRing.h`; replaced with 4 `KeyBinding` initializers carrying `BehaviorDescriptor` fields and `logicFn = GenericLogicThreadFn`
- Removed `ToxicCommandoLogicThread` (61 lines of logic) from `ToxicCommando.h`; replaced with 2 `WheelToKey` bindings and `logicFn = GenericLogicThreadFn`
- All thread-only forward declarations (`IsGameRunning`, `SetTrayIconState`, `PressKey`, etc.) removed from both game headers — now supplied by BehaviorEngine.h
- Theme colors preserved exactly in both profiles

## Task Commits

Each task was committed atomically:

1. **Task 1: Rebuild EldenRing.h as pure data** - `6f306a0` (feat)
2. **Task 2: Rebuild ToxicCommando.h as pure data** - `70d1b6e` (feat)

**Plan metadata:** _(committed with docs commit after summary)_

## Files Created/Modified
- `CustomControlZ/games/EldenRing.h` - Enum + timing constants + g_EldenRingProfile with 4 BehaviorDescriptor bindings; no logic thread
- `CustomControlZ/games/ToxicCommando.h` - Enum + scroll delta constants + g_ToxicCommandoProfile with 2 WheelToKey bindings; no logic thread

## Decisions Made
- `ER_KEY_INGAME` uses `HoldToToggle` with `outputVk=0` as a no-op sentinel: the generic dispatcher polls its `currentVk` as input but injects nothing (outputVk=0), making it harmless. The key's `currentVk` value is referenced as the output key in the SPRINT and DODGE descriptors.
- `ER_KEY_TRIGGER` `LongPress` behavior is a functional improvement over the original: the old code fired 'Q' on long-hold but did nothing on a tap. The new behavior fires `VK_ESCAPE` on tap (close/back menu) and 'Q' on hold (action) — matching the phase specification.
- Sprint/dodge `outputVk` hardcoded to `'F'` for Phase 1. Dynamic referencing of `profile->bindings[ER_KEY_INGAME].currentVk` is deferred to Phase 2 JSON profiles, as noted in the plan.
- All thread-lifecycle constants removed from game headers (GenericLogicThreadFn uses internal values: 10ms tick, 1000ms idle, 50ms bind-check).

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Both game profile headers are now pure data; Plan 03 can wire the final integration (verify no compilation errors once `CustomControlZ.cpp` is updated to remove any remaining calls to the old logic thread names)
- Blocker carried forward: Toxic Commando actual process name still unknown (placeholder `ToxicCommando.exe` in profile)

## Self-Check: PASSED

- CustomControlZ/games/EldenRing.h: FOUND
- CustomControlZ/games/ToxicCommando.h: FOUND
- .planning/phases/01-behavior-engine/01-02-SUMMARY.md: FOUND
- Commit 6f306a0 (Task 1): FOUND
- Commit 70d1b6e (Task 2): FOUND

---
*Phase: 01-behavior-engine*
*Completed: 2026-03-12*
