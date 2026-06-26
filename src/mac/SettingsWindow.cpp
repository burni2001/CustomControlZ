#include "SettingsWindow.h"
#include "GameEngine.h"
#include "IniConfig.h"

#include "../platform/mac_key_helpers.h"
#include "../GameProfiles.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QFrame>
#include <QFont>
#include <QKeyEvent>
#include <QDialog>
#include <QLabel>
#include <mutex>

// ---------------------------------------------------------------------------
// VK → readable label
// ---------------------------------------------------------------------------
QString BindingRow::vkToLabel(int vk) {
    if (vk == 0) return "—";
    // Common named keys
    switch (vk) {
        case 0x08: return "Backspace";
        case 0x09: return "Tab";
        case 0x0D: return "Enter";
        case 0x10: return "Shift";
        case 0x11: return "Ctrl";
        case 0x12: return "Alt";
        case 0x14: return "CapsLock";
        case 0x1B: return "Esc";
        case 0x20: return "Space";
        case 0x21: return "PgUp";
        case 0x22: return "PgDn";
        case 0x23: return "End";
        case 0x24: return "Home";
        case 0x25: return "←";
        case 0x26: return "↑";
        case 0x27: return "→";
        case 0x28: return "↓";
        case 0x2E: return "Del";
        case 0x01: return "LBtn";
        case 0x02: return "RBtn";
        case 0x04: return "MBtn";
        case 0x05: return "X1Btn";
        case 0x06: return "X2Btn";
        case 0xA0: return "LShift";
        case 0xA1: return "RShift";
        case 0xA2: return "LCtrl";
        case 0xA3: return "RCtrl";
        case 0xA4: return "LAlt";
        case 0xA5: return "RAlt";
    }
    if (vk >= 0x30 && vk <= 0x39) return QString(QChar(vk));          // 0–9
    if (vk >= 0x41 && vk <= 0x5A) return QString(QChar(vk));          // A–Z
    if (vk >= 0x70 && vk <= 0x7B) return QString("F%1").arg(vk - 0x6F); // F1–F12
    return QString("0x%1").arg(vk, 2, 16, QLatin1Char('0')).toUpper();
}

// ---------------------------------------------------------------------------
// Key capture dialog
// ---------------------------------------------------------------------------
class KeyCaptureDialog : public QDialog {
    Q_OBJECT
public:
    explicit KeyCaptureDialog(QWidget* parent = nullptr)
        : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint)
    {
        setWindowTitle("Press a key…");
        auto* lay = new QVBoxLayout(this);
        auto* lbl = new QLabel("Press any key to bind it.\n\nPress Escape to cancel.", this);
        lbl->setAlignment(Qt::AlignCenter);
        lay->addWidget(lbl);
        setFixedSize(240, 100);
        setFocusPolicy(Qt::StrongFocus);
    }

    int capturedVk() const { return m_vk; }

protected:
    void keyPressEvent(QKeyEvent* ev) override {
        int key = ev->key();
        if (key == Qt::Key_Escape) { reject(); return; }

        // Map Qt key to Windows VK
        m_vk = qtKeyToVk(key, ev->nativeVirtualKey());
        if (m_vk) accept();
    }

private:
    int m_vk = 0;

    static int qtKeyToVk(int qtKey, quint32 nativeVk) {
        // Prefer the native VK if it's a simple ASCII key (A-Z, 0-9)
        if (nativeVk >= 0x30 && nativeVk <= 0x5A) return (int)nativeVk;

        switch (qtKey) {
            case Qt::Key_Space:     return 0x20;
            case Qt::Key_Return:
            case Qt::Key_Enter:     return 0x0D;
            case Qt::Key_Backspace: return 0x08;
            case Qt::Key_Tab:       return 0x09;
            case Qt::Key_Escape:    return 0x1B;
            case Qt::Key_Delete:    return 0x2E;
            case Qt::Key_Left:      return 0x25;
            case Qt::Key_Up:        return 0x26;
            case Qt::Key_Right:     return 0x27;
            case Qt::Key_Down:      return 0x28;
            case Qt::Key_Home:      return 0x24;
            case Qt::Key_End:       return 0x23;
            case Qt::Key_PageUp:    return 0x21;
            case Qt::Key_PageDown:  return 0x22;
            case Qt::Key_Shift:     return 0x10;
            case Qt::Key_Control:   return 0x11;
            case Qt::Key_Alt:       return 0x12;
            case Qt::Key_CapsLock:  return 0x14;
            case Qt::Key_F1:  return 0x70; case Qt::Key_F2:  return 0x71;
            case Qt::Key_F3:  return 0x72; case Qt::Key_F4:  return 0x73;
            case Qt::Key_F5:  return 0x74; case Qt::Key_F6:  return 0x75;
            case Qt::Key_F7:  return 0x76; case Qt::Key_F8:  return 0x77;
            case Qt::Key_F9:  return 0x78; case Qt::Key_F10: return 0x79;
            case Qt::Key_F11: return 0x7A; case Qt::Key_F12: return 0x7B;
            case Qt::Key_Semicolon:  return 0xBA;
            case Qt::Key_Equal:      return 0xBB;
            case Qt::Key_Comma:      return 0xBC;
            case Qt::Key_Minus:      return 0xBD;
            case Qt::Key_Period:     return 0xBE;
            case Qt::Key_Slash:      return 0xBF;
            case Qt::Key_QuoteLeft:  return 0xC0;
            case Qt::Key_BracketLeft:  return 0xDB;
            case Qt::Key_Backslash:    return 0xDC;
            case Qt::Key_BracketRight: return 0xDD;
            case Qt::Key_Apostrophe:   return 0xDE;
            default: return 0;
        }
    }
};

