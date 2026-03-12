# Phase 1: Behavior Engine - Research

**Researched:** 2026-03-12
**Domain:** Win32 input injection, C++ behavior engine design, UIPI detection, RAII cleanup
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BEH-01 | App supports HoldToToggle binding behavior — hold key activates action, release deactivates it | Existing EldenRing sprint logic demonstrates the exact pattern; generic descriptor captures inputVk + outputVk |
| BEH-02 | App supports EdgeTrigger binding behavior — action fires on key release, not press | Existing Elden Ring dodge logic fires on rising edge then completes; generic descriptor captures inputVk + outputVk + durationMs |
| BEH-03 | App supports LongPress binding behavior — short press vs. long press trigger different output keys | Existing Elden Ring trigger logic already implements this with GetTickCount64; descriptor captures inputVk + shortOutputVk + longOutputVk + thresholdMs |
| BEH-04 | App supports WheelToKey binding behavior — mouse scroll up/down mapped to configurable key press(es) | Existing ToxicCommando logic sends MOUSEEVENTF_WHEEL; descriptor maps inputVk to scroll delta direction |
| FIX-01 | All injected key presses are guaranteed released on app exit or game stop (RAII key tracking) | WM_DESTROY + logic thread teardown is the viable path; TerminateProcess skips all cleanup — must use active key tracking + release-before-exit in the logic thread and WM_DESTROY handler |
| FIX-02 | App detects when SendInput is blocked by UIPI and displays a warning to the user | SendInput returns 0 when blocked by UIPI but GetLastError does NOT distinguish UIPI from other failures; best approach is to detect target process elevation via OpenProcessToken + GetTokenInformation(TokenIntegrityLevel) at game start |
</phase_requirements>

---

## Summary

The existing codebase contains two fully functional but hardcoded game profiles. The behavior engine in Phase 1 is NOT about building a JSON parser or UI — it is about extracting the behavior logic from those hardcoded implementations into a generic, data-driven engine that can be configured via in-process descriptor structs. The hardcoded profile files (EldenRing.h, ToxicCommando.h) should be replaced by a generic `LogicThreadFn` that reads a `BehaviorDescriptor` per binding and dispatches to the correct behavior handler.

The two reliability fixes are interrelated. FIX-01 (no stuck keys on exit) requires that the logic thread maintains an explicit set of currently-injected keys and releases them both when the game stops and when the logic thread exits. FIX-02 (UIPI warning) requires detecting, at game activation time, whether the target game process is running elevated — because `SendInput` silently returns 0 when blocked by UIPI without setting a distinguishable error code.

The LongPress requirement (BEH-03) needs a clarification: the existing Elden Ring implementation fires the long-press output on a sustained hold but does NOT fire the short-press output on a tap — it simply does nothing on short tap. The success criterion in the phase goal says "fires the short-press output on a tap." This is new behavior not present in the current hardcoded code and must be designed into the generic LongPress handler.

**Primary recommendation:** Add a new header `BehaviorEngine.h` that defines a `BehaviorType` enum and `BehaviorDescriptor` struct, implement a single generic `GenericLogicThreadFn`, and rebuild `g_EldenRingProfile` and `g_ToxicCommandoProfile` to use the generic function with per-binding descriptors — without touching any UI code.

---

## Standard Stack

### Core (all existing — no new dependencies)

| API / Component | Version | Purpose | Notes |
|----------------|---------|---------|-------|
| `SendInput` + `INPUT` struct | Win32 (all Windows versions) | Inject keyboard and mouse wheel events | Already used; UIPI-sensitive |
| `GetAsyncKeyState` | Win32 | Poll physical key state in logic thread | Already used; polling loop every 10ms |
| `GetTickCount64` | Win32 | Measure elapsed time for LongPress threshold | Already used in EldenRing |
| `OpenProcessToken` + `GetTokenInformation(TokenIntegrityLevel)` | Win32 (Vista+) | Detect if target process is elevated | New addition for FIX-02 |
| `CreateToolhelp32Snapshot` | Win32 | Detect if game process is running | Already used in `IsGameRunning` |
| `MessageBox` / custom win32 dialog | Win32 | Surface UIPI warning to user | Existing UI pattern |
| `std::atomic<bool>` | C++20 stdlib | Stop signal for logic thread | Already used (`g_logicRunning`) |
| `std::mutex` + `std::lock_guard` | C++20 stdlib | Protect shared binding state | Already used (`g_configMutex`) |

