# Pitfalls Research

**Domain:** Win32 input remapping — data-driven game profile system with behavior engine
**Researched:** 2026-03-11
**Confidence:** HIGH (UIPI/SendInput, threading, key-stuck issues confirmed via official docs + community evidence); MEDIUM (anti-cheat detection specifics, as policies evolve); LOW (specific EAC/BattlEye behavior-engine detection thresholds)

---

## Critical Pitfalls

### Pitfall 1: UIPI Blocks SendInput When Game Runs Elevated

**What goes wrong:**
When a game runs with elevated privileges (UAC "Run as administrator") and CustomControlZ runs at medium integrity (normal user launch), Windows UIPI silently discards all `SendInput` calls targeting the elevated process. The return value of `SendInput` is 0 and `GetLastError` returns `ERROR_ACCESS_DENIED` (5) — but many tools do not check this, so injection appears to work while doing nothing. The existing codebase does not currently check the `SendInput` return value.

**Why it happens:**
UIPI (User Interface Privilege Isolation, Windows Vista+) prevents lower-integrity processes from injecting input into higher-integrity processes. Most games do not require elevation, but some launchers and anti-cheat clients do elevate the game executable. Developers assume SendInput always works because it works during development, where they typically run both processes at the same level.

**How to avoid:**
- Check `SendInput` return value on every call; log failures.
- On persistent zero-injection (all calls return 0), surface a diagnostic to the user: "Game may be running elevated — try launching CustomControlZ as administrator."
- Do not auto-elevate silently. Require the user to choose (tray menu "Restart as Administrator" option).
- Document the known limitation in the in-app profile editor UI.

**Warning signs:**
- `SendInput` returns 0 but `GetLastError` is `ERROR_ACCESS_DENIED`.
- Remapping appears active (tray shows green) but no in-game effect occurs.
- Affects games launched via launchers that self-elevate (some anti-cheat setups).

**Phase to address:**
Behavior engine phase. The generic `SendInput` wrapper in the engine is the right place to add return-value checking and diagnostic emission. Do not defer to a polish phase — silent failure is very hard to debug later.

---

### Pitfall 2: Injected Key Left Pressed on App Exit or Crash

**What goes wrong:**
If the logic thread is mid-behavior (e.g., holding a sprint key, or in the middle of a combo sequence) when the app exits or crashes, the synthesized key remains pressed at the OS input queue level. The game then receives a continuous keypress with no corresponding release. The existing EldenRing profile already has a partial mitigation in the happy path (`sprintActive` cleanup on game exit), but force-close and crash paths are unguarded. This becomes more dangerous with a generic behavior engine managing multiple held keys simultaneously across multiple behavior types.

**Why it happens:**
`SendInput` injects a KEYDOWN event into the input queue but does not track that it needs a corresponding KEYUP. If the thread exits without sending KEYUP, the key is stuck. This is universally documented in AutoHotkey community discussions as one of the most common persistent bugs in remapping tools.

**How to avoid:**
- The behavior engine must maintain a list of "currently injected pressed keys."
- On any exit path — normal shutdown, `g_logicRunning = false`, exception in the logic thread, `std::terminate` — iterate the list and send KEYUP for all tracked keys before the thread terminates.
- Use RAII: wrap the "pressed keys" set in a scope guard or destructor that fires the KEYUP events on destruction.
- Test explicitly: terminate app via Task Manager while a behavior is active and verify no stuck keys.

**Warning signs:**
- Playtesters report keys "getting stuck" after switching profiles or closing the app.
- The game continues moving/acting as if a key is held after CustomControlZ is closed.
- The existing `sprintActive` path in `EldenRing.h` is the correct pattern — the bug risk is forgetting to carry this pattern into the generic engine.

**Phase to address:**
Behavior engine phase. This must be solved before any behavior type is considered complete. "Cleanup on all exit paths" is a go/no-go criterion for engine correctness.

---

