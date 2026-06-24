#pragma once

#include <QMainWindow>

class QLabel;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onCheckOpenSslClicked();
    void onRunCryptoSelfTestClicked();
    void onLockFolderClicked();
    void onUnlockFolderClicked();

private:
    void setupUi();

    QLabel* m_statusLabel = nullptr;
    QPushButton* m_checkButton = nullptr;
    QPushButton* m_selfTestButton = nullptr;
    QPushButton* m_lockFolderButton = nullptr;
    QPushButton* m_unlockFolderButton = nullptr;
};
