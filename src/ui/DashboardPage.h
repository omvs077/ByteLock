#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class MainViewModel;

class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPage(MainViewModel* viewModel, QWidget* parent = nullptr);

private slots:
    void onCheckOpenSslClicked();
    void onRunCryptoSelfTestClicked();
    void onLockFolderClicked();
    void onUnlockFolderClicked();

    void onBusyChanged(bool busy);
    void onStatusTextChanged(const QString& text);

private:
    void setupUi();
    void setButtonsEnabled(bool enabled);

    MainViewModel* m_viewModel = nullptr;

    QLabel* m_statusLabel = nullptr;
    QPushButton* m_checkButton = nullptr;
    QPushButton* m_selfTestButton = nullptr;
    QPushButton* m_lockFolderButton = nullptr;
    QPushButton* m_unlockFolderButton = nullptr;
};