### Pitfall 3: Profile Hot-Edit Race Condition — Partial Read of Behavior Parameters

**What goes wrong:**
When the user edits a binding's behavior type or parameters in the profile editor UI while the logic thread is executing, the logic thread can read a partially-updated behavior descriptor. Example: user changes a binding from "hold-to-sprint" to "mouse-wheel-to-key" mid-edit. The logic thread reads the new `behaviorType` enum value but still sees the old `outputKey` or `thresholdMs` from the previous behavior. This produces incorrect input injection — potentially injecting wrong keys or infinite loops.

**Why it happens:**
The existing codebase uses `g_configMutex` to protect `currentVk` reads, which is correct for the current simple bindings. But a behavior descriptor with multiple fields (type enum + multiple parameters) is not atomically updatable under a single mutex acquisition if the UI writes fields individually. If the profile editor writes `behaviorType` in one `lock_guard` scope and `thresholdMs` in another, the logic thread can observe the intermediate state.

**How to avoid:**
- Define a `BehaviorDescriptor` struct that packages ALL parameters for one binding.
- The UI writes a complete new `BehaviorDescriptor` value under a single `lock_guard` acquisition — never update individual fields one by one.
- The logic thread snapshots the complete descriptor under the mutex at the top of each tick, then works only on its local copy.
- This is the "snapshot + work on local copy" pattern already used for `currentVk` — extend it to the full descriptor.
- Consider using `std::atomic<std::shared_ptr<ProfileData>>` for lock-free swapping of the entire profile at once (create-new, atomic-swap, old ref drops when no thread holds it).

**Warning signs:**
- Intermittent wrong key injections when editing a binding that is actively in use.
- Logic thread behavior differs depending on timing of UI edits.
- Unit-testable: inject a fake "half-written" descriptor and verify the engine handles it or locks it out correctly.

**Phase to address:**
Behavior engine phase (define the data contract). Profile editor phase (enforce that writes are atomic at the descriptor level).

---

### Pitfall 4: GetAsyncKeyState Polling at 10ms Misses Very Short Keypresses

**What goes wrong:**
`GetAsyncKeyState` has a "high-order bit set + low-order bit latched" behavior: it will not miss a keypress that happened between polls as long as the key was pressed and released entirely between polls AND the low-order "was pressed since last call" bit is honored. However, the existing codebase checks only the high-order bit (key currently down), not the low-order latch bit. A very fast tap (< 10ms) that completes between polls will be silently missed. For combo/sequence behaviors that require precise tap detection, this is a correctness bug.

**Why it happens:**
`GetAsyncKeyState` documentation describes the low-order bit behavior, but it is commonly misread or ignored. Most simple "is key held" checks only test `& 0x8000`, which is correct for hold-detection but wrong for tap-detection. The 10ms polling interval means keys shorter than one tick are invisible to hold-only detection.

**How to avoid:**
- For **hold-based behaviors** (hold-to-sprint, hold-to-toggle): high-order bit only is sufficient.
- For **tap/combo behaviors**: check `& 0x0001` (was-pressed latch) OR check `& 0x8000` — combine both for robustness: `(GetAsyncKeyState(vk) & 0x8001) != 0`.
- Note: the latch bit is process-specific and cleared on the next `GetAsyncKeyState` call for that key. The logic thread already owns this polling, so there is no contention issue.
- Document explicitly in the behavior engine which behaviors require latch detection vs. hold detection.

**Warning signs:**
- Combo behaviors miss occasional inputs on fast typists or mechanical keyboards with fast actuation.
- Tap-to-toggle behaviors occasionally fail to register.
- Testing: bind a behavior to a key and tap it very quickly; count misses at normal and fast rates.

**Phase to address:**
Behavior engine phase. The polling strategy must be chosen per-behavior-type when behavior descriptors are defined.

---

### Pitfall 5: Anti-Cheat Detection of Synthetic Input via SendInput

