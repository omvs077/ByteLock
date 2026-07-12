#pragma once
#include <QString>
#include <QByteArray>

namespace MobilePairing {
    bool isPaired();
    QByteArray generateQrPng(const QString& text);
    void storePairedPhoneKey(const QByteArray& publicKeyBase64);
    QByteArray pairedPhonePublicKey();
    void clearPairing();
}
