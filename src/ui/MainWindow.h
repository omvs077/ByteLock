#pragma once

#include <QMainWindow>

class QLabel;
class QPushButton;
class QProgressDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString& startupContainerPath = QString(), QWidget* parent = nullptr, const QString& lockFolderPath = QString());
    ~MainWindow() override;

private slots:
    void onCheckOpenSslClicked();
    void onRunCryptoSelfTestClicked();
    void onLockFolderClicked();
    void onUnlockFolderClicked();
    void onSettingsClicked();
    void onProgressChanged(quint64 done, quint64 total);

private:
    void setupUi();

    QLabel* m_statusLabel = nullptr;
    QPushButton* m_checkButton = nullptr;
    QPushButton* m_selfTestButton = nullptr;
    QPushButton* m_lockFolderButton = nullptr;
    QPushButton* m_unlockFolderButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    QString m_startupContainerPath;
    void showStartupUnlockPrompt();
    void unlockContainer(const QString& containerPath);
    void unlockContainerAttempt(const QString& containerPath, const QString& warningText);
    void unlockContainerSilent(const QString& containerPath);
    void lockContainer(const QString& folder, bool closeWhenDone = false);
    QProgressDialog* m_progressDialog = nullptr;

signals:
    void progressChanged(quint64 done, quint64 total);
};







