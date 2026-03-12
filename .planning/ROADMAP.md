# Roadmap: CustomControlZ — Data-Driven Profile System

## Overview

This milestone replaces two hardcoded C++ game profiles with a fully data-driven system. The work proceeds in a strict dependency chain: the behavior engine must be proven correct before persistence can be designed against it, persistence must exist before the editor can be built on top of it, and the editor pattern must be validated before the more complex Add Game wizard is assembled. Each phase delivers a coherent, independently testable capability.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Behavior Engine** - Generic behavior engine with all four preset state machines, RAII key cleanup, and UIPI detection
- [ ] **Phase 2: Profile Persistence** - JSON profile schema, auto-discovery, Elden Ring and Toxic Commando ported, settings.ini migrated
- [ ] **Phase 3: Profile Editor** - Existing settings window extended with behavior type picker, duplicate warning, and enable/disable toggle
- [ ] **Phase 4: Add Game UI** - New profile creation and management wizard with anti-cheat disclaimer

## Phase Details

### Phase 1: Behavior Engine
**Goal**: All four behavior presets run reliably in a generic engine, with no keys ever stuck in the OS input queue and UIPI failures surfaced to the user
**Depends on**: Nothing (first phase)
**Requirements**: BEH-01, BEH-02, BEH-03, BEH-04, FIX-01, FIX-02
**Success Criteria** (what must be TRUE):
  1. Running the Elden Ring profile through the generic engine (with hardcoded descriptors) produces the same hold-to-toggle and release-trigger behavior as the original hardcoded implementation
  2. Running the Toxic Commando profile through the generic engine produces the same wheel-to-key behavior as the original
  3. Force-killing the app while a key is held down does not leave that key stuck in the OS — the key is released before the process exits
  4. When the target game runs elevated and SendInput is blocked, a visible warning is shown to the user rather than silently failing
  5. LongPress behavior fires the short-press output on a tap and the long-press output on a sustained hold
**Plans**: 3 plans

Plans:
- [x] 01-01-PLAN.md — Create BehaviorEngine.h (GenericLogicThreadFn, KeyTracker, all behavior types) + extend GameProfiles.h KeyBinding with BehaviorDescriptor
- [ ] 01-02-PLAN.md — Rebuild EldenRing.h and ToxicCommando.h as pure descriptor data, remove hardcoded logic threads
- [ ] 01-03-PLAN.md — Add UIPI detection (IsProcessRunningElevated) to CustomControlZ.cpp + human verification of all 6 smoke tests

### Phase 2: Profile Persistence
**Goal**: Game profiles live as JSON files on disk; the app discovers and loads them automatically at startup with no hardcoded profile list in source code
**Depends on**: Phase 1
**Requirements**: PROF-01, PROF-02, PROF-03, PROF-04
**Success Criteria** (what must be TRUE):
  1. A `profiles\` folder next to the exe contains one `.json` file per game; removing a file removes that game from the app on next launch
  2. Elden Ring and Toxic Commando load from JSON files and behave identically to their previous hardcoded implementations
  3. The app starts with no settings.ini binding data — all binding values come from the JSON profile files
  4. Adding a new `.json` file to `profiles\` and restarting the app causes that game to appear in the tray menu without any code change
**Plans**: TBD

### Phase 3: Profile Editor
**Goal**: Users can configure behavior type and parameters for each binding, enable or disable individual bindings, and are warned when a duplicate key assignment is detected — all within the existing settings window
**Depends on**: Phase 2
**Requirements**: BEH-05, BEH-06, BEH-07
**Success Criteria** (what must be TRUE):
  1. The settings window shows a behavior type dropdown (HoldToToggle / EdgeTrigger / LongPress / WheelToKey) for each binding, and saving applies the selected type to the running engine
  2. Assigning the same source key to two bindings causes a visible warning in the settings window before the user saves
  3. Unchecking a binding's enable toggle causes that binding to stop firing without deleting it; re-enabling restores it
**Plans**: TBD

### Phase 4: Add Game UI
**Goal**: Users can create, edit, and delete game profiles entirely within the app UI — no file editing or recompilation required
**Depends on**: Phase 3
**Requirements**: GAME-01, GAME-02, GAME-03, GAME-04
**Success Criteria** (what must be TRUE):
  1. A user can create a new game profile by filling in a name, process .exe, color theme, and initial bindings in an in-app wizard — a corresponding `.json` file appears in `profiles\` and the game appears in the tray menu
  2. A user can open an existing profile and change its name, process name, or color theme and have the change persist after app restart
  3. A user can delete a game profile from within the app; the corresponding `.json` file is removed and the game no longer appears in the tray menu
  4. An anti-cheat disclaimer is visible whenever the user creates a new profile or opens the Add Game wizard
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Behavior Engine | 1/3 | In progress | - |
| 2. Profile Persistence | 0/TBD | Not started | - |
| 3. Profile Editor | 0/TBD | Not started | - |
| 4. Add Game UI | 0/TBD | Not started | - |
