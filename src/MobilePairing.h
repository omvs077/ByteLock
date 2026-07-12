#pragma once
#include <QString>
#include <QByteArray>

namespace MobilePairing {
    bool isPaired();
    QString recoveryPageUrl();
    QByteArray generateUrlQrPng();
    void storePairedPhoneKey(const QByteArray& publicKeyBase64);
    QByteArray pairedPhonePublicKey();
    void clearPairing();
}