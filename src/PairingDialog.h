#pragma once
#include <QDialog>

class QLabel;
class QPushButton;
class LocalPairingServer;

class PairingDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PairingDialog(QWidget* parent = nullptr);
    ~PairingDialog() override;

private slots:
    void onPhoneKeyReceived(const QByteArray& publicKeyBase64);
    void onServerError(const QString& message);
    void onTimeout();

private:
    void showWaitingState();
    void showSuccessState();
    void showErrorState(const QString& message);

    QLabel* m_qrLabel;
    QLabel* m_stepLabel;
    QLabel* m_statusPanel;
    QPushButton* m_closeButton;
    LocalPairingServer* m_server;
    bool m_paired;
};
