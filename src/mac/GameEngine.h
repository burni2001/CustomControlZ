#pragma once
#ifndef _WIN32

#include <atomic>
#include <thread>
#include <vector>
#include <functional>

struct GameProfile;

// GameEngine manages game detection and the behavior thread lifecycle.
// One instance lives for the duration of the app (owned by main_mac.cpp).
class GameEngine {
public:
    GameEngine();
    ~GameEngine();

    // Number of available game profiles
    int profileCount() const;

    // Access a profile by index (0 … profileCount()-1)
    GameProfile* profileAt(int index) const;

    // Currently selected profile index (-1 = none)
    int  selectedIndex() const  { return m_selectedIndex; }
    void selectProfile(int index);

    // Load all per-binding VK codes from g_config for the active profile
    void loadConfig();

    // Save all per-binding VK codes to g_config for the active profile
    void saveConfig();

    // Called by TrayIcon when the user changes the game via the menu
    std::function<void(int)>  onProfileChanged;   // new index
    // Called when the behavior engine activates/deactivates
    std::function<void(bool)> onActiveStateChange;

private:
    int m_selectedIndex = 0;

    std::thread        m_thread;
    std::atomic<bool>  m_threadRunning{ false };

    void stopThread();
    void startThread();
};

#endif // !_WIN32
