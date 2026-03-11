# Architecture Research

**Domain:** Win32 input remapping engine — data-driven behavior interpreter
**Researched:** 2026-03-11
**Confidence:** HIGH (codebase read directly; patterns derived from existing code and verified analogues)

---

## Standard Architecture

### System Overview

The target architecture replaces the compiled-in `LogicThreadFn` with a generic behavior
interpreter that reads behavior descriptors from profile data and dispatches to stateful
behavior handlers on each tick.

```
┌──────────────────────────────────────────────────────────────────┐
│  UI Layer (Main Thread — unchanged)                              │
│  Win32 message pump, tray, game select, settings windows        │
├──────────────────────────────────────────────────────────────────┤
│  Profile Registry (Runtime — replaces compile-time array)        │
│  JSON loader → ProfileRegistry → GameProfile (extended)         │
│  Each profile: metadata + theme + BehaviorDescriptor[]          │
├──────────────────────────────────────────────────────────────────┤
│  Behavior Engine (Logic Thread — new generic LogicThreadFn)      │
│  ┌──────────────┐  ┌────────────────┐  ┌─────────────────────┐  │
│  │ Input Poller │→ │ Behavior Runner │→ │  Output Dispatcher  │  │
│  │              │  │  (per-behavior  │  │  SendInput wrapper  │  │
│  │ GetAsyncKey  │  │   state machine)│  │  keyboard + mouse   │  │
│  │ state        │  └────────────────┘  └─────────────────────┘  │
│  └──────────────┘                                               │
├──────────────────────────────────────────────────────────────────┤
│  Persistence Layer (Extended)                                    │
│  profiles/<id>.json  (behavior descriptors, bindings, theme)    │
│  settings.ini        (legacy; kept for app-level prefs)         │
└──────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Boundary |
|-----------|----------------|----------|
| `ProfileLoader` | Parse JSON profile files into `GameProfile` + `BehaviorDescriptor[]` | Owns file I/O; no Windows input |
| `ProfileRegistry` | Maintain list of known profiles; replace `g_gameProfiles[]` | Owns profile lifetime |
| `BehaviorDescriptor` | Data-only struct: behavior type enum + parameters (threshold ms, output VK, direction) | Pure data; no logic |
| `BehaviorState` | Per-descriptor mutable runtime state (key-was-down bools, timestamps, toggle state) | Lives in logic thread only |
| `BehaviorRunner` | On each tick: for each descriptor, evaluate input → update state → emit output events | No direct Win32 I/O |
| `InputPoller` | Snapshot `GetAsyncKeyState` for all relevant VKs; return `InputSnapshot` struct | No behavior logic |
| `OutputDispatcher` | Convert output events to `SendInput` calls (keyboard or mouse wheel) | Only component that calls `SendInput` |
| Generic `LogicThreadFn` | Tick loop: poll → run behaviors → dispatch; replaces per-game functions | Owns timing/sleep |

---

## Data Flow

### Startup: Profile Data → Runtime Structs

```
profiles/<id>.json
    ↓ ProfileLoader::Load()
GameProfile (metadata, theme, BehaviorDescriptor[])
    ↓ ProfileRegistry::Register()
g_gameProfiles[] equivalent (runtime list)
    ↓ user selects game
StartGameLogicThread(profile)
    ↓ GenericLogicThread(profile, running)
BehaviorState[] allocated (one per descriptor, stack or heap)
```

### Per-Tick: Poll → Interpret → Inject

```
GetAsyncKeyState × all bound VKs
    ↓ InputPoller::Snapshot()
InputSnapshot { map<WORD, bool> keyDown }
    ↓ BehaviorRunner::Tick(snapshot, descriptors[], states[])
OutputEvent[] { type: key/mouse_wheel, vk/delta, press/release }
    ↓ OutputDispatcher::Send(events[])
SendInput() calls (keyboard INPUT or MOUSEEVENTF_WHEEL)
```

### Rebind: UI → Profile → Logic Thread

```
User assigns new VK in SettingsProc
    ↓ g_configMutex locked
descriptor.currentVk updated (same mutex pattern as today)
    ↓ SaveProfile(profile)  ← writes JSON
