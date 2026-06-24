#include <QApplication>
#include <QMessageBox>

#include "GameEngine.h"
#include "TrayIcon.h"
#include "IniConfig.h"

#include "../platform/platform.h"
#include "../platform/mac_key_helpers.h"
#include "../GameProfiles.h"

int main(int argc, char* argv[]) {
    // QApplication must exist before any Qt objects are created.
    // LSUIElement=true in Info.plist suppresses the Dock icon.
    QApplication app(argc, argv);
    app.setApplicationName("CustomControlZ");
    app.setApplicationVersion("1.0");
    app.setQuitOnLastWindowClosed(false);  // stay alive as a menu bar app

    // System tray is required on macOS (always available)
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "CustomControlZ",
            "System tray is not available on this system.");
        return 1;
    }

    // Create the platform (installs CGEventTap, sets g_platform)
    std::unique_ptr<Platform> platform(createPlatform());

    // Load config
    std::string cfgPath = platform->configFilePath();
    g_config.load(cfgPath);

    // Create game engine, load config for the default profile
    GameEngine engine;

    // Read last-used game index from config
    int lastGame = g_config.getInt("App", "LastGame", 0);
    if (lastGame < 0 || lastGame >= engine.profileCount()) lastGame = 0;

    engine.loadConfig();    // load VK codes for profile 0
    engine.selectProfile(lastGame);  // this re-loads config for lastGame and starts the thread

    // Install keyboard hook — the engine's behavior thread calls IsKeyDown
    // which reads physical key state tracked by this hook.
    platform->startKeyboardHook([](MacKeyEvent) -> bool {
        return false;  // don't suppress any physical key events
    });

    // Create tray icon (must be after QApplication).
    // TrayIcon constructor wires engine.onActiveStateChange → tray.setActive.
    TrayIcon tray(&engine);

    // Connect the platform's tray state notification to the engine's active-state callback.
    // Flow: GenericLogicThreadFn → SetTrayIconState → platform->notifyTrayState
    //       → platform->onTrayStateChange (here) → engine.onActiveStateChange → tray.setActive
    platform->onTrayStateChange = [&engine](bool active) {
        if (engine.onActiveStateChange) engine.onActiveStateChange(active);
    };

    // Save last-used game index on profile change
    engine.onProfileChanged = [&](int index) {
        g_config.writeInt("App", "LastGame", index);
    };

    int result = app.exec();

    // Persist config on exit
    engine.saveConfig();
    g_config.writeInt("App", "LastGame", engine.selectedIndex());

    return result;
}
