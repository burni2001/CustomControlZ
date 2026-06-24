#include "TrayIcon.h"
#include "GameEngine.h"
#include "SettingsWindow.h"

#include "../platform/mac_key_helpers.h"
#include "../GameProfiles.h"

#include <QApplication>
#include <QPixmap>
#include <QPainter>
#include <QString>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static QIcon makeColorIcon(QColor color, bool active) {
    QPixmap px(22, 22);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    // Outer ring
    p.setPen(QPen(active ? QColor(80, 200, 80) : QColor(120, 120, 120), 2));
    p.setBrush(color);
    p.drawEllipse(3, 3, 16, 16);
    // Inner dot when active
    if (active) {
        p.setBrush(QColor(80, 200, 80));
        p.setPen(Qt::NoPen);
        p.drawEllipse(8, 8, 6, 6);
    }
    return QIcon(px);
}

static QString wcsToQt(const wchar_t* ws) {
    if (!ws) return {};
    return QString::fromWCharArray(ws);
}

// ---------------------------------------------------------------------------
// TrayIcon
// ---------------------------------------------------------------------------
TrayIcon::TrayIcon(GameEngine* engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_tray(std::make_unique<QSystemTrayIcon>(this))
{
    m_tray->setIcon(makeColorIcon(QColor(50, 50, 60), false));
    m_tray->setToolTip("CustomControlZ — Idle");

    connect(m_tray.get(), &QSystemTrayIcon::activated,
            this, &TrayIcon::onIconActivated);

    rebuildMenu();
    m_tray->show();

    // Wire up engine callbacks
    engine->onActiveStateChange = [this](bool active) {
        // Called from game engine thread; Qt is not thread-safe, so we use
        // a queued invocation via a lambda-based signal trick.
        QMetaObject::invokeMethod(this, [this, active]{ setActive(active); },
                                   Qt::QueuedConnection);
    };
}

TrayIcon::~TrayIcon() = default;

void TrayIcon::setActive(bool active) {
    m_active = active;
    GameProfile* p = m_engine->profileAt(m_engine->selectedIndex());
    QColor accent = p ? QColor(
        (p->theme.accent >> 16) & 0xFF,
        (p->theme.accent >>  8) & 0xFF,
         p->theme.accent        & 0xFF) : QColor(50, 50, 60);

    m_tray->setIcon(makeColorIcon(accent, active));

    const wchar_t* tip = p ? (active ? p->tipActive : p->tipIdle) : nullptr;
    m_tray->setToolTip(tip ? wcsToQt(tip) : "CustomControlZ");

    if (m_statusAction) {
        m_statusAction->setText(active
            ? QString("● Active — %1").arg(p ? wcsToQt(p->displayName) : "")
            : "○ Idle — waiting for game…");
    }
}

void TrayIcon::rebuildMenu() {
    m_menu = std::make_unique<QMenu>();

    // Status label (non-interactive)
    m_statusAction = m_menu->addAction("○ Idle — waiting for game…");
    m_statusAction->setEnabled(false);

    m_menu->addSeparator();

    // Game selector
    m_menu->addSection("Game");
    m_gameGroup = new QActionGroup(m_menu.get());
    m_gameGroup->setExclusive(true);
    for (int i = 0; i < m_engine->profileCount(); i++) {
        GameProfile* p = m_engine->profileAt(i);
        QAction* act = m_menu->addAction(wcsToQt(p->displayName));
        act->setCheckable(true);
        act->setChecked(i == m_engine->selectedIndex());
        m_gameGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, i]{ onProfileAction(i); });
    }

    m_menu->addSeparator();
    QAction* settingsAct = m_menu->addAction("Settings…");
    connect(settingsAct, &QAction::triggered, this, &TrayIcon::onSettingsTriggered);

    m_menu->addSeparator();
    QAction* quitAct = m_menu->addAction("Quit CustomControlZ");
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);

    m_tray->setContextMenu(m_menu.get());
}

void TrayIcon::onProfileAction(int index) {
    m_engine->selectProfile(index);
    setActive(false);   // reset until the new game is detected
    // Update checkmarks
    const auto actions = m_gameGroup->actions();
    for (int i = 0; i < actions.size(); i++)
        actions[i]->setChecked(i == index);
    // Refresh the settings window immediately if it's open
    if (m_settings && m_settings->isVisible())
        m_settings->refresh();
}

void TrayIcon::onSettingsTriggered() {
    if (!m_settings) {
        m_settings = std::make_unique<SettingsWindow>(m_engine);
    }
    m_settings->refresh();
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void TrayIcon::onIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        onSettingsTriggered();
    }
}
