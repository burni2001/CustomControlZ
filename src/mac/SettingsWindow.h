#pragma once
#ifndef _WIN32

#include <QDialog>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <vector>

class GameEngine;
struct GameProfile;

// A single row in the settings table: "Label    [KEY]  [...]"
class BindingRow : public QWidget {
    Q_OBJECT
public:
    BindingRow(GameProfile* profile, int bindingIndex, QWidget* parent = nullptr);

    void updateDisplay();  // refreshes the button label from currentVk

    static QString vkToLabel(int vk);  // public so SettingsWindow can reuse it

signals:
    void bindingChanged();

private slots:
    void onRebindClicked();

private:
    GameProfile* m_profile;
    int          m_bindingIndex;
    QPushButton* m_keyButton;
};

class SettingsWindow : public QDialog {
    Q_OBJECT
public:
    explicit SettingsWindow(GameEngine* engine, QWidget* parent = nullptr);

    // Rebuild the binding list for the current game profile
    void refresh();

private:
    GameEngine*  m_engine       = nullptr;
    QVBoxLayout* m_rowsLayout   = nullptr;
    QWidget*     m_rowsWidget   = nullptr;
    QLabel*      m_titleLabel   = nullptr;
    QPushButton* m_saveButton   = nullptr;

    std::vector<BindingRow*> m_rows;
};

#endif // !_WIN32
