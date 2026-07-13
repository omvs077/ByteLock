#pragma once
#include <QString>
#include <QByteArray>

namespace MobilePairing {
    bool isPaired();
    QByteArray generateQrPng(const QString& text);
    void storePairedPhoneKey(const QByteArray& publicKeyBase64);
    QByteArray pairedPhonePublicKey();
    void clearPairing();
    bool verifySignature(const QByteArray& message, const QByteArray& signatureBase64, const QByteArray& publicKeyBase64);
}
