#include "LocalPairingServer.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkInterface>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

LocalPairingServer::LocalPairingServer(Mode mode, QObject* parent)
    : QObject(parent), m_server(new QTcpServer(this)), m_mode(mode)
{
    connect(m_server, &QTcpServer::newConnection, this, &LocalPairingServer::onNewConnection);
}

bool LocalPairingServer::start()
{
    m_token = QUuid::createUuid().toString(QUuid::Id128);
    if (!m_server->listen(QHostAddress::AnyIPv4, 47891)) {
        emit errorOccurred("Could not start local server: " + m_server->errorString());
        return false;
    }
    return true;
}

void LocalPairingServer::stop()
{
    m_server->close();
}

QString LocalPairingServer::pairingUrl() const
{
    QString ip;
    for (const auto& addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            ip = addr.toString();
            break;
        }
    }
    if (ip.isEmpty()) return QString();
    QString path = (m_mode == Mode::Pairing) ? "/pair" : "/recover";
    return QString("http://%1:%2%3?token=%4").arg(ip).arg(m_server->serverPort()).arg(path, m_token);
}

void LocalPairingServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        handleSocket(m_server->nextPendingConnection());
    }
}

void LocalPairingServer::handleSocket(QTcpSocket* socket)
{
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        if (!socket->canReadLine()) return;
        QByteArray requestLine = socket->readLine();
        bool isPost = requestLine.startsWith("POST");
        QList<QByteArray> parts = requestLine.split(' ');
        QString path = parts.size() > 1 ? QString::fromUtf8(parts[1]) : "/";
        int qIdx = path.indexOf('?');
        if (qIdx >= 0) path = path.left(qIdx);

        int contentLength = 0;
        while (socket->canReadLine()) {
            QByteArray line = socket->readLine();
            if (line.startsWith("Content-Length:")) {
                contentLength = line.mid(15).trimmed().toInt();
            }
            if (line == "\r\n") break;
        }

        QByteArray responseBody;
        QByteArray contentType = "text/html";

        if (isPost) {
            while (socket->bytesAvailable() < contentLength) socket->waitForReadyRead(2000);
            QByteArray body = socket->read(contentLength);
            QJsonObject obj = QJsonDocument::fromJson(body).object();
            QString reqToken = obj["token"].toString();
            bool valid = (reqToken == m_token);

            if (valid && m_mode == Mode::Pairing) {
                QString pubKey = obj["publicKey"].toString();
                if (!pubKey.isEmpty()) {
                    emit phonePublicKeyReceived(pubKey.toUtf8());
                    responseBody = "{\"ok\":true}";
                } else {
                    responseBody = "{\"ok\":false}";
                }
            } else if (valid && m_mode == Mode::Recovery) {
                QString signature = obj["signature"].toString();
                if (!signature.isEmpty()) {
                    emit recoverySignatureReceived(signature.toUtf8());
                    responseBody = "{\"ok\":true}";
                } else {
                    responseBody = "{\"ok\":false}";
                }
            } else {
                responseBody = "{\"ok\":false}";
            }
            contentType = "application/json";
        } else if (path == "/nacl.min.js") {
            QFile js(":/pairing/nacl.min.js");
            if (js.open(QIODevice::ReadOnly)) responseBody = js.readAll();
            contentType = "application/javascript";
        } else if (m_mode == Mode::Recovery) {
            QFile page(":/pairing/recover.html");
            if (page.open(QIODevice::ReadOnly)) {
                responseBody = page.readAll();
            } else {
                responseBody = "<html><body>Page not found</body></html>";
            }
        } else {
            QFile page(":/pairing/pair.html");
            if (page.open(QIODevice::ReadOnly)) {
                responseBody = page.readAll();
            } else {
                responseBody = "<html><body>Page not found</body></html>";
            }
        }

        QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: " + contentType +
            "\r\nContent-Length: " + QByteArray::number(responseBody.size()) +
            "\r\nConnection: close\r\n\r\n" + responseBody;
        socket->write(response);
        socket->disconnectFromHost();
    });
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}