Next tick: InputPoller uses updated VK automatically
```

---

## Behavior Descriptor Schema

Each binding in a profile maps to exactly one `BehaviorDescriptor`. This replaces the
implicit per-game logic with explicit typed parameters.

```cpp
enum class BehaviorType : uint8_t {
    HoldToToggle,     // hold triggerVk → press/hold outputVk while held
    EdgeTrigger,      // tap triggerVk → single pulse of outputVk (dodge pattern)
    LongPress,        // hold triggerVk >= thresholdMs → fire actionVk once
    WheelToKey,       // tap triggerVk → send mouse wheel delta
    // Extensible: add types without touching engine tick loop
};

struct BehaviorDescriptor {
    BehaviorType type;
    WORD         triggerVk;    // input key (from binding.currentVk)
    WORD         outputVk;     // primary output key (keyboard)
    int          thresholdMs;  // long-press delay, or 0 if unused
    int          pulseDurationMs; // how long to hold outputVk on pulse, or 0
    DWORD        wheelDelta;   // for WheelToKey: +120 or -120, else 0
};
```

The Elden Ring profile covers all four types:
- Sprint key → `HoldToToggle` (hold sprint VK → hold ingame VK)
- Dodge key → `EdgeTrigger` (tap dodge VK → pulse ingame VK)
- Trigger key → `LongPress` (hold escape VK ≥ 400ms → fire 'Q')
- (No WheelToKey needed — Toxic Commando provides that pattern)

---

## BehaviorRunner: Per-Type State Machines

Each behavior type has a small, flat state machine. No virtual dispatch needed — a
`switch` on `BehaviorType` with a union or parallel state struct is sufficient given the
fixed set of types and the no-STL-containers constraint.

### HoldToToggle state machine

```
States: IDLE → ACTIVE
IDLE:   triggerVk down? → PressKey(outputVk), transition ACTIVE
ACTIVE: triggerVk still down? → stay ACTIVE
        triggerVk released? → ReleaseKey(outputVk), transition IDLE
Cleanup: if ACTIVE when thread stops → ReleaseKey(outputVk)
```

### EdgeTrigger state machine

```
States: UP → PRESSED → UP
UP:      triggerVk down? → PressKey(outputVk), Sleep(pulseDurationMs),
                           ReleaseKey(outputVk), transition PRESSED
PRESSED: triggerVk still down? → stay PRESSED (no re-fire)
         triggerVk released? → transition UP
```

Note: EdgeTrigger with a competing HoldToToggle on the same outputVk (Elden Ring
dodge-while-sprinting) requires ordering — evaluate EdgeTrigger first, then HoldToToggle
restores state. The descriptor array ordering controls this.

### LongPress state machine

```
States: UP → TIMING → FIRED → UP
UP:     triggerVk down? → record startTime, transition TIMING
TIMING: triggerVk still down AND (now - startTime) >= thresholdMs?
          → PressKey(actionVk), Sleep(pulseDurationMs), ReleaseKey(actionVk), transition FIRED
        triggerVk released before threshold? → transition UP
FIRED:  triggerVk still down? → stay FIRED (no repeat)
        triggerVk released? → transition UP
```

### WheelToKey state machine

```
States: UP → PRESSED
UP:      triggerVk down? → SendMouseWheel(wheelDelta), transition PRESSED
PRESSED: triggerVk still down? → stay PRESSED (no repeat)
         triggerVk released? → transition UP
