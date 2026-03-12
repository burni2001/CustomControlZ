#pragma once
#include <windows.h>
#include <atomic>
#include <mutex>
#include <set>

// Include order note: GameProfiles.h includes BehaviorEngine.h (one-way).
// BehaviorEngine.h does NOT include GameProfiles.h to avoid circular dependency.
// MAX_BINDINGS must be defined before this header in any translation unit (provided by GameProfiles.h).

// --- FORWARD DECLARATIONS ---
// Helpers defined in CustomControlZ.cpp; available in any TU that includes GameProfiles.h first.

struct GameProfile;
inline void PressKey(WORD vk);
inline void ReleaseKey(WORD vk);
inline bool IsKeyDown(WORD vk);
bool IsGameRunning(GameProfile* profile);
void SetTrayIconState(bool active, GameProfile* profile);
extern std::mutex g_configMutex;
extern std::atomic<int> g_waitingForBindID;

// --- BEHAVIOR TYPES ---

enum class BehaviorType : uint8_t {
    HoldToToggle,  // hold inputVk -> hold outputVk continuously
    EdgeTrigger,   // rising edge on inputVk -> pulse outputVk for durationMs
    LongPress,     // tap -> pulse shortOutputVk; hold >= thresholdMs -> pulse longOutputVk
    WheelToKey,    // rising edge on inputVk -> send mouse wheel delta (wheelDelta)
};

struct BehaviorDescriptor {
    BehaviorType type         = BehaviorType::HoldToToggle;
    WORD         outputVk     = 0;      // HoldToToggle, EdgeTrigger, LongPress short output
    WORD         longOutputVk = 0;      // LongPress only: output on sustained hold
    int          thresholdMs  = 400;    // LongPress: hold threshold in milliseconds
    int          durationMs   = 50;     // EdgeTrigger: output pulse duration in milliseconds
    DWORD        wheelDelta   = 120;    // WheelToKey: MOUSEEVENTF_WHEEL mouseData (120 = up, (DWORD)-120 = down)
};

// --- PER-BINDING STATE STRUCTS ---

struct HoldToToggleState {
    bool active = false;
};

struct EdgeTriggerState {
    bool fired = false;
};

struct LongPressState {
    bool      keyDown   = false;
    bool      longFired = false;
    ULONGLONG pressTime = 0;
};

struct WheelToKeyState {
    bool pressed = false;
};

union BindingState {
    HoldToToggleState holdToggle;
    EdgeTriggerState  edgeTrigger;
    LongPressState    longPress;
    WheelToKeyState   wheelToKey;
    BindingState() { memset(this, 0, sizeof(*this)); }
};

// --- KEY TRACKER ---
// Tracks every injected key so all can be released on teardown (satisfies FIX-01).

class KeyTracker {
public:
    void press(WORD vk)   { if (vk) active_.insert(vk); }
    void release(WORD vk) { active_.erase(vk); }
    void releaseAll() {
        for (WORD vk : active_) ReleaseKey(vk);
        active_.clear();
    }
private:
    std::set<WORD> active_;
};

// --- GENERIC LOGIC THREAD ---
// Implements all four behavior types in one dispatcher.
// Drop-in replacement for LogicThreadFn (GameProfiles.h).

void GenericLogicThreadFn(GameProfile* profile, std::atomic<bool>& running) {
    BindingState state[MAX_BINDINGS];    // zero-initialised via BindingState()
    KeyTracker   tracker;
    bool         lastGameState = false;

    while (running) {
        // Pause while the UI is capturing a new key binding
        if (g_waitingForBindID != 0) {
            Sleep(50);
            continue;
        }

        bool currentGameState = IsGameRunning(profile);

        if (currentGameState != lastGameState) {
            if (lastGameState && !currentGameState) {
                // Game just stopped: release all held outputs
                tracker.releaseAll();
                SetTrayIconState(false, profile);
            } else {
                // Game just started
                SetTrayIconState(true, profile);
            }
            lastGameState = currentGameState;
        }

        if (!currentGameState) {
            Sleep(1000);
            continue;
        }

        // Snapshot VKs under mutex to avoid data races with UI thread
        WORD localVk[MAX_BINDINGS] = {};
        {
            std::lock_guard<std::mutex> lock(g_configMutex);
            for (int i = 0; i < profile->bindingCount; i++) {
                localVk[i] = profile->bindings[i].currentVk;
            }
        }

        // Per-binding dispatch
        for (int i = 0; i < profile->bindingCount; i++) {
            const BehaviorDescriptor& desc = profile->bindings[i].behavior;
            bool keyDown = IsKeyDown(localVk[i]);

            switch (desc.type) {

            case BehaviorType::HoldToToggle: {
                HoldToToggleState& s = state[i].holdToggle;
                if (keyDown && !s.active) {
                    PressKey(desc.outputVk);
                    tracker.press(desc.outputVk);
                    s.active = true;
                } else if (!keyDown && s.active) {
                    ReleaseKey(desc.outputVk);
                    tracker.release(desc.outputVk);
                    s.active = false;
                }
                break;
            }

            case BehaviorType::EdgeTrigger: {
                EdgeTriggerState& s = state[i].edgeTrigger;
                if (keyDown && !s.fired) {
                    PressKey(desc.outputVk);
                    tracker.press(desc.outputVk);
                    Sleep(desc.durationMs);
                    ReleaseKey(desc.outputVk);
                    tracker.release(desc.outputVk);
                    s.fired = true;
                } else if (!keyDown) {
                    s.fired = false;
                }
                break;
            }

            case BehaviorType::LongPress: {
                LongPressState& s = state[i].longPress;
                if (keyDown) {
                    if (!s.keyDown) {
                        // Rising edge: record press start
                        s.pressTime = GetTickCount64();
                        s.keyDown   = true;
                        s.longFired = false;
                    } else if (!s.longFired) {
                        // Still held: check if threshold elapsed
                        ULONGLONG elapsed = GetTickCount64() - s.pressTime;
                        if ((int)elapsed >= desc.thresholdMs) {
                            PressKey(desc.longOutputVk);
                            tracker.press(desc.longOutputVk);
                            Sleep(30);
                            ReleaseKey(desc.longOutputVk);
                            tracker.release(desc.longOutputVk);
                            s.longFired = true;
                        }
                    }
                } else {
                    if (s.keyDown) {
                        // Falling edge
                        if (!s.longFired) {
                            // Tap: fire short output
                            PressKey(desc.outputVk);
                            tracker.press(desc.outputVk);
                            Sleep(30);
                            ReleaseKey(desc.outputVk);
                            tracker.release(desc.outputVk);
                        }
                        s.keyDown   = false;
                        s.longFired = false;
                    }
                }
                break;
            }

            case BehaviorType::WheelToKey: {
                WheelToKeyState& s = state[i].wheelToKey;
                if (keyDown && !s.pressed) {
                    INPUT inp = {};
                    inp.type         = INPUT_MOUSE;
                    inp.mi.dwFlags   = MOUSEEVENTF_WHEEL;
                    inp.mi.mouseData = desc.wheelDelta;
                    SendInput(1, &inp, sizeof(INPUT));
                    s.pressed = true;
                } else if (!keyDown) {
                    s.pressed = false;
                }
                break;
            }

            } // switch
        }

        Sleep(10);
    }

    // Thread exiting: release any keys still held
    tracker.releaseAll();
}