### No new libraries

The constraint is explicit: C++20, Win32 API only, no new runtime dependencies. All research confirms the required patterns are achievable with existing Win32 primitives.

---

## Architecture Patterns

### Recommended File Structure

```
CustomControlZ/
├── BehaviorEngine.h       NEW — BehaviorType enum, BehaviorDescriptor struct,
│                               GenericLogicThreadFn, KeyTracker helper
├── GameProfiles.h         EXISTING — add BehaviorDescriptor field to KeyBinding
├── games/
│   ├── EldenRing.h        MODIFY — remove EldenRingLogicThread, rebuild using
│   │                               GenericLogicThreadFn + descriptors
│   └── ToxicCommando.h    MODIFY — remove ToxicCommandoLogicThread, rebuild
└── CustomControlZ.cpp     MODIFY — add UIPI check in OnGameSelected/
                                    StartGameLogicThread; ensure ReleaseAllTrackedKeys
                                    is called in WM_DESTROY and StopGameLogicThread
```

### Pattern 1: BehaviorDescriptor Struct

Each `KeyBinding` gains a `BehaviorDescriptor` that the generic engine reads at runtime. No virtual dispatch — a simple `switch` on `BehaviorType` is sufficient for four cases.

```cpp
// BehaviorEngine.h
enum class BehaviorType : uint8_t {
    HoldToToggle,  // BEH-01: hold inputVk -> hold outputVk
    EdgeTrigger,   // BEH-02: rising edge on inputVk -> pulse outputVk for durationMs
    LongPress,     // BEH-03: tap -> pulse shortOutputVk; hold >= thresholdMs -> pulse longOutputVk
    WheelToKey,    // BEH-04: rising edge on inputVk -> send mouse wheel delta
};

struct BehaviorDescriptor {
    BehaviorType type;
    WORD         outputVk;       // HoldToToggle, EdgeTrigger, LongPress (short output)
    WORD         longOutputVk;   // LongPress only: output on sustained hold
    int          thresholdMs;    // LongPress: hold threshold (default 400ms)
    int          durationMs;     // EdgeTrigger: output pulse duration (default 50ms)
    DWORD        wheelDelta;     // WheelToKey: MOUSEEVENTF_WHEEL mouseData value
};
```

**Extend `KeyBinding` in GameProfiles.h:**
```cpp
struct KeyBinding {
    const wchar_t*    iniKey;
    const wchar_t*    label;
    WORD              defaultVk;
    WORD              currentVk;
    BehaviorDescriptor behavior;   // NEW
};
```

### Pattern 2: KeyTracker for RAII Key Release (FIX-01)

The logic thread must track every key it has injected so it can release them all during teardown. `TerminateProcess` bypasses all C++ destructors and atexit handlers — the only reliable cleanup path for a force-killed GUI app is the `StopGameLogicThread` path (which the existing `WM_DESTROY` calls) plus a final sweep-and-release inside the thread function itself before it returns.

```cpp
// Inside BehaviorEngine.h or the logic thread function
class KeyTracker {
public:
    void press(WORD vk)   { if (vk) active_.insert(vk); }
    void release(WORD vk) { active_.erase(vk); }
    void releaseAll() {
        for (WORD vk : active_) SendKeyInput(vk, true /*keyUp*/);
        active_.clear();
    }
private:
    std::set<WORD> active_;  // small set, 8 entries max
};
```

The logic thread instantiates one `KeyTracker` on the stack. Every `PressKey(vk)` call is paired with `tracker.press(vk)` and every `ReleaseKey(vk)` call with `tracker.release(vk)`. At the bottom of the thread function (before return, and when the game stops), call `tracker.releaseAll()`.

