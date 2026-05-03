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
inline bool IsPhysKeyDown(WORD vk); // physical-only: ignores SendInput-injected keys
inline void PressVk(WORD vk);       // routes to PressKey or PressMouse based on VK
inline void ReleaseVk(WORD vk);     // routes to ReleaseKey or ReleaseMouse based on VK
inline void PressMouse(WORD vk);    // Sends MOUSEEVENTF_*DOWN for VK_LBUTTON / VK_RBUTTON / VK_MBUTTON
inline void ReleaseMouse(WORD vk);  // Sends MOUSEEVENTF_*UP
bool IsGameRunning(GameProfile* profile);
bool IsGameWindowForeground(GameProfile* profile);
void SetTrayIconState(bool active, GameProfile* profile);
bool IsProcessRunningElevated(const wchar_t* processName);  // FIX-02: UIPI detection
extern std::mutex g_configMutex;
extern std::atomic<int> g_waitingForBindID;

// --- BEHAVIOR TYPES ---

enum class ReturnWeapon : uint8_t { Primary = 0, Secondary = 1, Auto = 2 };

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
    WalkRunSwap,     // directional key: alone→inject dir+walk modifier; with sprint key→inject dir only
    WalkModifier,    // no-op marker: configurable walk modifier key referenced by WalkRunSwap
    DashKey,         // no-op marker: configurable dash/dodge key referenced by SprintHoldDash
    SprintHoldDash,  // hold sprint key → hold DashKey's VK (sprinting via dash-hold in game)
    InGameKey,       // no-op marker: labels a configurable in-game key; referenced by SimulateKey siblings
    SimulateKey,     // rising edge on inputVk → pulse nearest preceding InGameKey sibling's VK
};

struct BehaviorDescriptor {
    BehaviorType   type             = BehaviorType::HoldToToggle;
    WORD           outputVk         = 0;       // HoldToToggle, EdgeTrigger, LongPress short output; WeaponCombo: switch-to key
    WORD           longOutputVk     = 0;       // LongPress: output on sustained hold; WeaponCombo: switch-back key
    int            thresholdMs      = 400;     // LongPress/WeaponCombo: hold threshold in milliseconds
    int            durationMs       = 50;      // EdgeTrigger: pulse duration; WeaponCombo: delay after weapon switch key (ms)
    DWORD          wheelDelta        = 120;    // WheelToKey/WheelToggle: MOUSEEVENTF_WHEEL mouseData
    WORD           attackVk          = 0;     // WeaponCombo/MeleeBurst: attack button VK (e.g. VK_LBUTTON)
    int            returnDelayMs     = 500;   // MeleeBurst: ms of idle before auto-switching back
    const wchar_t* outputVkLabel     = nullptr; // If non-null, shows as a configurable output key row in the settings UI
    const wchar_t* longOutputVkLabel = nullptr; // If non-null, shows as a configurable long-output key row in the settings UI
    ReturnWeapon   returnWeapon      = ReturnWeapon::Primary; // MeleeBurst: which weapon to return to (Primary = longOutputVk, Secondary = returnAltVk)
    WORD           returnAltVk       = 0;    // MeleeBurst: secondary return weapon VK; when non-zero a Primary/Secondary dropdown appears in the UI
    WORD           tertiaryOutputVk       = 0;       // KeyToggle: third weapon key in cycle
    const wchar_t* tertiaryOutputVkLabel  = nullptr; // If non-null, shows as a configurable output key row in the settings UI
    bool           includeTertiaryInCycle = false;   // KeyToggle: include tertiary key in quickswitch cycle
    const wchar_t* checkboxLabel          = nullptr; // If non-null, renders a checkbox row below this binding
    bool           checkboxEnabled        = true;    // Runtime state of the checkbox; default on
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
    bool pressed         = false; // tracks rising edge (prevents repeat while held)
    int  cycleIndex      = 0;     // 0=outputVk, 1=longOutputVk, 2=tertiaryOutputVk
    bool outWasDown      = false; // rising-edge tracker for outputVk (manual weapon-key sync)
    bool altWasDown      = false; // rising-edge tracker for longOutputVk (manual weapon-key sync)
    bool tertiaryWasDown = false; // rising-edge tracker for tertiaryOutputVk (manual weapon-key sync)
};

struct MeleeBurstState {
    bool      inMelee            = false; // currently switched to melee weapon (outputVk held)
    bool      keyDown            = false;
    bool      thresholdHit       = false; // true once long-press threshold exceeded
    bool      holding            = false; // true while attack button is held (long-press mode)
    ULONGLONG pressTime          = 0;     // time of current rising edge (for long-press threshold)
    ULONGLONG lastPressTime      = 0;     // time of last key event (for return-to-main timer)
    WORD      lastUsedWeaponVk   = 0;     // Auto mode: last weapon key pressed before entering melee
    bool      primaryWasDown     = false; // rising-edge tracker for longOutputVk (Auto mode)
    bool      secondaryWasDown   = false; // rising-edge tracker for returnAltVk (Auto mode)
    bool      heavyWasDown       = false; // rising-edge tracker for heavy weapon key (Auto mode)
};

struct GlobalSuspendState {
    bool pressed   = false; // tracks rising edge (prevents repeat while held)
    bool suspended = false; // current toggle state
};

struct SimulateKeyState {
    bool pressed = false;
};

struct SprintHoldDashState {
    bool held            = false; // currently holding the dash key via sprint
    WORD pressedVk       = 0;     // VK actually pressed (for correct release when DashKey is rebound)
    bool dashPhysWasDown = false; // rising-edge tracker for physical dash key press
};

struct WalkRunSwapState {
    bool dirSent      = false; // currently injecting directional key
    bool modSent      = false; // currently injecting walk modifier key
    WORD pressedDirVk = 0;     // actual VK pressed for direction (for correct release)
    WORD pressedModVk = 0;     // actual VK pressed for modifier (for correct release)
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
    GlobalSuspendState  globalSuspend;
    WalkRunSwapState    walkRunSwap;
    SprintHoldDashState sprintHoldDash;
    SimulateKeyState    simulateKey;
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
