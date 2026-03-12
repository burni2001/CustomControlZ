# Requirements: CustomControlZ

**Defined:** 2026-03-11
**Core Value:** All remapping behavior types (hold-to-toggle, combos/sequences, mouse wheel → key, timing/delays) work reliably for any game profile the user creates.

## v1 Requirements

### Behavior Engine

- [x] **BEH-01**: App supports HoldToToggle binding behavior — hold key activates action, release deactivates it
- [x] **BEH-02**: App supports EdgeTrigger binding behavior — action fires on key release, not press
- [x] **BEH-03**: App supports LongPress binding behavior — short press vs. long press trigger different output keys
- [x] **BEH-04**: App supports WheelToKey binding behavior — mouse scroll up/down mapped to configurable key press(es)
- [ ] **BEH-05**: Settings window shows a behavior type picker per binding (dropdown: HoldToToggle / EdgeTrigger / LongPress / WheelToKey)
- [ ] **BEH-06**: Settings window warns user when the same key is assigned to more than one binding
- [ ] **BEH-07**: User can enable or disable individual bindings without deleting them

### Profile Storage

- [ ] **PROF-01**: Game profiles stored as JSON files in a `profiles\` folder next to the exe
- [ ] **PROF-02**: App auto-discovers and loads all `.json` profile files in `profiles\` at startup — no hardcoded list
- [ ] **PROF-03**: Elden Ring and Toxic Commando rebuilt as JSON profiles; hardcoded C++ implementations removed
- [ ] **PROF-04**: User's saved key bindings migrated from `settings.ini` into the corresponding JSON profile files

### Add / Edit / Delete Game

- [ ] **GAME-01**: User can create a new game profile via an in-app wizard (name, process .exe, color theme, initial bindings)
- [ ] **GAME-02**: User can edit an existing game profile's name, process name, or color theme
- [ ] **GAME-03**: User can delete a game profile from the app (removes the .json file)
- [ ] **GAME-04**: App displays an anti-cheat disclaimer when user creates or opens a game profile

### Reliability Fixes

- [x] **FIX-01**: All injected key presses are guaranteed released on app exit or game stop (RAII key tracking)
- [ ] **FIX-02**: App detects when `SendInput` is blocked by UIPI (game running elevated) and displays a warning to the user

## v2 Requirements

### Behavior Engine — Advanced

- **BEH-V2-01**: Behavior parameters UI — configure behavior-specific settings per binding (e.g. hold threshold ms, output key for WheelToKey scroll direction)
- **BEH-V2-02**: Combo/chord behavior preset — press two keys simultaneously to trigger an action

### Polish

- **POL-V2-01**: Game select window uses user's selected font instead of hardcoded "Segoe UI"
- **POL-V2-02**: Behavior parameters exposed as configurable fields with sensible defaults (e.g. LongPress threshold, WheelToKey repeat rate)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Visual rule builder / scripting | Start simple with presets; can expand in a future milestone |
| Multiple simultaneous active profiles | Single-game-at-a-time is the intended model |
| Plugin DLL loading | Data-driven profiles are sufficient; DLLs add security/complexity |
| Cloud sync or profile sharing | Local-only is fine for this tool |
| Mobile or non-Windows platforms | Windows-only by design |
| Anti-cheat evasion techniques | Documenting risk only; no behavioral masking |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| BEH-01 | Phase 1 | Complete (01-01) |
| BEH-02 | Phase 1 | Complete (01-01) |
| BEH-03 | Phase 1 | Complete (01-01) |
| BEH-04 | Phase 1 | Complete (01-01) |
| BEH-05 | Phase 3 | Pending |
| BEH-06 | Phase 3 | Pending |
| BEH-07 | Phase 3 | Pending |
| PROF-01 | Phase 2 | Pending |
| PROF-02 | Phase 2 | Pending |
| PROF-03 | Phase 2 | Pending |
| PROF-04 | Phase 2 | Pending |
| GAME-01 | Phase 4 | Pending |
| GAME-02 | Phase 4 | Pending |
| GAME-03 | Phase 4 | Pending |
| GAME-04 | Phase 4 | Pending |
| FIX-01 | Phase 1 | Complete (01-01) |
| FIX-02 | Phase 1 | Pending |

**Coverage:**
- v1 requirements: 17 total
- Mapped to phases: 17
- Unmapped: 0 ✓

---
*Requirements defined: 2026-03-11*
*Last updated: 2026-03-12 after 01-01 completion (BEH-01, BEH-02, BEH-03, BEH-04, FIX-01 marked complete)*