**What goes wrong:**
`SendInput` sets the `LLMHF_INJECTED` flag on injected input events. Anti-cheat systems (EAC, BattlEye, Vanguard) monitor for this flag. BattlEye documentation explicitly mentions "suspicious input macros" as a detection category. EAC uses behavioral analysis of input patterns. A tool that injects regular, metronomic input at 10ms intervals with identical timing exhibits machine-pattern characteristics that anti-cheat heuristics are tuned to detect. The risk is real but not guaranteed — detection policies vary by game and anti-cheat version.

**Why it happens:**
`SendInput` was designed for accessibility tools and automation, not for undetected input injection. The injected flag exists precisely so processes can distinguish synthetic from physical input. Anti-cheat vendors detect both the flag and timing patterns.

**How to avoid:**
- This is a **documentation and user-expectation pitfall**, not a code fix. There is no reliable way to make `SendInput` undetectable while staying within the Win32 API (driver-level injection is out of scope and violates the constraint of no new runtime dependencies).
- The profile editor UI should surface a clear disclaimer: "This tool injects synthetic input. Use with games that have aggressive anti-cheat (EAC, BattlEye) at your own risk."
- Do not add timing jitter or randomization to evade anti-cheat — this changes the tool's purpose and does not guarantee safety.
- The in-app "Add Game" UI should let users flag a game as "anti-cheat present" and show a persistent warning.

**Warning signs:**
- User reports of bans in games using EAC or BattlEye should be expected and documented, not treated as bugs to fix.
- If the tool is used with Elden Ring (EAC), risk exists — but the existing tool has operated without reports of bans, likely because hold-to-sprint is indistinguishable from a modifier key held by the player.

**Phase to address:**
Profile editor phase. The "Add Game" UI is where risk warnings belong. The behavior engine phase should not attempt anti-detection measures.

---

### Pitfall 6: Behavior Engine State Not Reset When Profile Switches Mid-Behavior

**What goes wrong:**
When the user switches from one game profile to another (or the game exits mid-behavior), the generic behavior engine may retain state from the previous execution: toggle flags set to "on," combo sequence progress partially advanced, hold timers ticking. The new profile activates with stale state from the previous one, producing immediate spurious input injection.

**Why it happens:**
Each behavior type in a data-driven engine maintains runtime state (e.g., `bool toggleActive`, `int comboStep`, `DWORD holdStartTime`). When the profile switches, `StopGameLogicThread` terminates the old thread and `StartGameLogicThread` starts a new one. If the runtime state lives in the behavior descriptor objects that are shared across profile loads, state leaks between sessions. If state lives in the thread function's local stack, it is naturally reset — but only if the entire thread function is restarted.

**How to avoid:**
- Runtime behavior state (toggle flags, timer offsets, combo progress) must be stored in objects that are created fresh per logic thread launch, not in the profile descriptor itself.
- The profile descriptor holds only static configuration (behavior type, parameters). Runtime state is in a separate `BehaviorState` object, allocated when the thread starts, destroyed when it stops.
- On `StopGameLogicThread`, verify all injected keys are released (see Pitfall 2) before the state objects are freed.

**Warning signs:**
- After switching games, the new game profile immediately injects input that was not triggered by the user.
- Toggle behaviors start in the "active" state when they should start inactive.
- Integration test: switch profiles twice in rapid succession while a behavior is active.