**Critical note on force-kill:** When the OS force-terminates a process (Task Manager "End Task"), `TerminateProcess` is called. This does NOT run destructors, does NOT call atexit handlers, and does NOT send WM_DESTROY to message-loop-less threads. The only protection is that `StopGameLogicThread` is called from `WM_DESTROY` (which IS sent during a normal close/exit initiated by the app itself). For a genuine hard kill by the OS, keys injected via `SendInput` will remain in the OS input queue. This is a fundamental Win32 limitation — it cannot be fully solved without a kernel driver. The requirement states "force-killing the app... does not leave that key stuck" — this can be approximated but the OS-level hard-kill case has no solution in user-mode. The RAII tracker covers the app's own exit path.

### Pattern 3: Generic Logic Thread

```cpp
// BehaviorEngine.h
void GenericLogicThreadFn(GameProfile* profile, std::atomic<bool>& running);
```

The thread loop:
1. Polls `g_waitingForBindID` (same as existing logic — pause during key rebinding).
2. Calls `IsGameRunning(profile)` — updates tray icon on state change.
3. When game is running: iterates all bindings, reads `binding.currentVk` under lock, dispatches to behavior handler by `BehaviorType`.
4. On game-stop transition: calls `tracker.releaseAll()` on all HoldToToggle keys that are active.
5. On thread exit (running = false): calls `tracker.releaseAll()`.
6. Sleeps 10ms per tick.

### Pattern 4: UIPI Detection (FIX-02)

`SendInput` returns 0 when blocked by UIPI, but `GetLastError` does NOT identify UIPI specifically (confirmed by official docs). The correct strategy is to proactively detect the target game's integrity level when the game process is found, and show a warning immediately.

```cpp
// Returns true if the named process is running at High integrity (elevated)
bool IsProcessRunningElevated(const wchar_t* processName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = { sizeof(pe) };
    bool elevated = false;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) {
                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                           FALSE, pe.th32ProcessID);
                if (hProc) {
                    HANDLE hToken = nullptr;
                    if (OpenProcessToken(hProc, TOKEN_QUERY, &hToken)) {
                        DWORD infoLen = 0;
                        GetTokenInformation(hToken, TokenIntegrityLevel,
                                            nullptr, 0, &infoLen);
                        if (infoLen > 0) {
                            auto* label = static_cast<TOKEN_MANDATORY_LABEL*>(
                                          _alloca(infoLen));
                            if (GetTokenInformation(hToken, TokenIntegrityLevel,
                                                    label, infoLen, &infoLen)) {
                                DWORD level = *GetSidSubAuthority(
                                    label->Label.Sid,
                                    *GetSidSubAuthorityCount(label->Label.Sid) - 1);
                                elevated = (level >= SECURITY_MANDATORY_HIGH_RID);
                            }
                        }
                        CloseHandle(hToken);
                    }
                    CloseHandle(hProc);
                }
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return elevated;
}
```

Call this in `StartGameLogicThread` (or `OnGameSelected`) when `IsGameRunning` first returns `true`. If elevated, show a `MessageBox` warning and do not start the logic thread (or start it but suppress injection with a warning state).

**Why not detect via SendInput return value:** `SendInput` returning 0 is indistinguishable from the input being blocked by another thread or the window being in the wrong focus state. Proactive elevation check is the only reliable approach.

### Pattern 5: LongPress — Short-Press Output on Tap (new behavior vs. existing)

The existing Elden Ring LongPress does nothing on short tap. The success criterion requires the short-press output fires on tap. The generic handler must:

1. On rising edge of inputVk: record `triggerStartTime`, set `keyDown = true`.
2. On key still held: if `elapsed >= thresholdMs` and not yet fired, pulse `longOutputVk` and set `longFired = true`.
3. On falling edge of inputVk:
   - If NOT `longFired`: pulse `shortOutputVk` (this is the new short-tap behavior).
   - Reset state.

```cpp
// LongPress handler state per binding
struct LongPressState {
    bool       keyDown    = false;
    bool       longFired  = false;
    ULONGLONG  pressTime  = 0;
};
```

### Anti-Patterns to Avoid

