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
inline void PressMouse(WORD vk);    // Sends MOUSEEVENTF_*DOWN for VK_LBUTTON / VK_RBUTTON / VK_MBUTTON
inline void ReleaseMouse(WORD vk);  // Sends MOUSEEVENTF_*UP
bool IsGameRunning(GameProfile* profile);
void SetTrayIconState(bool active, GameProfile* profile);
bool IsProcessRunningElevated(const wchar_t* processName);  // FIX-02: UIPI detection
extern std::mutex g_configMutex;
extern std::atomic<int> g_waitingForBindID;

// --- BEHAVIOR TYPES ---

enum class BehaviorType : uint8_t {
    HoldToToggle,   // hold inputVk -> hold outputVk continuously
    EdgeTrigger,    // rising edge on inputVk -> pulse outputVk for durationMs
    LongPress,      // tap -> pulse shortOutputVk; hold >= thresholdMs -> pulse longOutputVk
    WheelToKey,     // rising edge on inputVk -> send mouse wheel delta (wheelDelta)
    WheelToggle,    // rising edge on inputVk -> alternate between +wheelDelta and -wheelDelta each press
    WeaponCombo,    // tap -> switch weapon + click + switch back; hold -> switch weapon + hold attack + switch back on release
    MeleeBurst,     // repeated taps stay on melee weapon; auto-switch back after returnDelayMs of idle
    KeyToggle,      // rising edge on inputVk -> alternate between pulsing outputVk and longOutputVk each press
    GlobalSuspend,  // toggle: first press suspends all other bindings; second press re-enables
};

struct BehaviorDescriptor {
    BehaviorType   type             = BehaviorType::HoldToToggle;
    WORD           outputVk         = 0;       // HoldToToggle, EdgeTrigger, LongPress short output; WeaponCombo: switch-to key
    WORD           longOutputVk     = 0;       // LongPress: output on sustained hold; WeaponCombo: switch-back key
    int            thresholdMs      = 400;     // LongPress/WeaponCombo: hold threshold in milliseconds
    int            durationMs       = 50;      // EdgeTrigger: pulse duration; WeaponCombo: delay after weapon switch key (ms)
    DWORD          wheelDelta        = 120;    // WheelToKey/WheelToggle: MOUSEEVENTF_WHEEL mouseData
    WORD           attackVk          = 0;     // WeaponCombo/MeleeBurst: attack button VK (e.g. VK_LBUTTON)
    int            returnDelayMs     = 500;   // MeleeBurst: ms of idle before auto-switching back to main weapon
    const wchar_t* outputVkLabel     = nullptr; // If non-null, shows as a configurable output key row in the settings UI
    const wchar_t* longOutputVkLabel = nullptr; // If non-null, shows as a configurable long-output key row in the settings UI
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

struct WheelToggleState {
    bool pressed   = false;  // tracks rising edge (prevents repeat while held)
    bool scrollUp  = true;   // alternates each press: true = +wheelDelta, false = -wheelDelta
};

struct WeaponComboState {
    bool      keyDown      = false;
    bool      thresholdHit = false;  // true once long-press threshold exceeded
    bool      holding      = false;  // true while attack button is held (long-press mode)
    ULONGLONG pressTime    = 0;
};

struct KeyToggleState {
    bool pressed = false; // tracks rising edge (prevents repeat while held)
    bool useAlt  = false; // alternates: false = outputVk, true = longOutputVk
};

struct MeleeBurstState {
    bool      inMelee       = false; // currently switched to melee weapon (outputVk held)
    bool      keyDown       = false;
    bool      thresholdHit  = false; // true once long-press threshold exceeded
    bool      holding       = false; // true while attack button is held (long-press mode)
    ULONGLONG pressTime     = 0;     // time of current rising edge (for long-press threshold)
    ULONGLONG lastPressTime = 0;     // time of last key event (for return-to-main timer)
};

struct GlobalSuspendState {
    bool pressed   = false; // tracks rising edge (prevents repeat while held)
    bool suspended = false; // current toggle state
};

union BindingState {
    HoldToToggleState holdToggle;
    EdgeTriggerState  edgeTrigger;
    LongPressState    longPress;
    WheelToKeyState   wheelToKey;
    WheelToggleState  wheelToggle;
    WeaponComboState  weaponCombo;
    KeyToggleState    keyToggle;
    MeleeBurstState   meleeBurst;
    GlobalSuspendState globalSuspend;
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
// Body is defined in GameProfiles.h after GameProfile is fully defined.
void GenericLogicThreadFn(GameProfile* profile, std::atomic<bool>& running);