**Phase to address:**
Behavior engine phase. The separation of "static descriptor" from "runtime state" is a fundamental engine design decision that must be made before implementing any behavior type.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Keep all game profile state in one struct (merging config + runtime state) | Simpler code initially | State leaks between sessions (Pitfall 6); impossible to reset cleanly | Never — separate descriptors from runtime state from the start |
| Skip `SendInput` return value checking | Less error-handling code | Silent failures when game is elevated (Pitfall 1); impossible to diagnose | Never in the behavior engine — check on every call |
| Write behavior parameter fields individually under separate mutex acquisitions | Simpler UI code | Partial-read race conditions (Pitfall 3) | Never — always write a complete descriptor atomically |
| Use `& 0x8000` only for all behavior types | Simpler polling code | Missed taps for combo behaviors (Pitfall 4) | Acceptable only for hold-only behaviors; must be per-behavior-type |
| Extend `settings.ini` with new keys for behavior parameters | No new file format to parse | INI has no nesting support, 32KB size limit, no Unicode support; behavior descriptors with multiple parameters become unmaintainable key soup | Never for complex nested data — migrate to JSON at the start of this milestone |
| Hardcode the profile list in the new data-driven engine | Faster initial ship | Defeats the entire "add game without recompiling" goal | Never |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| `SendInput` + elevated game | Assume injection works because `SendInput` doesn't throw | Check return value; if 0, check `GetLastError` for `ERROR_ACCESS_DENIED`; surface diagnostic to user |
| `GetAsyncKeyState` + combo behaviors | Test only `& 0x8000` (held) | Also test `& 0x0001` (latched since last call) for tap-detection behaviors |
| INI → JSON migration | Keep INI for "simple" data, add JSON alongside for new behavior data | Consolidate everything into JSON from the start; dual formats create load/save divergence bugs |
| `g_configMutex` + multi-field behavior descriptor | Lock per-field update (one `lock_guard` per field) | Lock over the entire descriptor write as one atomic operation |
| Win32 owner-drawn controls + dark mode | Rely on system theme colors for control backgrounds | All control backgrounds must be explicitly painted with the profile's COLORREF theme; system colors ignore dark mode for owner-drawn content |
| `WM_ERASEBKGND` + custom controls | Leave default handler, expect system to erase | Return non-zero from `WM_ERASEBKGND` without erasing; paint full background in `WM_PAINT` to eliminate flicker |
| Profile editor combobox for behavior type | Use standard `COMBOBOX` with `CBS_DROPDOWNLIST` | Works, but requires subclassing the edit control to intercept keyboard input; owner-drawn variant required for dark mode background |

---

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| `CreateToolhelp32Snapshot` on the logic thread hot path every 10ms | 35-40ms jank spikes per snapshot call; game stutter | Poll process existence at 1s intervals (current design); do not reduce polling interval below 500ms | Immediately at any polling rate faster than ~200ms |
| Full profile JSON file re-parse on every settings window open | Noticeable delay opening the editor for large profile files | Parse JSON once at startup and on explicit "reload"; keep parsed representation in memory | When profile files accumulate many entries |
| `g_configMutex` contention in the logic thread's 10ms hot loop | Logic thread stalls waiting for UI thread; input injection timing jitter | Minimize time held: snapshot binding values at loop top, release mutex, then execute behavior logic below | If UI thread holds mutex for long operations (e.g., file I/O during save) — never save under the mutex |
| Invalidating the entire settings window on every font change | Perceptible flash during font cycling | Already an acceptable infrequent operation; acceptable as-is for the profile editor too | Not a problem at current scale |

---

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| No input validation on JSON profile values (vk codes, threshold ms, behavior type enum) | Malformed profile file injects arbitrary virtual key codes or causes behavior engine to enter unexpected states | Validate all loaded values: vk in `[0x01, 0xFF]`, threshold in `[0, 10000]ms`, behavior type within the known enum set; reject and warn on out-of-range |
| Loading profile JSON from an arbitrary path (e.g., relative path traversal) | A malicious profile file placed in a predictable location triggers unintended input injection | Restrict profile loading to the directory adjacent to the `.exe`; reject paths with `..` components |
| Process name matching activating on spoofed process name | A process named `eldenring.exe` unexpectedly activates remapping | Existing `_wcsicmp` is correct; no additional risk from the data-driven upgrade; document as known low-risk limitation |
| Synthetic input flagged by anti-cheat as macro behavior | User account ban in EAC/BattlEye games | Document risk in UI; do not add timing jitter or injection-flag-clearing attempts (out of scope, ineffective with Win32 API only) |