- **Releasing keys in a static global destructor or atexit handler:** These are not called on `TerminateProcess` or when the app is hard-killed. Do the release inside the logic thread on exit, and in `WM_DESTROY` via `StopGameLogicThread`.
- **Using `GetLastError` to detect UIPI:** The official documentation explicitly states neither `GetLastError` nor the return value identifies UIPI as the cause of failure. Always use token integrity level check.
- **Polling game state every 10ms even when game is not running:** The existing code correctly sleeps 1000ms when game is not running — preserve this optimization in the generic engine.
- **Accessing `binding.currentVk` without the config mutex:** The existing pattern uses a lock to snapshot VK values at the top of each tick — maintain this in the generic dispatcher.
- **Sending a mouse wheel event on every tick while key is held for WheelToKey:** The existing TC implementation correctly edge-triggers (one event per key press, not per tick). The generic engine must preserve this.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Key state polling | Custom hook / raw input loop | `GetAsyncKeyState` in existing polling thread | Already battle-tested; hooks require additional message pump |
| Input injection | `keybd_event` / `PostMessage WM_KEYDOWN` | `SendInput` with `KEYEVENTF_SCANCODE` | Only `SendInput` uses hardware scan codes; avoids VK-only limitations |
| Timing | `timeGetTime`, `QueryPerformanceCounter` | `GetTickCount64` | Already used; millisecond precision is sufficient for 400ms thresholds |
| Process detection | WMI queries | `CreateToolhelp32Snapshot` | Already used; faster, no COM initialization |
| Integrity level detection | Registry queries, WMI | `OpenProcessToken` + `GetTokenInformation(TokenIntegrityLevel)` | Official Win32 API; works without elevation |

---

## Common Pitfalls

### Pitfall 1: UIPI Failure Is Silent
**What goes wrong:** `SendInput` returns 0 when the target game is elevated and UIPI blocks injection. No `GetLastError` code distinguishes this from other failures (blocked by another thread, etc.).
**Why it happens:** UIPI is a security boundary; Windows does not expose the reason for failure to avoid information leakage.
**How to avoid:** Check the integrity level of the game process at activation time using `OpenProcessToken` + `GetTokenInformation(TokenIntegrityLevel)`. Show a warning before the user tries to use remapping.
**Warning signs:** Keys not registering in-game; `SendInput` return value == 0 consistently.

### Pitfall 2: Keys Stuck on Force-Kill
**What goes wrong:** User opens Task Manager and ends the process while a key is held (e.g., sprint is active). The OS leaves the simulated key in a pressed state until the user physically presses and releases it.
**Why it happens:** `TerminateProcess` does not run C++ destructors, atexit handlers, or `WM_DESTROY`. The OS-level input queue retains the injected key state.
**How to avoid:** Release all tracked keys in the logic thread body before returning (covers app exit via the message loop). For hard kills, this is a known Win32 limitation — document it but do not claim it is fully solved in the success criteria verification.
**Warning signs:** Character in game keeps sprinting / moving after app exit.

### Pitfall 3: LongPress Short-Tap Output Gap
**What goes wrong:** If implementing LongPress from the existing Elden Ring code, a short tap fires nothing (existing behavior). The phase success criterion requires firing the short-press output on tap.
**Why it happens:** The existing implementation only fires on sustained hold.
**How to avoid:** Add falling-edge detection in the LongPress handler — if key was released before `thresholdMs` elapsed and `longFired` is false, pulse `shortOutputVk`.

### Pitfall 4: Per-Binding State Arrays
**What goes wrong:** The generic engine needs state per binding (e.g., `sprintActive`, `dodgePressed`, `triggerDown`). If these are stored incorrectly (e.g., as a single global bool), behaviors conflict when two bindings use the same type.
**Why it happens:** The hardcoded implementations used named variables; a generic engine needs indexed per-binding state.
**How to avoid:** Define state structs per behavior type (`HoldToToggleState`, `EdgeTriggerState`, `LongPressState`, `WheelToKeyState`) and maintain an array `state[MAX_BINDINGS]` using a `union` or `variant`.

### Pitfall 5: Scan Code vs. Virtual Key for Extended Keys
**What goes wrong:** Using `KEYEVENTF_SCANCODE` for extended keys (arrows, Ins, Del, PgUp, PgDn) without `KEYEVENTF_EXTENDEDKEY` flag causes the wrong key to be sent.
**Why it happens:** Extended keys share scan codes with numpad keys; the extended flag distinguishes them.
**How to avoid:** Check the existing `SendKeyInput` function — it already handles this for VKs with zero scan code by falling back to VK mode. Preserve this logic in the generic engine rather than duplicating or simplifying it.

