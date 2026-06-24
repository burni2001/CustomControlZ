#pragma once
// macOS implementations of the helper functions declared in BehaviorEngine.h.
// Include this file BEFORE GameProfiles.h in any macOS translation unit that
// uses GenericLogicThreadFn (i.e. GameEngine.cpp).
//
// Include order inside that TU must be:
//   1. mac_key_helpers.h   (provides PressKey, ReleaseKey, IsKeyDown, etc.)
//   2. GameProfiles.h      (defines GenericLogicThreadFn which calls them)
#ifndef _WIN32

#include "win_types_compat.h"
#include "platform.h"
#include <mutex>
#include <atomic>

// ---------------------------------------------------------------------------
// Globals defined in GameEngine.cpp
// ---------------------------------------------------------------------------
extern std::mutex       g_configMutex;
extern std::atomic<int> g_waitingForBindID;

// ---------------------------------------------------------------------------
// Keyboard helpers (inline bodies for BehaviorEngine.h forward declarations)
// ---------------------------------------------------------------------------

inline void PressKey(WORD vk)   { if (g_platform) g_platform->injectKey(vk, true);  }
inline void ReleaseKey(WORD vk) { if (g_platform) g_platform->injectKey(vk, false); }

inline bool IsKeyDown(WORD vk) {
    return g_platform ? g_platform->isKeyDown(vk) : false;
}
inline bool IsPhysKeyDown(WORD vk) {
    return g_platform ? g_platform->isKeyPhysicallyDown(vk) : false;
}

// Mouse VKs: VK_LBUTTON=0x01, VK_RBUTTON=0x02, VK_MBUTTON=0x04,
//            VK_XBUTTON1=0x05, VK_XBUTTON2=0x06
static inline bool isMouse(WORD vk) {
    return vk == 0x01 || vk == 0x02 || vk == 0x04 || vk == 0x05 || vk == 0x06;
}

inline void PressVk(WORD vk) {
    if (!g_platform) return;
    if (isMouse(vk)) g_platform->injectMouseButton(vk, true);
    else             g_platform->injectKey(vk, true);
}
inline void ReleaseVk(WORD vk) {
    if (!g_platform) return;
    if (isMouse(vk)) g_platform->injectMouseButton(vk, false);
    else             g_platform->injectKey(vk, false);
}
inline void PressMouse(WORD vk) {
    if (g_platform) g_platform->injectMouseButton(vk, true);
}
inline void ReleaseMouse(WORD vk) {
    if (g_platform) g_platform->injectMouseButton(vk, false);
}

// ---------------------------------------------------------------------------
// Game state helpers — bodies are defined in GameEngine.cpp (after GameProfile
// is fully defined by GameProfiles.h).  Declared here as non-inline so they
// can be used by GenericLogicThreadFn which lives in the same TU.
// ---------------------------------------------------------------------------
struct GameProfile;
bool IsGameRunning(GameProfile* profile);
bool IsGameWindowForeground(GameProfile* profile);
void SetTrayIconState(bool active, GameProfile* profile);

// Elevation check: not applicable on macOS (no UIPI)
inline bool IsProcessRunningElevated(const wchar_t*) { return false; }

#endif // !_WIN32
