#pragma once
#include <QDialog>
#include <QByteArray>

class QLabel;
class QPushButton;
class LocalPairingServer;

class RecoveryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RecoveryDialog(QWidget* parent = nullptr);
    ~RecoveryDialog() override;

    bool wasVerified() const { return m_verified; }

private slots:
    void onSignatureReceived(const QByteArray& signatureBase64);
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
    bool m_verified;
};
