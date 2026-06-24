#include "SettingsWindow.h"
#include "GameEngine.h"
#include "IniConfig.h"

#include "../platform/mac_key_helpers.h"
#include "../GameProfiles.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QFrame>
#include <QSizePolicy>
#include <QFont>
#include <QKeyEvent>
#include <QDialog>
#include <QLabel>
#include <QDebug>
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

    auto* mainLayout = new QVBoxLayout(this);

    // Title label
    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_titleLabel);

    // Scroll area for binding rows
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_rowsWidget = new QWidget(m_scrollArea);
    m_rowsLayout = new QVBoxLayout(m_rowsWidget);
    m_rowsLayout->setSpacing(2);
    m_rowsLayout->addStretch();
    m_scrollArea->setWidget(m_rowsWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    // Save button
    m_saveButton = new QPushButton("Save", this);
    connect(m_saveButton, &QPushButton::clicked, this, [this] {
        m_engine->saveConfig();
        close();
    });
    mainLayout->addWidget(m_saveButton);
}

void SettingsWindow::refresh() {
    // Clear existing rows
    for (auto* r : m_rows) r->deleteLater();
    m_rows.clear();
    // Remove all items except the trailing stretch
    while (m_rowsLayout->count() > 1)
        delete m_rowsLayout->takeAt(0);

    GameProfile* profile = m_engine->profileAt(m_engine->selectedIndex());
    if (!profile) return;

    m_titleLabel->setText(QString::fromWCharArray(profile->settingsTitle));

    for (int i = 0; i < profile->bindingCount; i++) {
        const KeyBinding& b = profile->bindings[i];

        // Separator
        if (b.separatorAbove) {
            auto* line = new QFrame(m_rowsWidget);
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            m_rowsLayout->insertWidget(m_rowsLayout->count() - 1, line);
        }

        // InGameKey, WalkModifier, DashKey: show as read-only info row
        bool showRebind =
            b.behavior.type != BehaviorType::WalkModifier &&
            b.behavior.type != BehaviorType::DashKey;

        if (!showRebind) {
            auto* row = new QWidget(m_rowsWidget);
            auto* lay = new QHBoxLayout(row);
            lay->setContentsMargins(8, 2, 8, 2);
            lay->addWidget(new QLabel(QString::fromWCharArray(b.label), row));
            lay->addStretch();
            lay->addWidget(new QLabel(BindingRow::vkToLabel(b.currentVk), row));
            m_rowsLayout->insertWidget(m_rowsLayout->count() - 1, row);
            continue;
        }

        auto* row = new BindingRow(profile, i, m_rowsWidget);
        connect(row, &BindingRow::bindingChanged, this, [] {
            // No-op: changes are live; Save button persists them
        });
        m_rows.push_back(row);
        m_rowsLayout->insertWidget(m_rowsLayout->count() - 1, row);
    }

    // Size the dialog to show all rows without a scrollbar.
    // Force the layout to compute sizes first, then temporarily set the scroll area's
    // minimum height to the full content height so adjustSize() expands the window.
    m_rowsLayout->activate();
    m_scrollArea->setMinimumHeight(m_rowsWidget->sizeHint().height());
    adjustSize();
    m_scrollArea->setMinimumHeight(0);  // allow manual resize afterwards
}
