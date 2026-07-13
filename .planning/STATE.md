# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-11)

**Core value:** All remapping behavior types (hold-to-toggle, combos/sequences, mouse wheel → key, timing/delays) work reliably for any game profile the user creates.
**Current focus:** Phase 1 — Behavior Engine

## Current Position

Phase: 1 of 4 (Behavior Engine)
Plan: 1 of 3 in current phase
Status: In progress
Last activity: 2026-03-12 — Completed 01-01 (BehaviorEngine.h + GameProfiles.h extension)

Progress: [█░░░░░░░░░] 8%

## Performance Metrics

**Velocity:**
- Total plans completed: 1
- Average duration: 2 min
- Total execution time: 0.03 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01-behavior-engine | 1 | 2 min | 2 min |

**Recent Trend:**
- Last 5 plans: 01-01 (2 min)
- Trend: —

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Init]: Data-driven profiles over plugin DLLs — presets cover all needed logic types
- [Init]: In-app UI editor is the primary authoring surface; JSON files are storage
- [Init]: Start with four preset behavior types; scripting deferred to later milestone
- [Init]: Rebuild Elden Ring + Toxic Commando as JSON profiles; hardcoded implementations removed
- [01-01]: BehaviorEngine.h uses forward declaration of GameProfile to avoid circular include with GameProfiles.h
- [01-01]: MAX_BINDINGS flows into BehaviorEngine.h from GameProfiles.h include order (no redefinition)
- [01-01]: LongPress fires shortOutputVk on falling edge (tap) only, not rising edge

### Pending Todos

None yet.

### Blockers/Concerns

- Toxic Commando actual process name is unknown (current value is a placeholder) — must be confirmed before Phase 2 JSON profile can be completed
- Owner-drawn combobox dark-mode handling (Phase 3) — may need a spike before committing to settings window layout

## Session Continuity

Last session: 2026-03-12
Stopped at: Completed 01-01-PLAN.md (BehaviorEngine.h + GameProfiles.h extension)
Resume file: None
