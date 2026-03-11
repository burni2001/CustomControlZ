# Project Research Summary

**Project:** CustomControlZ — Data-Driven Game Profile System
**Domain:** Win32 input remapping engine with data-driven behavior interpreter
**Researched:** 2026-03-11
**Confidence:** HIGH

## Executive Summary

CustomControlZ is a zero-dependency, single-exe Win32 keyboard/mouse remapper that auto-activates per-game profiles based on process name detection. The current milestone replaces two hardcoded C++ game profiles (Elden Ring, Toxic Commando) with a fully data-driven system where profiles are JSON files users can create and edit through an in-app UI — without recompiling. This is a well-understood problem domain: reWASD, Steam Input, and AutoHotkey all solve it at various levels of complexity. CustomControlZ's competitive niche is portability and simplicity, not feature breadth, so the implementation strategy is deliberately constrained: one header-only JSON library (nlohmann/json), no new runtime dependencies, no new build tools.

The recommended approach is to build bottom-up: define the `BehaviorDescriptor` data schema first, then implement a generic behavior engine (replacing per-game `LogicThreadFn` functions with a parameterized interpreter), then port the two existing profiles to JSON, then expose profile creation and editing through a new in-app UI. This order is dictated by strict dependencies — every UI and persistence feature depends on the engine's data contract being settled first. The four core behavior presets that cover all current use cases are `HOLD_PASSTHROUGH`, `HOLD_TOGGLE`, `TRIGGER_ON_RELEASE`, and `WHEEL_TO_KEY`; more complex presets (`LONG_PRESS`, `TURBO`) are well-defined and low-risk additions after the engine is stable.

The critical risks are all in the behavior engine layer: keys getting stuck when the process exits mid-behavior, partial descriptor reads due to non-atomic mutex writes, and silent injection failures when a game runs elevated (UIPI). All three are preventable with known patterns — RAII cleanup for injected keys, single-lock writes for descriptor updates, and `SendInput` return-value checking — but they must be addressed during the engine phase, not retroactively. Anti-cheat detection is a documentation and UX concern, not a code concern: the recommended mitigation is a visible disclaimer in the Add Game UI, not any attempt to evade detection.

---

## Key Findings

### Recommended Stack

The existing stack (C++20, MSVC v143, MSBuild, Win32) requires no changes. The only addition is nlohmann/json v3.12.0 as a single-file header drop-in (`vendor/nlohmann/json.hpp`), with one new include path in the `.vcxproj`. JSON is the right format over TOML or INI extensions because profiles need nested arrays (multiple bindings per profile, multiple parameters per binding) that INI cannot represent and TOML handles awkwardly. `pathcch.lib` (already in the Windows SDK) provides exe-relative path resolution via `GetModuleFileNameW` + `PathCchRemoveFileSpec` — no new runtime dependency.

**Core technologies:**
- C++20 / MSVC v143: Implementation language — already in use, no changes
- Win32 API: File I/O, path resolution, UI, input injection — no changes
- nlohmann/json v3.12.0: Profile file parsing and serialization — single-header drop-in, MIT, C++20 compatible
- `pathcch.lib`: Exe-relative path resolution — already in Windows SDK, link via `#pragma comment`
- `settings.ini` (existing): Retained for app-level preferences (font, last selected game); not extended for profile data

