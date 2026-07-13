#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>

class QTcpServer;
class QTcpSocket;

class LocalPairingServer : public QObject
{
    Q_OBJECT
public:
    enum class Mode { Pairing, Recovery };

    explicit LocalPairingServer(Mode mode, QObject* parent = nullptr);
    bool start();
    void stop();
    QString pairingUrl() const;
    QString token() const { return m_token; }

signals:
    void phonePublicKeyReceived(const QByteArray& publicKeyBase64);
    void recoverySignatureReceived(const QByteArray& signatureBase64);
    void errorOccurred(const QString& message);

private slots:
    void onNewConnection();

private:
    void handleSocket(QTcpSocket* socket);
    QTcpServer* m_server;
    QString m_token;
    Mode m_mode;
    QString m_boundHost;
    bool m_tokenConsumed = false;
};