```

---

## BehaviorState: Runtime State Per Descriptor

```cpp
struct BehaviorState {
    bool      keyWasDown;       // edge detection (all types)
    bool      toggleActive;     // HoldToToggle output currently active
    bool      actionFired;      // LongPress already fired this press
    ULONGLONG pressStartTime;   // LongPress: GetTickCount64() at key-down
};
// One BehaviorState per BehaviorDescriptor, same index.
```

No heap allocation required: `BehaviorState states[MAX_BINDINGS]` on the logic thread
stack, zeroed at thread start. Matches existing stack-allocated state pattern in
EldenRing.h.

---

## Component Boundaries: What Knows What

| Knows about... | ProfileLoader | BehaviorDescriptor | BehaviorRunner | InputPoller | OutputDispatcher | GenericLogicThread |
|----------------|:---:|:---:|:---:|:---:|:---:|:---:|
| File I/O / JSON | YES | no | no | no | no | no |
| Win32 `GetAsyncKeyState` | no | no | no | YES | no | no |
| `SendInput` | no | no | no | no | YES | no |
| Behavior logic / state machines | no | no | YES | no | no | no |
| Threading / sleep / tick | no | no | no | no | no | YES |
| Profile data structs | YES | IS | reads | no | no | owns ptr |

This separation means each component can be tested, changed, or extended without
touching the others. Adding a new `BehaviorType` requires: enum value + case in
`BehaviorRunner::Tick()` only. No new files, no new threads.

---

## Integration Path with Existing Code

### What stays unchanged

- `GameProfile` struct identity (id, displayName, iniSection, processName1/2, tipActive/tipIdle, settingsTitle, theme)
- `KeyBinding` struct (`iniKey`, `label`, `defaultVk`, `currentVk`) — reused verbatim
- `g_configMutex` / `g_waitingForBindID` / `g_logicRunning` threading primitives
- `IsGameRunning()`, `SetTrayIconState()`, `PressKey()`, `ReleaseKey()`, `IsKeyDown()` helpers
- `StartGameLogicThread()` / `StopGameLogicThread()` call sites — signature unchanged
- All UI layer code: `SettingsProc`, `GameSelectProc`, `WindowProc`, tray handling

### What changes

| What | Before | After |
|------|--------|-------|
| `GameProfile.logicFn` | Per-game static function | Single `GenericLogicThread` function for all profiles |
| `GameProfile.bindings[]` | `KeyBinding` only | `KeyBinding` + parallel `BehaviorDescriptor[]` |
| Profile source | Hardcoded `.h` files compiled in | JSON files in `profiles/` directory |
| Profile registry | `g_gameProfiles[]` static array in `.cpp` | Runtime-populated array from `ProfileLoader` |
| `settings.ini` binding storage | Binding VKs per profile section | JSON profile file (or hybrid: JSON for behavior, ini for VKs) |

### Recommended migration sequence

```
1. Add BehaviorDescriptor struct + BehaviorType enum to GameProfiles.h
   (no behavior change yet — existing profiles still compile)

2. Add BehaviorState struct

3. Implement GenericLogicThread as a new function in a new file
   (BehaviorRunner + InputPoller + OutputDispatcher all inline in it initially)
   Test by assigning it to one existing profile's logicFn

4. Implement ProfileLoader (JSON → GameProfile + BehaviorDescriptor[])
   Use nlohmann/json or hand-rolled parser (no external deps constraint)

5. Port Elden Ring → profiles/eldenring.json; remove EldenRing.h
   Port Toxic Commando → profiles/toxiccommando.json; remove ToxicCommando.h

6. Replace g_gameProfiles[] static array with ProfileRegistry populated at startup
   from all .json files found next to the .exe

7. Extend SettingsProc / add UI for configuring behavior type per binding
```

---

## Recommended File Structure

```
CustomControlZ/
├── GameProfiles.h          # GameProfile, KeyBinding, Theme (extended with BehaviorDescriptor)
├── BehaviorTypes.h         # BehaviorType enum, BehaviorDescriptor, BehaviorState structs
├── GenericLogicThread.h    # GenericLogicThread function (replaces per-game .h files)
├── ProfileLoader.h/.cpp    # JSON → GameProfile loading
├── CustomControlZ.cpp      # Main file (unchanged except profile registry)
└── games/                  # Removed once migration complete; EldenRing.h, ToxicCommando.h deleted

