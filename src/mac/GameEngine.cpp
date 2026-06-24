// IMPORTANT: mac_key_helpers.h must be first — it provides the inline
// implementations (PressKey, ReleaseKey, IsKeyDown…) that GameProfiles.h
// (via GenericLogicThreadFn) calls at compile time.
#include "../platform/mac_key_helpers.h"

// Now pull in the shared game profiles.  The #include <windows.h> inside
// BehaviorEngine.h will resolve to src/platform/windows.h (our shim) because
// CMakeLists.txt puts src/platform first in the include path.
#include "../GameProfiles.h"

// Individual game profiles
#include "../games/EldenRing.h"
#include "../games/DarkSoulsIII.h"
#include "../games/ToxicCommando.h"
#include "../games/Darktide.h"
#include "../games/Valheim.h"

#include "GameEngine.h"
#include "IniConfig.h"

#include <mutex>
#include <atomic>
#include <cassert>

// ---------------------------------------------------------------------------
// Globals declared in mac_key_helpers.h / platform.h
// ---------------------------------------------------------------------------
std::mutex       g_configMutex;
std::atomic<int> g_waitingForBindID{ 0 };
IniConfig        g_config;

// ---------------------------------------------------------------------------
// Global profile registry
// All game profiles available on macOS. Add new profiles here.
// ---------------------------------------------------------------------------
static GameProfile* s_profiles[] = {
    &g_EldenRingProfile,
    &g_DarkSoulsIIIProfile,
    &g_ToxicCommandoProfile,
    &g_DarktideProfile,
    &g_ValheimProfile,
};
static constexpr int kProfileCount = static_cast<int>(sizeof(s_profiles) / sizeof(s_profiles[0]));

// ---------------------------------------------------------------------------
// Platform helpers called by GenericLogicThreadFn (declared in mac_key_helpers.h)
// Bodies here have access to GameProfile because GameProfiles.h is included above.
// ---------------------------------------------------------------------------
bool IsGameRunning(GameProfile* profile) {
    if (!g_platform || !profile) return false;
    if (g_platform->isProcessRunning(profile->processName1)) return true;
    if (profile->processName2 && g_platform->isProcessRunning(profile->processName2)) return true;
    return false;
}

bool IsGameWindowForeground(GameProfile* profile) {
    if (!g_platform || !profile) return false;
    if (g_platform->isAppInForeground(profile->processName1)) return true;
    if (profile->processName2 && g_platform->isAppInForeground(profile->processName2)) return true;
    return false;
}

void SetTrayIconState(bool active, GameProfile* /*profile*/) {
    if (g_platform) g_platform->notifyTrayState(active);
}

// ---------------------------------------------------------------------------
// GameEngine
// ---------------------------------------------------------------------------
GameEngine::GameEngine() = default;

GameEngine::~GameEngine() {
    stopThread();
}

int GameEngine::profileCount() const {
    return kProfileCount;
}

GameProfile* GameEngine::profileAt(int index) const {
    if (index < 0 || index >= kProfileCount) return nullptr;
    return s_profiles[index];
}

void GameEngine::selectProfile(int index) {
    if (index < 0 || index >= kProfileCount) return;
    stopThread();
    m_selectedIndex = index;
    loadConfig();
    startThread();
    if (onProfileChanged) onProfileChanged(index);
}

// ---------------------------------------------------------------------------
// Config load / save
// ---------------------------------------------------------------------------
void GameEngine::loadConfig() {
    GameProfile* profile = profileAt(m_selectedIndex);
    if (!profile) return;

    std::lock_guard<std::mutex> lk(g_configMutex);
    for (int i = 0; i < profile->bindingCount; i++) {
        KeyBinding& b = profile->bindings[i];
        int vk = g_config.getInt(profile->iniSection, b.iniKey, static_cast<int>(b.defaultVk));
        b.currentVk = static_cast<WORD>(vk);

        // Output VK labels (configurable output keys)
        if (b.behavior.outputVkLabel) {
            std::wstring outKey(b.iniKey);
            outKey += L"_OutputKey";
            int ovk = g_config.getInt(profile->iniSection, outKey.c_str(),
                                       static_cast<int>(b.behavior.outputVk));
            b.behavior.outputVk = static_cast<WORD>(ovk);
        }
        if (b.behavior.longOutputVkLabel) {
            std::wstring outKey(b.iniKey);
            outKey += L"_LongOutputKey";
            int ovk = g_config.getInt(profile->iniSection, outKey.c_str(),
                                       static_cast<int>(b.behavior.longOutputVk));
            b.behavior.longOutputVk = static_cast<WORD>(ovk);
        }
        if (b.behavior.tertiaryOutputVkLabel) {
            std::wstring outKey(b.iniKey);
            outKey += L"_TertiaryOutputKey";
            int ovk = g_config.getInt(profile->iniSection, outKey.c_str(),
                                       static_cast<int>(b.behavior.tertiaryOutputVk));
            b.behavior.tertiaryOutputVk = static_cast<WORD>(ovk);
        }
    }
}

void GameEngine::saveConfig() {
    GameProfile* profile = profileAt(m_selectedIndex);
    if (!profile) return;

    std::lock_guard<std::mutex> lk(g_configMutex);
    for (int i = 0; i < profile->bindingCount; i++) {
        const KeyBinding& b = profile->bindings[i];
        g_config.writeInt(profile->iniSection, b.iniKey, static_cast<int>(b.currentVk));

        if (b.behavior.outputVkLabel) {
            std::wstring outKey(b.iniKey); outKey += L"_OutputKey";
            g_config.writeInt(profile->iniSection, outKey.c_str(),
                              static_cast<int>(b.behavior.outputVk));
        }
        if (b.behavior.longOutputVkLabel) {
            std::wstring outKey(b.iniKey); outKey += L"_LongOutputKey";
            g_config.writeInt(profile->iniSection, outKey.c_str(),
                              static_cast<int>(b.behavior.longOutputVk));
        }
        if (b.behavior.tertiaryOutputVkLabel) {
            std::wstring outKey(b.iniKey); outKey += L"_TertiaryOutputKey";
            g_config.writeInt(profile->iniSection, outKey.c_str(),
                              static_cast<int>(b.behavior.tertiaryOutputVk));
        }
    }
}

// ---------------------------------------------------------------------------
// Thread management
// ---------------------------------------------------------------------------
void GameEngine::startThread() {
    GameProfile* profile = profileAt(m_selectedIndex);
    if (!profile || !profile->logicFn) return;

    m_threadRunning = true;
    m_thread = std::thread([this, profile]() {
        profile->logicFn(profile, m_threadRunning);
    });
}

void GameEngine::stopThread() {
    if (m_threadRunning) {
        m_threadRunning = false;
        if (m_thread.joinable()) m_thread.join();
    }
}
