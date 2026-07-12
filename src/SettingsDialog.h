#pragma once
#include <QDialog>

class QLabel;
class QPushButton;
class QListWidget;
class QStackedWidget;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onRecoverFolderClicked();
    void onExportTokenClicked();
    void onPairMobileClicked();

private:
    QWidget* buildMasterRecoveryPage();
    QWidget* buildGeneralPage();
    QWidget* buildSecurityPage();
    QWidget* buildAboutPage();

    QLabel* m_statusLabel = nullptr;
    QPushButton* m_recoverButton = nullptr;
    QPushButton* m_exportButton = nullptr;
    QPushButton* m_pairButton = nullptr;
    QListWidget* m_sidebar = nullptr;
    QStackedWidget* m_stack = nullptr;
};
