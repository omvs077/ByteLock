#pragma once
#include <QDialog>

class QLabel;
class QPushButton;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onRecoverFolderClicked();
    void onExportTokenClicked();

private:
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_recoverButton = nullptr;
    QPushButton* m_exportButton = nullptr;
};
