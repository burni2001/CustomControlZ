# Feature Research

**Domain:** Data-driven game input remapping tool (Windows, keyboard/mouse, system tray)
**Researched:** 2026-03-11
**Confidence:** MEDIUM — behavior type taxonomy from HIGH-confidence sources (Steam Input official docs, reWASD help docs); UI/UX patterns from MEDIUM-confidence sources (product pages, community forums); implementation details specific to Win32/C++20 context from training data + codebase analysis

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features a remapper must have. Missing these makes the tool feel half-built or unreliable.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Simple 1:1 key remap | Every remapper has this; without it the tool has no baseline utility | LOW | Already exists via `SendInput`. Must remain available as a behavior type in the new system. |
| Hold-to-activate (pass-through hold) | The existing Elden Ring sprint behavior. Users intuitively expect a held input to produce a held output | LOW | Inject on press, release on release. Already implemented; must survive the refactor as the `HOLD_PASSTHROUGH` preset. |
| Hold-to-toggle | A key held past a threshold latches the output on; releasing the trigger key does not release the output. Required for sprint-lock, auto-walk | MEDIUM | Requires per-binding state machine + configurable threshold (ms). The Elden Ring profile uses this pattern. |
| Release-triggered action | Output fires on key release, not key press. Needed for dodge-on-release (existing Elden Ring behavior) | LOW | Already implemented. Must be a named preset: `TRIGGER_ON_RELEASE`. |
| Mouse wheel → key injection | Scroll wheel up/down injects a keyboard key press + release. Existing Toxic Commando behavior | LOW | Already implemented. Must become the `WHEEL_TO_KEY` preset with configurable direction and output key. |
| Process-name game detection | Auto-activates profile when target `.exe` is running (polling ToolHelp32). Users expect seamless switch | MEDIUM | Already implemented. In the data-driven system the process name(s) must be user-configurable, not compiled in. Up to 2 process names per profile (existing architecture limit). |
| Per-profile key bindings | Each game has its own set of input → output assignments. Expected by any user familiar with per-game configs | LOW | `KeyBinding[]` array already exists. Must be persisted to the new data format. |
| Persistent settings | Bindings survive app restart | LOW | Currently INI. Migrating to JSON profile files changes the storage format but the expectation is unchanged. |
| Duplicate-binding conflict detection | If the same source key is assigned to two bindings, behavior is undefined. Users expect a warning | LOW | Currently a known bug (CONCERNS.md §Known Bugs #4). Must be added in the new editor. |
| Disable/enable individual binding | User needs to turn off a specific remap without deleting it | LOW | Not currently supported (CONCERNS.md §Missing Features). Add a per-binding enabled flag to the schema. |

### Differentiators (Competitive Advantage)

Features that set CustomControlZ apart within its niche: a lightweight, zero-dependency, game-specific Win32 remapper with an in-app UI.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| In-app "Add Game" UI (no config file editing) | Tools like AutoHotkey require scripting; reWASD is paid software. CustomControlZ targets users who want a simple UI wizard without writing code or paying | MEDIUM | Core milestone goal. Needs: game name field, process name field(s), behavior-type picker per binding, output key picker. |
| Behavior preset picker (visual, not scripted) | Steam Input activators are powerful but complex. Presets with named types (HOLD_TOGGLE, WHEEL_TO_KEY, etc.) and a small set of parameters are easier to understand | MEDIUM | Dropdown or radio-button selector per binding. Parameters change based on selected type. This is the key UX decision point. |
| Long-press / timed-action preset | Press-duration determines behavior: short press = one action, held past threshold = different action. Differentiates from simple 1:1 remap | MEDIUM | Requires per-binding timer in the logic thread. Add `LONG_PRESS` preset: short tap fires `outputKeyTap`, long hold (> `thresholdMs`) fires `outputKeyHold`. |
| Combo / chord preset | Two simultaneous source keys produce a single output. Useful for games where no single key maps cleanly | HIGH | Requires tracking a second source key state. Add `CHORD` preset: `sourceKey` + `modifierKey` → `outputKey`. Complex to implement correctly with existing GetAsyncKeyState polling loop. |
| Sequence / macro preset | Press a key; inject a timed sequence of key press/release events | HIGH | Rare need for this tool's use cases. Steam Input and AutoHotkey both support this; reWASD calls it "Send Input". Defer to v2. |
| Per-game color theme (existing, data-driven) | Profiles already have themes. Making theme color customizable via UI reinforces the "this is my profile" feeling | LOW | Currently hardcoded per game header. Expose as configurable COLORREF values in the JSON profile. Low complexity since the theme struct is already defined. |
| Turbo / auto-repeat preset | Hold key to rapidly fire repeated taps at a configurable rate | LOW | Simple: in the logic loop, if `TURBO` preset and source key held, inject press+release every N ms. Steam Input and JoyToKey both expose this. Useful for some games. |

### Anti-Features (Deliberately Not Building in v1)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Visual scripting / conditional logic | Power users want "if game state X then remap Y" | Requires a rule engine, state machine, and essentially reimplements AutoHotkey inside a Win32 app. Out of scope (PROJECT.md §Out of Scope). | Presets + parameters cover all stated use cases. Scripting is a v3+ discussion. |
| Multiple simultaneous active profiles | Some users play two games at once, or want global + game-specific remaps layered | Architecture is single-active-profile by design; supporting layers requires complete logic thread redesign | Single profile per game, user switches manually via tray menu. |
| Cloud sync / profile sharing | Users want to share configs with friends | Network dependency violates the zero-dependency, single-exe constraint | Profiles are JSON files next to the exe; users can share files manually. |
| Runtime plugin DLLs for custom behavior | Power users want to inject arbitrary C++ logic | Security risk; defeats the purpose of data-driven profiles; adds DLL versioning complexity | Preset system with parameters is sufficient for the defined behavior types. |
| Gamepad / controller input remapping | Users with controllers expect parity | Requires VirtualHID / ViGEm or similar; significant driver-level complexity; out of stated scope | Tool is keyboard+mouse-only by design. Clearly document this. |
| Profile auto-discovery on startup | Show the last active game automatically | Race condition with game detection; adds statefulness that complicates restart behavior | Manual selection from tray menu is the intended model. |
| Per-binding note/comment field | Power users annotate configs | Nice-to-have; adds UI complexity for minimal value | JSON file is human-readable; users can add comments via external editor if needed (though JSON doesn't support comments natively — use a `"_comment"` field convention). |

---

## Feature Dependencies

```
[Data-driven JSON profile format]
    └──required by──> [In-app Add Game UI]
    └──required by──> [In-app profile editor]
    └──required by──> [Behavior preset picker]
    └──required by──> [Portable profile sharing]

[Behavior preset engine (runtime interpreter)]
    └──required by──> [HOLD_TOGGLE preset]
    └──required by──> [TRIGGER_ON_RELEASE preset]
    └──required by──> [WHEEL_TO_KEY preset]
    └──required by──> [LONG_PRESS preset]
    └──required by──> [CHORD preset]
    └──required by──> [TURBO preset]

[Behavior preset picker (UI)]
    └──depends on──> [Behavior preset engine]
    └──depends on──> [Data-driven JSON profile format]

[In-app Add Game UI]
    └──depends on──> [Data-driven JSON profile format]
    └──enhances──> [Process-name game detection]

[Duplicate-binding conflict detection]
    └──depends on──> [In-app profile editor]

[Disable/enable individual binding]
    └──depends on──> [Data-driven JSON profile format]
    └──enhances──> [In-app profile editor]

[CHORD preset]
    └──conflicts with──> [WHEEL_TO_KEY preset] (on same source key — nonsensical combination)

[Per-game color theme (data-driven)]
    └──enhances──> [In-app Add Game UI]
```

### Dependency Notes

- **JSON profile format is the foundation:** Every other new feature in this milestone depends on the profile schema being defined first. The schema must accommodate all planned preset types and their parameters before any UI or engine work begins.
- **Preset engine before UI:** The behavior interpreter (generic logic thread that reads preset descriptors) must exist before the settings UI can display or configure presets meaningfully.
- **CHORD complexity:** The chord/combo preset requires tracking two source keys simultaneously in the polling loop, introducing state that is unlike all other presets. Implement last, after simpler presets are stable.
- **LONG_PRESS timer isolation:** Long-press requires per-binding timer state that must be reset when the source key is released mid-hold. Keep timer state local to each binding descriptor in the logic thread, not global.

---

## MVP Definition

### Launch With (v1 — this milestone)

Minimum needed to replace the two hardcoded profiles with a fully data-driven system.

- [x] **JSON profile schema** — defines game identity, process names, theme, bindings array, and per-binding behavior type + parameters. All other features depend on this.
- [x] **Preset engine** — generic logic thread that interprets behavior descriptors. Replaces `LogicThreadFn` pointers with a data-driven interpreter. Required presets: `HOLD_PASSTHROUGH`, `HOLD_TOGGLE`, `TRIGGER_ON_RELEASE`, `WHEEL_TO_KEY`.
- [x] **Elden Ring rebuilt as data profile** — validates the preset engine covers hold-to-toggle and release-trigger patterns.
- [x] **Toxic Commando rebuilt as data profile** — validates wheel-to-key pattern; also forces resolution of the placeholder process name bug.
- [x] **In-app Add Game UI** — wizard dialog: game name, process name, per-binding source key + behavior type selector. Creates a new JSON profile file on disk.
- [x] **In-app profile editor** — existing settings window extended to show behavior type and editable parameters per binding.
- [x] **Duplicate-binding conflict detection** — inline warning when user tries to assign an already-used source key.
- [x] **Disable/enable per-binding flag** — toggle in the editor UI; stored in JSON.

### Add After Validation (v1.x)

Add once the core data-driven system is stable.

- [ ] **LONG_PRESS preset** — add after the base preset engine is proven. Adds timer complexity but has clear user demand.
- [ ] **TURBO / auto-repeat preset** — low complexity; add when a user profile needs it.
- [ ] **Per-game color theme customization in UI** — currently hardcoded; exposing it in the Add Game wizard is low effort once the JSON schema supports COLORREF values.
- [ ] **Toxic Commando process name resolution** — can ship the data profile with a placeholder, but must be fixed before v1 is "done".

### Future Consideration (v2+)

Defer until product-market fit established.

- [ ] **CHORD / simultaneous-key preset** — high complexity; not required by any existing profile.
- [ ] **SEQUENCE / macro preset** — requires timing infrastructure beyond what the current polling loop supports cleanly.
- [ ] **Profile import/export UI** — users can share JSON files manually; a UI wrapper is nice-to-have.
- [ ] **Profile search / library** — multiple profiles on disk; UI to discover and manage them. Only needed when users have many profiles.

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| JSON profile schema | HIGH | MEDIUM | P1 |
| Preset engine (4 core types) | HIGH | HIGH | P1 |
| In-app Add Game UI | HIGH | MEDIUM | P1 |
| In-app profile editor (behavior params) | HIGH | MEDIUM | P1 |
| Elden Ring data profile | HIGH | LOW | P1 |
| Toxic Commando data profile | HIGH | LOW | P1 |
| Duplicate-binding detection | MEDIUM | LOW | P1 |
| Disable/enable binding flag | MEDIUM | LOW | P1 |
| LONG_PRESS preset | MEDIUM | MEDIUM | P2 |
| TURBO preset | LOW | LOW | P2 |
| Per-game color theme in UI | LOW | LOW | P2 |
| CHORD preset | MEDIUM | HIGH | P3 |
| SEQUENCE / macro preset | LOW | HIGH | P3 |
| Profile import/export UI | LOW | LOW | P3 |

**Priority key:** P1 = must have for this milestone, P2 = add in v1.x, P3 = v2+

---

## Competitor Feature Analysis

| Feature | AutoHotkey | reWASD | Steam Input | CustomControlZ (planned) |
|---------|------------|--------|-------------|--------------------------|
| Hold-to-toggle | Script only | Yes (activators) | Yes (toggle flag on activator) | HOLD_TOGGLE preset — data-driven |
| Release-triggered action | Script only | Yes (Release Press activator) | Yes (Release Press activator) | TRIGGER_ON_RELEASE preset |
| Long press (timed) | Script only | Yes | Yes (Long Press activator, 0–1s) | LONG_PRESS preset (v1.x) |
| Double tap | Script only | Yes (activators) | Yes (Double Press activator) | Not planned — low demand for this use case |
| Turbo / auto-repeat | Script only | Yes | Yes (Hold to Repeat activator) | TURBO preset (v1.x) |
| Chorded / simultaneous | Yes (& operator) | Yes | Yes (Chorded Press activator) | CHORD preset (v2+) |
| Sequence / macro | Yes (full scripting) | Yes (Send Input feature) | Limited | SEQUENCE preset (v2+) |
| Mouse wheel → key | Script only | Yes (from v5.5.1) | Partial (as action layer) | WHEEL_TO_KEY preset — existing behavior |
| Game-specific auto-activation | Script (process watch) | Yes (Autodetect feature) | Via Steam launch | Core feature — process name in profile |
| In-app profile editor | No (text editor) | Yes (GUI, paid) | Yes (Steam overlay) | Core goal of this milestone |
| Zero install / portable | No | No (service + driver) | No (requires Steam) | Yes — single exe + JSON files |
| Free | Free (open) | Paid ($6/yr+) | Free (Steam required) | Free |
| Win32 / no dependencies | No (.NET) | No (driver) | No (Steam) | Yes — C++20, Win32 only |

**Key insight:** CustomControlZ's competitive advantage is not feature breadth — AutoHotkey and reWASD both outclass it on raw features. The advantage is portability (single exe, no install, no subscription), simplicity (preset-based not scripted), and game-specific targeting (profiles auto-activate per game). The feature set must stay focused on making those advantages shine, not on catching up to reWASD.

---

## Behavior Preset Type Reference

This section defines each preset type for the schema and engine, synthesized from competitor analysis.

### HOLD_PASSTHROUGH
**What it does:** While source key is held, output key is held. On release, output is released.
**Parameters:** `sourceVk`, `outputVk`
**Existing behavior:** Yes (simplest case of existing remap)
**Complexity:** LOW

### HOLD_TOGGLE
**What it does:** Source key held past `thresholdMs` latches output on. Output stays on until source key is pressed again (toggle off).
**Parameters:** `sourceVk`, `outputVk`, `thresholdMs` (default: 300)
**Existing behavior:** Yes — Elden Ring sprint uses this pattern
**Complexity:** MEDIUM
**State required:** `bool toggleOn`, `DWORD holdStartTime` per binding

### TRIGGER_ON_RELEASE
**What it does:** Output key is injected (tap: press + release) when source key is released.
**Parameters:** `sourceVk`, `outputVk`
**Existing behavior:** Yes — Elden Ring dodge-on-release uses this
**Complexity:** LOW
**State required:** `bool wasDown` per binding

### WHEEL_TO_KEY
**What it does:** Mouse wheel movement in the configured direction triggers a tap of the output key.
**Parameters:** `direction` (UP | DOWN), `outputVk`
**Existing behavior:** Yes — Toxic Commando uses this
**Complexity:** LOW
**Note:** Wheel events are discrete (each tick = one scroll notch). No hold state needed.

### LONG_PRESS (v1.x)
**What it does:** Short tap (< `thresholdMs`) fires `outputVkTap`; hold past threshold fires `outputVkHold` (and holds it until source key release).
**Parameters:** `sourceVk`, `outputVkTap`, `outputVkHold`, `thresholdMs` (default: 500)
**Complexity:** MEDIUM
**State required:** `bool holdFired`, `DWORD holdStartTime` per binding

### TURBO (v1.x)
**What it does:** While source key is held, output key fires repeated tap-press-release at `repeatRateMs` interval.
**Parameters:** `sourceVk`, `outputVk`, `repeatRateMs` (default: 100)
**Complexity:** LOW
**State required:** `DWORD lastFireTime` per binding

### CHORD (v2+)
**What it does:** Source key + modifier key held simultaneously triggers output key.
**Parameters:** `sourceVk`, `modifierVk`, `outputVk`
**Complexity:** HIGH
**Note:** Requires careful handling to prevent raw source key passthrough while waiting for chord confirmation.

---

## Sources

- [Steam Input Activator Types — Steamworks Documentation](https://partner.steamgames.com/doc/features/steam_controller/activators) — HIGH confidence (official Valve documentation)
- [reWASD Autodetect — reWASD Help Guide](https://help.rewasd.com/basic-functions/autodetect.html) — HIGH confidence (official product documentation)
- [reWASD Workspace UI — reWASD Help Guide](https://help.rewasd.com/interface/workspace.html) — HIGH confidence (official product documentation)
- [reWASD Mouse Wheel Remapping — reWASD Release 5.5.1](https://www.rewasd.com/releases/release-5.5.1) — HIGH confidence (official release notes)
- [JoyToKey Advanced Features](https://joytokey.net/en/advanced) — MEDIUM confidence (official but sparse)
- [AutoHotkey Remapping Docs](https://www.autohotkey.com/docs/v1/misc/Remap.htm) — HIGH confidence (official docs)
- [Karabiner-Elements Complex Modifications](https://karabiner-elements.pqrs.org/docs/manual/configuration/configure-complex-modifications/) — HIGH confidence (official docs, macOS-only but behavior taxonomy is portable)
- [PCGamer: Steam Activators Update](https://www.pcgamer.com/customise-every-button-press-with-the-steam-controllers-activators-update/) — MEDIUM confidence (trade press)
- CustomControlZ codebase analysis (ARCHITECTURE.md, CONCERNS.md, PROJECT.md) — HIGH confidence (first-party)

---

*Feature research for: data-driven game input remapping (CustomControlZ milestone)*
*Researched: 2026-03-11*