profiles/                   # Next to the .exe at runtime
├── eldenring.json
└── toxiccommando.json
```

The games/ directory is deleted as profiles migrate. GenericLogicThread.h follows the
existing header-only convention for the logic thread.

---

## Architectural Patterns

### Pattern 1: Data-only Descriptor with Parallel State Array

**What:** Separate the behavior specification (immutable data read from profile) from the
runtime state (mutable, per-tick). Two parallel arrays indexed by binding slot: one
`BehaviorDescriptor[]` loaded once, one `BehaviorState[]` zeroed at thread start.

**When to use:** When behavior types are fixed and finite, number of behaviors per
profile is small (≤8), and no heap allocation is desired.

**Trade-offs:** Simple, zero-allocation, debuggable. Inflexible if behaviors need to be
composable at runtime (not a requirement here).

```cpp
// In GenericLogicThread:
BehaviorDescriptor descs[MAX_BINDINGS];  // populated from profile
BehaviorState      states[MAX_BINDINGS] = {};  // zeroed
// ... tick loop:
for (int i = 0; i < profile->bindingCount; i++) {
    RunBehavior(descs[i], states[i], snapshot, events);
}
```

### Pattern 2: Switch-dispatch on Behavior Type (no virtual dispatch)

**What:** `RunBehavior()` is a plain function with a `switch (desc.type)` dispatching to
inline state machine logic per type. No virtual functions, no `std::function` overhead
inside the hot loop.

**When to use:** Small, fixed enum of types; single-threaded dispatch; no-external-dep
constraint.

**Trade-offs:** Adding a new type requires modifying `RunBehavior()`. Acceptable because
the set of behavior types is intentionally limited and adding one is a deliberate design
decision, not a user action.

### Pattern 3: Output Event Buffer (deferred SendInput)

**What:** `RunBehavior()` writes to an `OutputEvent[]` buffer rather than calling
`SendInput` directly. After all behaviors run, `OutputDispatcher` flushes the buffer.
This mirrors how cpp-remapper batches control changes atomically.

**When to use:** When multiple behaviors may need to coordinate output (e.g., Elden Ring's
sprint-release-then-dodge sequence). Also isolates `SendInput` to one call site.

**Trade-offs:** Adds one indirection. For behaviors that need to `Sleep()` mid-sequence
(EdgeTrigger pulse duration), the sleep must happen inside RunBehavior before emitting the
release event, so "deferred" is logical not temporal.

---

## Anti-Patterns

### Anti-Pattern 1: One LogicThreadFn per Game (current state)

**What people do:** Write custom C++ logic for each game profile; compile it in.
**Why it's wrong:** Every new profile requires a rebuild. The behaviors are duplicated
across profiles with minor parameter variations. The system cannot support user-created
profiles at runtime.
**Do this instead:** Identify the recurring state machine patterns (they are always
HoldToToggle, EdgeTrigger, LongPress, or WheelToKey), parameterize them, interpret at
runtime.

### Anti-Pattern 2: Calling SendInput Inside BehaviorRunner Directly

**What people do:** Call `PressKey()` / `ReleaseKey()` inline inside the behavior state
machine logic, interleaved with `Sleep()` calls.
**Why it's wrong:** Tightly couples behavior logic to Win32 output. Makes it impossible
to unit-test behavior logic without actually injecting input. Behaviors that need
coordinated output (sprint + dodge) require careful ordering of direct calls.
**Do this instead:** Emit output events from the runner; let the OutputDispatcher handle
the actual `SendInput` call. For sequences requiring Sleep mid-action, the sleep stays in
the runner but the I/O call is still isolated.

Note: The existing codebase does call `PressKey()` directly — this anti-pattern is called
out because the new generic engine is the right place to enforce the boundary.

### Anti-Pattern 3: JSON Parser with Heap Allocation per Tick

**What people do:** Parse or query profile JSON on each logic tick to read binding values.
**Why it's wrong:** The logic tick runs every 10ms; file I/O or heap allocation in the
hot path would cause jitter.
**Do this instead:** Parse JSON once at profile load time into the `BehaviorDescriptor[]`
array. Binding VK changes (rebind) are applied to the descriptor's `triggerVk` field
under the existing `g_configMutex`, same as `currentVk` today.

### Anti-Pattern 4: Behavior State in the Descriptor (mixing data and state)

**What people do:** Put `keyWasDown`, `pressStartTime`, `toggleActive` directly into
`BehaviorDescriptor` so there is only one struct to pass around.
**Why it's wrong:** Descriptors come from profile data and may eventually be shared or
reloaded. Mutable state embedded in them makes reload/reset logic fragile and requires
mutex protection for state that only the logic thread touches.
**Do this instead:** Keep `BehaviorDescriptor` const after load. Keep `BehaviorState` as
a logic-thread-only stack variable, zeroed on thread start.

---

## Build Order (What Must Exist Before What)

```
1. BehaviorTypes.h
   (BehaviorType enum, BehaviorDescriptor, BehaviorState)
   — no dependencies