**Profile file layout:** One JSON file per profile in a `profiles\` subdirectory next to the exe. Discovery via `FindFirstFileW("profiles\\*.json")` — no hardcoded profile index anywhere.

### Expected Features

All four currently-implemented behavior patterns (hold-passthrough, hold-to-toggle, release-trigger, wheel-to-key) must survive as named presets in the new engine. The in-app Add Game UI and behavior type picker are the core differentiators — the features that make CustomControlZ useful to non-technical users who don't want to write AutoHotkey scripts or pay for reWASD.

**Must have (table stakes — v1):**
- JSON profile schema with `id`, `processNames`, `theme`, and `bindings[]` (with `behaviorType` + parameters per binding)
- Generic behavior engine with `HOLD_PASSTHROUGH`, `HOLD_TOGGLE`, `TRIGGER_ON_RELEASE`, `WHEEL_TO_KEY` presets
- Elden Ring profile ported to JSON (validates hold-toggle and release-trigger patterns)
- Toxic Commando profile ported to JSON (validates wheel-to-key; fixes placeholder process name bug)
- In-app Add Game UI wizard (game name, process name, per-binding source key + behavior type selector)
- In-app profile editor (behavior type dropdown + editable parameters per binding)
- Duplicate-binding conflict detection (inline warning on save)
- Disable/enable per-binding flag (stored in JSON, toggleable in editor)

**Should have (v1.x after validation):**
- `LONG_PRESS` preset — tap vs. hold produces different output; clear user demand, medium complexity
- `TURBO` / auto-repeat preset — hold key to repeat; low complexity
- Per-game color theme customization in the Add Game UI (currently hardcoded)

**Defer (v2+):**
- `CHORD` / simultaneous-key preset — high complexity, no existing profile requires it
- `SEQUENCE` / macro preset — requires timing infrastructure beyond the current polling loop
- Profile import/export UI — users can share JSON files manually

### Architecture Approach

The architecture separates three previously-entangled concerns: static profile data (`BehaviorDescriptor[]` loaded from JSON, immutable after load), per-tick runtime state (`BehaviorState[]` allocated fresh per logic thread launch), and input/output I/O (`InputPoller` wrapping `GetAsyncKeyState`, `OutputDispatcher` as the only component that calls `SendInput`). The generic `LogicThreadFn` replaces all per-game logic thread functions with a single tick loop: poll snapshot, run all behavior state machines, flush output events.

**Major components:**
1. `ProfileLoader` — parses JSON files into `GameProfile` + `BehaviorDescriptor[]`; owns all file I/O; no Windows input APIs
2. `ProfileRegistry` — runtime list of loaded profiles; replaces `g_gameProfiles[]` static array
3. `BehaviorDescriptor` — immutable data struct: behavior type enum + parameters; loaded once, never mutated
4. `BehaviorState` — mutable per-thread state per descriptor (toggle flags, timestamps, key-was-down bools); stack-allocated, zeroed on thread start
5. `BehaviorRunner` — switch-dispatch on `BehaviorType`; evaluates state machines, emits output events; no direct Win32 I/O
6. `InputPoller` — snapshots `GetAsyncKeyState` for all bound VKs; returns `InputSnapshot`; no behavior logic
7. `OutputDispatcher` — converts output events to `SendInput` calls; the only `SendInput` call site in the engine
8. Generic `LogicThreadFn` — owns tick loop (poll, run, dispatch, sleep 10ms); no behavior knowledge

**Existing code that does not change:** `GameProfile` struct identity, `KeyBinding` struct, `g_configMutex`, `IsGameRunning()`, `PressKey()`/`ReleaseKey()`, `StartGameLogicThread()`/`StopGameLogicThread()` call sites, all UI layer code.

**Key pattern — build order enforced by dependencies:**
1. `BehaviorTypes.h` (enum + structs, no deps)
2. Generic `LogicThreadFn` (depends on 1 + existing helpers)
3. Smoke-test with hardcoded descriptors on existing profiles (validates engine before touching persistence)
4. `ProfileLoader` (JSON parsing, depends on 1)
5. JSON profile files for Elden Ring + Toxic Commando (depends on 4)
6. Runtime `ProfileRegistry` (depends on 4, 5)
7. UI extensions: behavior type selector + Add Game wizard (depends on 6)

### Critical Pitfalls

1. **UIPI blocks SendInput when game runs elevated** — Check `SendInput` return value on every call; surface a diagnostic ("Game may be running elevated") when return is 0 and `GetLastError` is `ERROR_ACCESS_DENIED`. Do not silently fail. Implement in the `OutputDispatcher`.

2. **Key stuck in OS input queue on exit or crash** — Maintain a set of currently-injected pressed keys in the behavior engine. Release all held keys on every exit path (normal shutdown, profile switch, force-kill) using RAII. This must be verified with a Task Manager force-kill test before any behavior type is considered complete.

3. **Partial descriptor read race condition** — The UI must write a complete `BehaviorDescriptor` as a single atomic operation under one `lock_guard` acquisition. Never update individual descriptor fields under separate mutex acquisitions. The logic thread snapshots the complete descriptor at the top of each tick.

4. **`GetAsyncKeyState` misses very short taps** — For tap-detection behaviors (toggle, combo), check both the high-order held bit (`& 0x8000`) and the low-order latch bit (`& 0x0001`). Hold-only behaviors can use `& 0x8000` alone. Must be per-behavior-type, not global.

5. **Stale behavior state on profile switch** — Runtime state (toggle flags, timers, combo progress) must live in `BehaviorState[]` objects created fresh per logic thread launch, never in `BehaviorDescriptor` or `GameProfile`. Switching profiles terminates the old thread (and its stack-allocated state) before starting the new thread.

---

## Implications for Roadmap

Based on combined research, the milestone decomposes naturally into four sequential phases. Each phase has a hard prerequisite on the previous one; the dependency chain is enforced by the architecture.

### Phase 1: Data Schema and Behavior Engine Foundation

**Rationale:** Every other feature in this milestone depends on the `BehaviorDescriptor` schema being defined and the generic engine being proven correct. This phase has zero UI work — it is purely engine and data structure work. Doing it first means the UI phases build on a stable contract. Doing it later means rework.

**Delivers:** `BehaviorTypes.h` with `BehaviorType` enum, `BehaviorDescriptor`, `BehaviorState`; a working generic `LogicThreadFn` smoke-tested against the two existing profiles with hardcoded descriptors (before JSON files exist); `HOLD_PASSTHROUGH`, `HOLD_TOGGLE`, `TRIGGER_ON_RELEASE`, and `WHEEL_TO_KEY` state machines implemented and tested.

**Addresses (from FEATURES.md):** Preset engine (P1), the behavior contract that all presets depend on.

**Avoids (from PITFALLS.md):**
- Pitfall 2 (key stuck on exit): RAII injected-key cleanup is a go/no-go criterion for this phase
- Pitfall 4 (missed taps): Per-behavior polling strategy decided here
- Pitfall 6 (stale state on profile switch): Descriptor vs. runtime state separation established here

### Phase 2: Profile Persistence (JSON Files)

**Rationale:** Once the engine accepts `BehaviorDescriptor[]` and the schema is stable, persistence is straightforward. JSON profile files must exist before the profile registry can be built at runtime, and the profile registry must exist before the UI can list profiles dynamically. Port both existing hardcoded profiles here to validate the round-trip fidelity before any UI is built.

**Delivers:** `ProfileLoader` (JSON → `GameProfile` + `BehaviorDescriptor[]`), `profiles/eldenring.json`, `profiles/toxiccommando.json`, runtime `ProfileRegistry` replacing `g_gameProfiles[]` static array, `settings.ini` binding values migrated to JSON (resolves the INI/JSON dual-format risk), Toxic Commando process name placeholder resolved.

**Uses (from STACK.md):** nlohmann/json v3.12.0 (added to `vendor/` and include path here), `GetModuleFileNameW` + `PathCchRemoveFileSpec` for exe-relative path resolution.

**Avoids (from PITFALLS.md):**
- Pitfall 3 (partial descriptor read race): Write contract defined during ProfileLoader design
- INI + JSON dual-format divergence: JSON is authoritative from this phase forward
- JSON value validation (vk codes in `[0x01, 0xFF]`, threshold in `[0, 10000]ms`) implemented in `ProfileLoader`

### Phase 3: Profile Editor (Existing Settings Window Extended)

**Rationale:** Extending the existing settings window to expose behavior type and parameters per binding is lower risk than building the Add Game UI, because the window chrome and layout already exist. Validate that the mutex contract (atomic descriptor writes) works correctly under live editing before building a more complex wizard dialog.

**Delivers:** Behavior type dropdown per binding in `SettingsProc`, editable behavior parameters (threshold ms, output VK), disable/enable binding flag toggle, duplicate-binding conflict detection on save, behavior type changes applied only on explicit Save (not on dropdown change).

**Addresses (from FEATURES.md):** In-app profile editor (P1), duplicate-binding detection (P1), disable/enable binding flag (P1).

**Avoids (from PITFALLS.md):**
- Pitfall 1 (UIPI): `OutputDispatcher` return-value diagnostic surfaced in editor UI
- Pitfall 3 (partial descriptor race): Atomic descriptor writes enforced in `SettingsProc`
- UX pitfall: Save on explicit confirm, not every keystroke; no live-apply on behavior type change

### Phase 4: Add Game UI (New Profile Creation)

**Rationale:** The Add Game wizard is the milestone's headline feature, but it is the last thing to build because it requires a stable profile schema (Phase 1), a working `ProfileLoader` that can write new files (Phase 2), and a proven editor pattern (Phase 3) to follow. Building it last means the wizard can reuse patterns proven in the editor.

**Delivers:** New dialog: game name field, process name field(s), per-binding source key capture + behavior type picker, profile color theme selector; creates a new `profiles/<id>.json` on disk; validates process name is non-empty and ends in `.exe`; anti-cheat disclaimer visible in the wizard for games with EAC/BattlEye.

**Addresses (from FEATURES.md):** In-app Add Game UI (P1), per-game color theme (initial exposure).

**Avoids (from PITFALLS.md):**
- Pitfall 5 (anti-cheat): Disclaimer present and specific in wizard UI
- UX pitfall: Process name validation with helpful hint ("matched case-insensitively")
- Duplicate-binding detection reused from Phase 3

### Phase Ordering Rationale

- Phases 1 → 2 → 3 → 4 follow the strict dependency chain identified in FEATURES.md: JSON schema is the foundation; preset engine is required before UI; profile files are required before the registry; the registry is required before dynamic UI.
- The smoke-test step at the end of Phase 1 (hardcoded descriptors on existing profiles) de-risks Phase 2 by proving the engine is correct before any persistence work begins.
- Splitting the editor (Phase 3) from the Add Game wizard (Phase 4) isolates complexity: the editor deals with modifying an existing in-memory profile; the wizard deals with creating a new file from scratch.

### Research Flags

Phases with well-documented patterns (skip additional research):
- **Phase 1 (Behavior Engine):** State machine patterns for all four behavior types are fully specified in ARCHITECTURE.md. No additional research needed — implement from the spec.
- **Phase 2 (JSON Persistence):** nlohmann/json integration is fully documented. Path resolution pattern is standard Win32. No gaps.

Phases that may need deeper research during planning:
- **Phase 3 (Profile Editor):** The behavior type combobox requires owner-drawn handling for dark mode (called out in PITFALLS.md). May need a spike on Win32 owner-drawn combobox with custom background before committing to layout.
- **Phase 4 (Add Game UI):** Key-capture control (waiting for user to press a key and displaying the VK label) is not a standard Win32 control. The existing settings window has this, so examine its implementation before designing the wizard.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | nlohmann/json verified from official release page; Win32 path pattern from official Microsoft Learn docs; no speculation |
| Features | HIGH | Behavior preset taxonomy derived from official Steam Input, reWASD, and AutoHotkey documentation; table stakes from codebase analysis |
| Architecture | HIGH | Derived directly from reading the existing codebase; build order and component boundaries confirmed by analogues (cpp-remapper, reWASD) |
| Pitfalls | HIGH (code pitfalls) / MEDIUM (anti-cheat specifics) | UIPI, key-stuck, race condition all confirmed from official docs + community evidence; anti-cheat detection thresholds are not publicly documented |

**Overall confidence:** HIGH

### Gaps to Address

- **Toxic Commando process name:** The actual executable name for Toxic Commando is not known (filed as a bug in CONCERNS.md). Must be researched or confirmed with the user before the JSON profile can be completed in Phase 2.
- **Key-capture control implementation:** The existing settings window has a "press a key to bind" control. Before designing the Add Game wizard (Phase 4), read and understand the existing `WM_KEYDOWN` capture pattern in `SettingsProc` to confirm it can be reused.
- **Owner-drawn combobox for dark mode (Phase 3):** The pitfalls research flags that a standard `CBS_DROPDOWNLIST` combobox requires subclassing for dark mode background. This is low-risk to defer to within Phase 3, but should be a planned spike item.
- **Anti-cheat detection thresholds:** EAC and BattlEye detection policies are not publicly documented and may change. Research confidence on this topic is MEDIUM. The recommended mitigation (UI disclaimer) is policy-independent and remains valid regardless of how detection policies evolve.

---

## Sources

### Primary (HIGH confidence)
- nlohmann/json GitHub — v3.12.0 release (single-header, MIT, C++20) — stack selection
- Microsoft Learn — `SendInput`, `GetAsyncKeyState`, `GetModuleFileNameW`, `PathCchRemoveFileSpec`, `CreateToolhelp32Snapshot` — pitfall prevention and Win32 patterns
- Steam Input Activator Types — Steamworks Documentation — behavior preset taxonomy
- reWASD Help Guide (Autodetect, Workspace UI, Release 5.5.1) — feature and behavior type benchmarking
- AutoHotkey v2 Remapping Docs — behavior type reference, hot-loop patterns
- CustomControlZ codebase direct analysis (EldenRing.h, ToxicCommando.h, GameProfiles.h, CustomControlZ.cpp, CONCERNS.md, PROJECT.md) — architecture, existing behavior, known bugs

### Secondary (MEDIUM confidence)
- BattlEye, Guided Hacking thread on input injection — anti-cheat detection risk assessment
- cpp-remapper (fredemmott) — confirms behavioral decomposition pattern (Source/Sink, ShortPressLongPress)
- catch22.net — WM_ERASEBKGND suppression and flicker-free drawing
- GameDev.net — `GetAsyncKeyState` polling latency discussion
- AutoHotkey community thread — key-stuck-on-exit widely reproduced behavior

### Tertiary (LOW confidence)
- Anti-cheat detection thresholds for EAC/BattlEye — policies not published; mitigation is documentation-only

---

*Research completed: 2026-03-11*
*Ready for roadmap: yes*
