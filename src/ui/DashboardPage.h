#pragma once

#include <QWidget>

#include "persistence/FolderRegistry.h"

class QLabel;
class QPushButton;
class QTableWidget;
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
    void onLockFinished(bool success, const QString& message);
    void onUnlockFinished(bool success, const QString& message);
    void onBusyChanged(bool busy);
    void onStatusTextChanged(const QString& text);

private:
    void setupUi();
    void setButtonsEnabled(bool enabled);
    void refreshTable();
    void startLockFlow(const QString& folderPath);
    void startUnlockFlow(const QString& containerPath, bool usesCustomPassword);
    void showRowMenu(int row, const QPoint& globalPos);

    MainViewModel* m_viewModel = nullptr;
    FolderRegistry m_registry;

    QLabel* m_statusLabel = nullptr;
    QPushButton* m_checkButton = nullptr;
    QPushButton* m_selfTestButton = nullptr;
    QPushButton* m_lockFolderCard = nullptr;
    QPushButton* m_unlockFolderCard = nullptr;
    QTableWidget* m_table = nullptr;

    QString m_pendingFolderPath;
    bool m_pendingIsCustom = false;
    QString m_pendingContainerPath;
};