// Need moc for KeyCaptureDialog inside the .cpp
#include "SettingsWindow.moc"

// ---------------------------------------------------------------------------
// BindingRow
// ---------------------------------------------------------------------------
BindingRow::BindingRow(GameProfile* profile, int bindingIndex, QWidget* parent)
    : QWidget(parent), m_profile(profile), m_bindingIndex(bindingIndex)
{
    const KeyBinding& b = profile->bindings[bindingIndex];

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 2, 8, 2);

    // Label
    auto* lbl = new QLabel(QString::fromWCharArray(b.label), this);
    lbl->setMinimumWidth(220);
    lay->addWidget(lbl);

    lay->addStretch();

    // Key button
    m_keyButton = new QPushButton(vkToLabel(b.currentVk), this);
    m_keyButton->setFixedWidth(70);
    m_keyButton->setFlat(false);
    lay->addWidget(m_keyButton);

    // Rebind button
    auto* rebindBtn = new QPushButton("…", this);
    rebindBtn->setFixedWidth(28);
    lay->addWidget(rebindBtn);

    connect(rebindBtn, &QPushButton::clicked, this, &BindingRow::onRebindClicked);
}

void BindingRow::updateDisplay() {
    m_keyButton->setText(vkToLabel(m_profile->bindings[m_bindingIndex].currentVk));
}

void BindingRow::onRebindClicked() {
    // Suspend the behavior engine while rebinding
    g_waitingForBindID = m_bindingIndex + 1;

    KeyCaptureDialog dlg(this);
    dlg.exec();

    if (dlg.result() == QDialog::Accepted && dlg.capturedVk()) {
        {
            std::lock_guard<std::mutex> lk(g_configMutex);
            m_profile->bindings[m_bindingIndex].currentVk = static_cast<WORD>(dlg.capturedVk());
        }
        updateDisplay();
        emit bindingChanged();
    }

    g_waitingForBindID = 0;
}

// ---------------------------------------------------------------------------
// SettingsWindow
// ---------------------------------------------------------------------------
SettingsWindow::SettingsWindow(GameEngine* engine, QWidget* parent)
    : QDialog(parent, Qt::Window)
    , m_engine(engine)
{
    setWindowTitle("CustomControlZ — Settings");
    setMinimumWidth(460);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(12, 12, 12, 12);

    // Header row: title on left, borderless game selector (•••) on right
    auto* headerLayout = new QHBoxLayout();

    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    headerLayout->addWidget(m_titleLabel, 1);

    m_gameSelectButton = new QToolButton(this);
    m_gameSelectButton->setText("•••");
    m_gameSelectButton->setPopupMode(QToolButton::InstantPopup);
    m_gameSelectButton->setAutoRaise(true);
    m_gameSelectButton->setStyleSheet(
        "QToolButton { border: none; background: transparent; font-size: 16px; }"
        "QToolButton::menu-indicator { image: none; }");
    m_gameMenu = new QMenu(this);
    for (int i = 0; i < m_engine->profileCount(); i++) {
        GameProfile* p = m_engine->profileAt(i);
        QAction* act = m_gameMenu->addAction(QString::fromWCharArray(p->displayName));
        connect(act, &QAction::triggered, this, [this, i] {
            m_engine->selectProfile(i);
            refresh();
        });
    }
    m_gameSelectButton->setMenu(m_gameMenu);
    headerLayout->addWidget(m_gameSelectButton, 0, Qt::AlignRight | Qt::AlignVCenter);

    m_mainLayout->addLayout(headerLayout);

    // Rows container — built fresh on each refresh(), window sizes to content
    m_rowsWidget = new QWidget(this);
    m_rowsLayout = new QVBoxLayout(m_rowsWidget);
    m_rowsLayout->setSpacing(2);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->addWidget(m_rowsWidget);

    // Save button
    m_saveButton = new QPushButton("Save", this);
    connect(m_saveButton, &QPushButton::clicked, this, [this] {
        m_engine->saveConfig();
        close();
    });
    m_mainLayout->addWidget(m_saveButton);
}

void SettingsWindow::refresh() {
    m_rows.clear();

    // Replace the rows widget entirely so no stale children survive.
    // Deleting m_rowsWidget recursively destroys all child widgets (rows, separators, etc.).
    m_mainLayout->removeWidget(m_rowsWidget);
    delete m_rowsWidget;

    m_rowsWidget = new QWidget(this);
    m_rowsLayout = new QVBoxLayout(m_rowsWidget);
    m_rowsLayout->setSpacing(2);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->insertWidget(1, m_rowsWidget);  // between header and Save

    GameProfile* profile = m_engine->profileAt(m_engine->selectedIndex());
    if (!profile) { adjustSize(); return; }

    m_titleLabel->setText(QString::fromWCharArray(profile->settingsTitle));

    for (int i = 0; i < profile->bindingCount; i++) {
        const KeyBinding& b = profile->bindings[i];

        if (b.separatorAbove) {
            auto* line = new QFrame(m_rowsWidget);
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            m_rowsLayout->addWidget(line);
        }

        auto* row = new BindingRow(profile, i, m_rowsWidget);
        m_rows.push_back(row);
        m_rowsLayout->addWidget(row);
    }

    // Let Qt compute natural sizes then snap the window to fit.
    m_mainLayout->activate();
    adjustSize();
}