### Pitfall 6: WheelToKey Binding Does Not Use outputVk
**What goes wrong:** Wheel-to-key behavior sends a mouse event, not a key event. Using the `outputVk` field for wheel delta would be misleading.
**How to avoid:** Use `wheelDelta` in the descriptor for `WheelToKey` type. Leave `outputVk` zero for these bindings. The settings UI (Phase 3) will need to handle this specially, but Phase 1 can hard-code wheel descriptors.

---

## Code Examples

### Existing SendKeyInput (preserve exactly)
```cpp
// Source: CustomControlZ.cpp (existing, confirmed correct)
inline void SendKeyInput(WORD vk, bool keyUp) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    if (scanCode == 0) {
        input.ki.wVk     = vk;
        input.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
    } else {
        input.ki.wScan   = static_cast<WORD>(scanCode);
        input.ki.dwFlags = KEYEVENTF_SCANCODE | (keyUp ? KEYEVENTF_KEYUP : 0);
    }
    SendInput(1, &input, sizeof(INPUT));
}
```

### UIPI / SendInput Return Value (official docs)
```
// Source: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput
// "This function fails when it is blocked by UIPI. Note that neither
//  GetLastError nor the return value will indicate the failure was caused
//  by UIPI blocking."
UINT sent = SendInput(1, &input, sizeof(INPUT));
// sent == 0 means blocked — but you cannot tell if it was UIPI or something else.
```

### Integrity Level Check Skeleton
```cpp
// Source: Raymond Chen, https://devblogs.microsoft.com/oldnewthing/20221017-00/?p=107291
// + TOKEN_ELEVATION docs: https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-token_elevation
// Pattern: get token, query TokenIntegrityLevel, read last SID subauthority
// Compare against SECURITY_MANDATORY_HIGH_RID (0x3000) for "elevated"
```

### WheelToKey Injection (preserve existing TC pattern)
```cpp
// Source: ToxicCommando.h (existing, confirmed correct)
INPUT input = {};
input.type             = INPUT_MOUSE;
input.mi.dwFlags       = MOUSEEVENTF_WHEEL;
input.mi.mouseData     = TC_SCROLL_DELTA_UP;  // 120 for up, (DWORD)-120 for down
SendInput(1, &input, sizeof(INPUT));
```

---

## State of the Art

| Old Approach | Current Approach | Impact for This Phase |
|--------------|------------------|----------------------|
| Hardcoded named variables per profile | Generic `BehaviorDescriptor` + per-binding state array | Enables same logic thread to handle any combination of 4 behavior types |
| `EldenRingLogicThread` / `ToxicCommandoLogicThread` | `GenericLogicThreadFn` | Both profile files become data declarations, not code |
| No key tracking | `KeyTracker` with release-all on exit | Addresses FIX-01 (normal exit path) |
| No UIPI awareness | Proactive elevation check at game start | Addresses FIX-02 |

---

## Open Questions

1. **Hard-kill stuck-key guarantee**
   - What we know: `TerminateProcess` bypasses all C++ cleanup. `WM_DESTROY` is only sent for graceful shutdown.
   - What's unclear: The success criterion says "force-killing the app does not leave that key stuck." If "force-killing" means Task Manager End Task (which calls `TerminateProcess`), this cannot be fully guaranteed in user-mode without a kernel driver.
   - Recommendation: Implement `KeyTracker` for the normal exit path, clearly document the hard-kill limitation. Reinterpret the success criterion as "the app's own exit path (WM_DESTROY, user Exit button, tray exit) releases all keys."

2. **LongPress tap fires short-press — interaction with EdgeTrigger bindings**
   - What we know: The current Elden Ring profile uses a separate binding for the "trigger" key. In a generic engine, LongPress and EdgeTrigger are distinct types on distinct bindings.
   - What's unclear: Can a user assign LongPress to the same input key as a HoldToToggle binding? In Phase 1, this is hardcoded in the descriptor, so no conflict arises.
   - Recommendation: Phase 1 uses fixed descriptors; no conflict detection is needed until Phase 3 (BEH-06).