2. GenericLogicThread.h
   (imports BehaviorTypes.h + existing GameProfiles.h helpers)
   — depends on (1) and existing GameProfiles.h

3. Hard-coded descriptors in existing profile structs for smoke test
   (assign GenericLogicThread to EldenRing/ToxicCommando logicFn; define descriptors inline)
   — depends on (1)(2); proves engine works before touching persistence

4. ProfileLoader.h/.cpp
   (JSON → GameProfile + BehaviorDescriptor[])
   — depends on (1); can be developed in parallel with (3)

5. profiles/eldenring.json + profiles/toxiccommando.json
   — depends on (4)

6. Runtime ProfileRegistry (replace g_gameProfiles[])
   — depends on (4)(5); requires changes to wWinMain startup

7. UI: behavior type selector in SettingsProc / new game editor
   — depends on (6); last because it requires profiles to be runtime-loadable
```

---

## Scaling Considerations

This is a single-user desktop tool; traditional scaling tiers do not apply. The relevant
scaling axes are "number of profiles" and "number of behaviors per profile."

| Constraint | Current Limit | With New Engine |
|------------|--------------|----------------|
| Profiles per install | 2 (compiled) | Unlimited (one JSON per file) |
| Behaviors per profile | 8 (MAX_BINDINGS) | 8 (same cap; easily raised) |
| Behavior types | 2 implemented | 4 initially; O(1) to add more |
| Memory | Stack-allocated state | Stack-allocated; ~10 bytes per behavior slot |
| CPU (10ms tick) | ~10ms sleep dominates | Unchanged; behavior loop for 8 items is negligible |

MAX_BINDINGS = 8 remains a hard cap in this design. Raising it to 16 or 32 is a
one-line change and does not affect any other component.

---

## Integration Points

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| UI ↔ BehaviorDescriptor | `g_configMutex` protects `descriptor.triggerVk` writes | Same pattern as today's `currentVk`; only the VK field changes at runtime |
| ProfileLoader ↔ ProfileRegistry | ProfileLoader returns `GameProfile` by value; registry owns lifetime | No callbacks; simple ownership |
| BehaviorRunner ↔ OutputDispatcher | `OutputEvent[]` buffer passed by reference | OutputDispatcher is a thin wrapper; no state |
| GenericLogicThread ↔ existing thread primitives | `g_logicRunning`, `g_waitingForBindID`, `g_configMutex` | No changes to synchronization model |

### External Boundary: JSON Profile Files

- Location: directory of the .exe (matches `settings.ini` convention)
- Format: JSON (hand-rolled parser acceptable given no-external-dep constraint; or
  bundled single-header nlohmann/json as a header-only include)
- On missing/malformed file: fall back to default bindings, log to debug output
- On missing profiles/ directory: show empty game list (not a crash)

---

## Sources

- Codebase direct analysis: `CustomControlZ/games/EldenRing.h`, `ToxicCommando.h`,
  `GameProfiles.h`, `CustomControlZ.cpp` (read 2026-03-11)
- cpp-remapper (fredemmott): Source/Sink pattern, ShortPressLongPress, MomentaryToLatchedButton
  abstractions — confirms behavioral decomposition approach
  https://github.com/fredemmott/cpp-remapper
- reWASD behavior model: Activators for single/double/long-press per button — confirms
  behavior-type enum approach is industry standard for remappers
  https://help.rewasd.com/types-of-mappings/commands.html
- AutoHotkey v2 dispatch model: hook-based input capture, per-hotkey variant state —
  confirms edge-triggered model and rebind-pause pattern
  https://www.autohotkey.com/docs/v2/lib/Hotkey.htm

---

*Architecture research for: CustomControlZ data-driven behavior engine*
*Researched: 2026-03-11*
