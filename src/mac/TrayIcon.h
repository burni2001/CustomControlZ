#pragma once
#ifndef _WIN32

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <memory>

class GameEngine;
class SettingsWindow;

class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(GameEngine* engine, QObject* parent = nullptr);
    ~TrayIcon() override;

    void setActive(bool active);

private slots:
    void onProfileAction(int index);
    void onSettingsTriggered();
    void onIconActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void rebuildMenu();

    GameEngine*                    m_engine   = nullptr;
    std::unique_ptr<QSystemTrayIcon> m_tray;
    std::unique_ptr<QMenu>           m_menu;
    std::unique_ptr<SettingsWindow>  m_settings;

    QAction*      m_statusAction  = nullptr;   // non-clickable status label
    QActionGroup* m_gameGroup     = nullptr;
    bool          m_active        = false;
};

#endif // !_WIN32
