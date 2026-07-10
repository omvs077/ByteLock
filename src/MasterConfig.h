#pragma once
#include <QString>
#include <QByteArray>
#include "engine/SecureBytes.h"

namespace MasterConfig {
    bool exists();
    bool verify(const QString& password);
    void setup(const QString& password);
    bytelock::SecureBytes deriveMasterKey(const QString& password);
    QString configPath();
    bytelock::SecureBytes getEscrowKey();
    QByteArray wrapEscrowKeyWithRecoveryKey(const QString& recoveryKey);
}