---

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Profile editor saves on every keystroke during key-bind capture | Excessive file I/O; if file write fails mid-session, partial profile saved | Save to file only when user explicitly confirms or closes the editor; keep in-memory state as the live source of truth during editing |
| No visual feedback when `SendInput` is silently blocked by UIPI | User thinks remapping is active; nothing happens in game; user blames the tool | Add a tray tooltip or notification: "Input injection may be blocked — game may be elevated" when `SendInput` returns 0 |
| Behavior type dropdown changes taking effect immediately on the live logic thread | Input behavior changes unexpectedly mid-game | Apply behavior type changes only when user clicks a "Save" or "Apply" button, not on dropdown selection change |
| No duplicate key binding check in the profile editor | Two bindings fire on the same key; undefined behavior in behavior engine | Validate on save: if two bindings share the same `currentVk`, reject with an inline error message |
| "Add Game" UI accepts any process name without validation | Profile never activates; user can't diagnose why | Validate process name is non-empty and ends in `.exe`; show "process name will be matched case-insensitively" hint |
| Profile editor window allows resizing but custom-drawn controls use hardcoded pixel positions | Controls overlap or disappear on resize | Either lock window size (simpler, consistent with existing settings window approach) or use proportional layout from the start |

---

## "Looks Done But Isn't" Checklist

- [ ] **Behavior engine — key-stuck cleanup:** Verify injected-key release fires on ALL exit paths: normal shutdown, profile switch, game-exit detection, and `std::terminate`. Test by force-killing the process via Task Manager while a hold behavior is active.
- [ ] **SendInput return value:** Verify every `SendInput` call site checks the return value and logs or surfaces failures. grep for `SendInput` and confirm none are fire-and-forget.
- [ ] **Mutex atomicity for descriptor writes:** Verify the UI never writes individual behavior parameter fields under separate mutex acquisitions. A code review of all `lock_guard` scopes in the profile editor is the check.
- [ ] **JSON migration completeness:** Verify that settings previously in `settings.ini` (`currentVk` values per binding) are migrated to JSON and that the INI reader is either removed or intentionally left as a fallback import path — not silently used alongside JSON in ways that can conflict.
- [ ] **Behavior state isolation:** Verify that runtime state (toggle flags, timer state, combo progress) is in per-thread objects, not in the `GameProfile` or behavior descriptor structs. Check that switching profiles resets all state to initial values.
- [ ] **Anti-cheat disclaimer:** Verify the "Add Game" UI and/or help text includes a visible, specific warning about anti-cheat risk for EAC/BattlEye games — not buried in documentation.
- [ ] **GetAsyncKeyState latch bit:** Verify that tap-detection behaviors (combo, single-press-to-toggle) use `& 0x8001`, not just `& 0x8000`. A code review of all `GetAsyncKeyState` call sites is the check.
- [ ] **Profile editor — no live apply on behavior type change:** Verify the logic thread does not immediately adopt a partially-configured behavior when the user is still mid-edit in the dropdown.

