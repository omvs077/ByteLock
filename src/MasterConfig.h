#pragma once
#include <QString>
#include <QByteArray>
#include <QStringList>
#include "engine/SecureBytes.h"

namespace MasterConfig {
    bool exists();
    bool verify(const QString& password);
    void setup(const QString& password);
    bytelock::SecureBytes deriveMasterKey(const QString& password);
    QString configPath();
    bytelock::SecureBytes getEscrowKey();
    QByteArray wrapEscrowKeyWithRecoveryKey(const QString& recoveryKey);
    void trackLockedFolder(const QString& folderPath);
    void untrackLockedFolder(const QString& folderPath);
    void repairOrphanedFolders();
    QStringList getLockedFolders();
    void recordFailedAttempt();
    void recordSuccessfulAttempt();
    qint64 secondsUntilNextAttempt();
}