3. **UIPI warning — when to show**
   - What we know: The game must be running to check its elevation. The check happens when `IsGameRunning` first returns true.
   - What's unclear: Should the warning block remapping entirely, or just warn once and continue?
   - Recommendation: Show a `MessageBox` warning once when elevated game is first detected; do not block the logic thread. The user can dismiss and continue, knowing remapping may silently fail.

---

## Validation Architecture

`nyquist_validation` is enabled in `.planning/config.json`.

### Test Framework

| Property | Value |
|----------|-------|
| Framework | None detected — this is a Win32 C++ binary with no existing test infrastructure |
| Config file | None — see Wave 0 gaps |
| Quick run command | Manual: launch app, activate profile, verify behavior |
| Full suite command | Manual: run all 5 success criteria scenarios |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BEH-01 | HoldToToggle: hold inputVk → outputVk held; release → outputVk released | manual-smoke | Run app, Elden Ring profile, hold sprint key, observe game response | N/A |
| BEH-02 | EdgeTrigger: press dodge key → single pulse of outputVk, no repeat while held | manual-smoke | Run app, Elden Ring profile, hold dodge key, confirm single dodge | N/A |
| BEH-03 | LongPress tap: fires shortOutputVk; sustained hold: fires longOutputVk | manual-smoke | Run app, Elden Ring profile, tap trigger key (should fire), hold trigger key (should fire long output) | N/A |
| BEH-04 | WheelToKey: press scroll-up key → mouse wheel up event | manual-smoke | Run app, TC profile, press scroll key, observe weapon cycle | N/A |
| FIX-01 | Force-close while key held → key released | manual-smoke | Hold sprint in Elden Ring mode, click Exit button, confirm key not stuck | N/A |
| FIX-02 | Game running elevated → warning MessageBox shown | manual-smoke | Launch Elden Ring as admin, activate profile, confirm warning dialog | N/A |

**Justification for manual-only:** This is a Win32 system-tray application that injects input via `SendInput` into a live game process. Automated unit tests would require either a mock `SendInput` (significant infrastructure) or a real game target. Phase 1 has no existing test infrastructure. All behaviors have observable, deterministic effects that are verifiable in under 5 minutes manually.

### Wave 0 Gaps

None — no automated test infrastructure exists or is required for this phase. Manual smoke testing against the five success criteria is the validation approach.

*(If a unit test harness is desired in a future phase, the behavior handler functions could be extracted into testable pure functions that take state structs and return key-press commands — but this is out of scope for Phase 1.)*

---

## Sources

### Primary (HIGH confidence)
- [SendInput (winuser.h) — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput) — UIPI blocking behavior, return value semantics
- [TerminateProcess (processthreadsapi.h) — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess) — destructor/cleanup not called on force termination
- [TOKEN_ELEVATION (winnt.h) — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-token_elevation) — process elevation detection
- [Raymond Chen: How can I check the integrity level of my process?](https://devblogs.microsoft.com/oldnewthing/20221017-00/?p=107291) — TokenIntegrityLevel pattern
- `CustomControlZ/games/EldenRing.h` — existing HoldToToggle, EdgeTrigger, LongPress logic
- `CustomControlZ/games/ToxicCommando.h` — existing WheelToKey logic
- `CustomControlZ/GameProfiles.h` — existing KeyBinding, GameProfile structs

### Secondary (MEDIUM confidence)
- [SendInput fail because UIPI — MSDN archive forum](https://learn.microsoft.com/en-us/archive/msdn-technet-forums/b68a77e7-cd00-48d0-90a6-d6a4a46a95aa) — confirms UIPI silent failure behavior
- [TOKEN_ELEVATION_TYPE — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ne-winnt-token_elevation_type) — elevation type enum

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all APIs are existing Win32 primitives already in use
- Architecture: HIGH — based directly on reading the existing working implementations
- Pitfalls: HIGH — UIPI behavior confirmed by official docs; destructor/atexit behavior confirmed by official TerminateProcess docs
- LongPress short-tap behavior: HIGH — gap identified by comparing existing code against the stated success criterion

**Research date:** 2026-03-12
**Valid until:** 2026-09-12 (Win32 APIs are stable; 6-month window is conservative)