---

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| UIPI injection failure not detected | MEDIUM | Add `SendInput` return-value check and diagnostic emission; requires touching every `SendInput` call site in the engine |
| Key stuck in OS input queue from crash/force-exit | LOW | User presses the stuck key physically to release it; no data loss; add RAII cleanup to prevent recurrence |
| Partial descriptor write race condition manifested as wrong injection | HIGH | Requires reproducing the race (hard), then refactoring descriptor writes to be atomic; difficult to fix after the engine is built with per-field locking |
| Behavior state leaked between profile switches | MEDIUM | Audit all stateful fields in behavior implementations; move to per-thread state objects; regression-testable |
| INI + JSON dual-format divergence | HIGH | If both formats are used in parallel, reconciling which is authoritative requires a migration tool and careful transition plan; avoid by consolidating to JSON from the start |
| Anti-cheat ban from injected input | N/A (not a code bug) | Document as known limitation; no code recovery; communicate risk to user proactively |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| UIPI injection failure (Pitfall 1) | Behavior engine (add return-value checks to `SendInput` wrapper) | grep confirms all `SendInput` calls check return; test with UAC-elevated target process |
| Key stuck on exit (Pitfall 2) | Behavior engine (RAII injected-key tracking in engine core) | Force-kill test while hold behavior active; no stuck keys after |
| Partial descriptor read race (Pitfall 3) | Behavior engine (define `BehaviorDescriptor` as atomic-write unit) + Profile editor (enforce single-lock writes) | Code review of all mutex acquisition scopes; concurrent edit stress test |
| `GetAsyncKeyState` missed taps (Pitfall 4) | Behavior engine (per-behavior-type polling strategy) | Rapid-tap test on combo/toggle behaviors; verify zero misses at normal typing speed |
| Anti-cheat detection (Pitfall 5) | Profile editor (user-facing disclaimer in "Add Game" UI) | UI review confirms disclaimer present and specific |
| Stale behavior state on profile switch (Pitfall 6) | Behavior engine (descriptor vs. runtime state separation) | Profile switch integration test while behavior active; verify no spurious injection on new profile start |
| JSON migration correctness | Profile storage phase (replace INI with JSON before editor is built) | Load old `settings.ini` binding values into new JSON format; verify round-trip fidelity |
| Duplicate key binding | Profile editor (validation on save) | Assign same key to two bindings; verify save is rejected with error message |

---

## Sources

- [SendInput function (winuser.h) — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput) — UIPI blocking, return value semantics (HIGH confidence)
- [UIPI and SendInput — MSDN Forums](https://social.msdn.microsoft.com/Forums/en-US/b68a77e7-cd00-48d0-90a6-d6a4a46a95aa/sendinput-fail-beause-interface-privilege-isolation-uipi-and-integrity) — ERROR_ACCESS_DENIED behavior (HIGH confidence)
- [BattlEye — The Anti-Cheat Gold Standard](https://www.battleye.com/) — macro/input behavior detection (MEDIUM confidence — policy details not published)
- [Low Level Methods of Sending Mouse Input that bypass anticheat — Guided Hacking](https://guidedhacking.com/threads/low-level-methods-of-sending-mouse-input-that-bypass-anticheat.14555/) — injected flag detection (MEDIUM confidence)
- [GetAsyncKeyState polling — GameDev.net](https://www.gamedev.net/forums/topic/677706-input-polling-low-latency-api/5285551/) — 10ms polling and missed inputs (HIGH confidence)
- [Stuck Hotkey or Remap — AutoHotkey Community](https://www.autohotkey.com/board/topic/78703-stuck-hotkey-or-remap-happens-in-game-continues-on-the-so/) — key stuck on exit behavior (HIGH confidence — widely reproduced)
- [Flicker-free Drawing — catch22.net](https://www.catch22.net/tuts/win32/flicker-free-drawing/) — WM_ERASEBKGND suppression and double buffering (HIGH confidence)
- [CreateToolhelp32Snapshot — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-createtoolhelp32snapshot) — snapshot timing and jank (HIGH confidence)
- [INI file format limitations — Old New Thing (Microsoft DevBlog)](https://devblogs.microsoft.com/oldnewthing/20071126-00/?p=24383) — 32KB cap, no nesting, Unicode issues (HIGH confidence)
- [Thread safety of nlohmann/json parse — GitHub Issue #1712](https://github.com/nlohmann/json/issues/1712) — JSON parse thread safety (HIGH confidence)
- Existing codebase CONCERNS.md — anti-cheat flag, mutex contention, key-stuck bug (EldenRing.h), INI validation gaps (PRIMARY source — codebase evidence)

---

*Pitfalls research for: Win32 input remapping — data-driven profile system with behavior engine*
*Researched: 2026-03-11*
