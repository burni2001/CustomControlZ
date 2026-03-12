---
phase: 01-behavior-engine
plan: 01
subsystem: behavior-engine
tags: [cpp, windows, input, behavior, key-injection, game-profiles]

# Dependency graph
requires: []
provides:
  - BehaviorType enum (HoldToToggle, EdgeTrigger, LongPress, WheelToKey)
  - BehaviorDescriptor struct with defaults for all four behavior types
  - BindingState union (zero-initialized per-binding state)
  - KeyTracker class with releaseAll() for RAII key cleanup (FIX-01)
  - GenericLogicThreadFn: single dispatcher for all four behavior types
  - KeyBinding.behavior field in GameProfiles.h
affects:
  - 01-behavior-engine/01-02 (game profile migration to use BehaviorDescriptor)
  - 01-behavior-engine/01-03 (wiring GenericLogicThreadFn into game profiles)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "BehaviorDescriptor: data-driven behavior config stored directly on KeyBinding"
    - "KeyTracker: RAII-style key release tracker using std::set<WORD>"
    - "GenericLogicThreadFn: single switch-dispatched loop replaces per-game logic threads"
    - "Acyclic header include: GameProfiles.h -> BehaviorEngine.h (never reverse)"

key-files:
  created:
    - CustomControlZ/BehaviorEngine.h
  modified:
    - CustomControlZ/GameProfiles.h

key-decisions:
  - "BehaviorEngine.h uses struct GameProfile; forward declaration instead of including GameProfiles.h to avoid circular dependency"
  - "MAX_BINDINGS is not redefined in BehaviorEngine.h; it is available because GameProfiles.h includes BehaviorEngine.h in every TU"
  - "BindingState union uses memset zero-init in constructor so all four state types start clean"
  - "LongPress fires shortOutputVk only on falling edge (tap), not on rising edge, matching plan specification"
  - "WheelToKey uses SendInput with INPUT_MOUSE/MOUSEEVENTF_WHEEL, not PressKey/ReleaseKey"

patterns-established:
  - "Behavior dispatch: switch on BehaviorDescriptor.type inside GenericLogicThreadFn"
  - "Key safety: KeyTracker.press/release mirrors all PressKey/ReleaseKey calls; releaseAll() at game-stop and thread exit"
  - "VK snapshot: localVk[] captured under g_configMutex once per tick, then used lock-free"

requirements-completed: [BEH-01, BEH-02, BEH-03, BEH-04, FIX-01]

# Metrics
duration: 2min
completed: 2026-03-12
---

# Phase 01 Plan 01: Behavior Engine Summary

**BehaviorEngine.h with four-type generic dispatcher (GenericLogicThreadFn), KeyTracker RAII key cleanup, and BehaviorDescriptor field added to KeyBinding in GameProfiles.h**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-12T10:36:22Z
- **Completed:** 2026-03-12T10:38:22Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Created `BehaviorEngine.h` with `BehaviorType` enum, `BehaviorDescriptor` struct, four per-binding state structs, `BindingState` union, `KeyTracker` class, and `GenericLogicThreadFn` implementation
- `GenericLogicThreadFn` handles all four behavior types in a single switch-dispatched loop — drop-in replacement for any `LogicThreadFn`
- `KeyTracker.releaseAll()` called on game-stop transition and thread exit, satisfying FIX-01 (no stuck keys on graceful exit)
- Extended `KeyBinding` in `GameProfiles.h` with `BehaviorDescriptor behavior` field; existing 4-field initializer lists in `EldenRing.h` and `ToxicCommando.h` remain valid due to default constructor
- Include chain is acyclic: `GameProfiles.h` → `BehaviorEngine.h` (one-way only)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create BehaviorEngine.h** - `f8a83fd` (feat)
2. **Task 2: Extend GameProfiles.h KeyBinding with BehaviorDescriptor** - `6aad246` (feat)

**Plan metadata:** _(committed with docs commit after summary)_

## Files Created/Modified
- `CustomControlZ/BehaviorEngine.h` - BehaviorType enum, BehaviorDescriptor, state structs, BindingState union, KeyTracker, GenericLogicThreadFn
- `CustomControlZ/GameProfiles.h` - Added `#include "BehaviorEngine.h"` and `BehaviorDescriptor behavior` field to KeyBinding

## Decisions Made
- `BehaviorEngine.h` uses `struct GameProfile;` forward declaration instead of `#include "GameProfiles.h"` to avoid circular dependency; `MAX_BINDINGS` flows in from `GameProfiles.h` which is always compiled first in any TU using `GenericLogicThreadFn`
- `BindingState` union uses `memset(this, 0, sizeof(*this))` in constructor so all member state starts clean regardless of active behavior type
- `LongPress` fires `shortOutputVk` (tap) on the falling edge only when `longFired == false`, exactly matching the plan specification
- `WheelToKey` sends `INPUT_MOUSE / MOUSEEVENTF_WHEEL` via `SendInput`, not `PressKey/ReleaseKey`, matching the plan's WheelToKey injection pattern

## Deviations from Plan

None - plan executed exactly as written. The circular-include resolution strategy was already specified in Task 2 (remove `#include "GameProfiles.h"` from BehaviorEngine.h, use forward declaration), so no deviation occurred.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- `BehaviorEngine.h` is ready; Plan 02 can migrate `EldenRing.h` and `ToxicCommando.h` to populate `BehaviorDescriptor` fields on their `KeyBinding` arrays
- Plan 03 can wire `GenericLogicThreadFn` as the `logicFn` for each game profile and remove the hardcoded `EldenRingLogicThread` / `ToxicCommandoLogicThread` implementations
- Blocker carried forward: Toxic Commando actual process name is still unknown (placeholder `ToxicCommando.exe` in profile)

## Self-Check: PASSED

- CustomControlZ/BehaviorEngine.h: FOUND
- CustomControlZ/GameProfiles.h: FOUND
- .planning/phases/01-behavior-engine/01-01-SUMMARY.md: FOUND
- Commit f8a83fd (Task 1): FOUND
- Commit 6aad246 (Task 2): FOUND

---
*Phase: 01-behavior-engine*
*Completed: 2026-03-12*
