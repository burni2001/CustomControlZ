#pragma once
#ifndef _WIN32

#include <functional>
#include <string>
#include <cstdint>

// Event delivered to the keyboard hook callback.
// vk uses Windows Virtual Key codes (as defined in win_types_compat.h).
struct MacKeyEvent {
    int  vk;    // Windows VK code
    bool down;  // true = key pressed, false = key released
    bool injected; // true = event was produced by injectKey (should be ignored by behavior engine)
};

// Abstract platform interface. Implemented in platform_macos.mm.
class Platform {
public:
    virtual ~Platform() = default;

    // Install a global keyboard event tap.
    // callback receives every physical key event; returning true suppresses it.
    virtual void startKeyboardHook(std::function<bool(MacKeyEvent)> callback) = 0;
    virtual void stopKeyboardHook() = 0;

    // Inject a keyboard key press/release.
    virtual void injectKey(int vk, bool down) = 0;

    // Inject a mouse button press/release.
    // vk: VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2
    virtual void injectMouseButton(int vk, bool down) = 0;

    // Inject a mouse wheel scroll. delta: positive = up, negative = down (matches Windows convention).
    virtual void injectMouseWheel(int delta) = 0;

    // Query current key state.
    virtual bool isKeyDown(int vk) const = 0;         // true if key is down (physical or injected)
    virtual bool isKeyPhysicallyDown(int vk) const = 0; // true only if physically pressed

    // Process and window detection.
    virtual bool isProcessRunning(const wchar_t* processName) const = 0;
    virtual bool isAppInForeground(const wchar_t* processName) const = 0;

    // Config file path (platform-appropriate).
    virtual std::string configFilePath() const = 0;

    // Tray state notification (called from game engine thread).
    virtual void notifyTrayState(bool active) = 0;

    // Warning dialog (maps MessageBox calls from GenericLogicThreadFn).
    virtual void showWarning(const wchar_t* title, const wchar_t* text) = 0;
};

// Singleton — created once in main_mac.cpp, used everywhere via this pointer.
extern Platform* g_platform;

// SendInput implementation pointer (set by platform_macos.mm, called by win_types_compat.h stub)
extern "C" unsigned int mac_SendInput(unsigned int n, void* inputs, int cbSize);

Platform* createPlatform();

#endif // !_WIN32
