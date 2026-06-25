#include "viewmodel/MainViewModel.h"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

#include <openssl/opensslv.h>

#include "engine/CryptoEngine.h"
#include "engine/FolderPacker.h"

using namespace bytelock;

MainViewModel::MainViewModel(QObject* parent)
    : QObject(parent)
{
}

MainViewModel::~MainViewModel() = default;

QString MainViewModel::opensslVersionText() const
{
    return QString::fromUtf8(OPENSSL_VERSION_TEXT);
}

void MainViewModel::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged(m_busy);
    }
}

void MainViewModel::setStatusText(const QString& text)
{
    m_statusText = text;
    emit statusTextChanged(m_statusText);
}

void MainViewModel::requestLockFolder(const QString& folderPath, const QString& password, bool rememberPassword)
{
    if (rememberPassword) {
        m_sessionPassword = password;
    }
    runLock(folderPath, password);
}

void MainViewModel::requestLockFolderWithSessionPassword(const QString& folderPath)
{
    if (!m_sessionPassword.has_value()) {
        setStatusText("No session password is saved yet - enter one first.");
        emit lockFinished(false, m_statusText);
        return;
    }
    runLock(folderPath, *m_sessionPassword);
}

void MainViewModel::requestUnlockFolder(const QString& containerPath, const QString& password, bool rememberPassword)
{
    if (rememberPassword) {
        m_sessionPassword = password;
    }
    runUnlock(containerPath, password);
}

void MainViewModel::requestUnlockFolderWithSessionPassword(const QString& containerPath)
{
    if (!m_sessionPassword.has_value()) {
        setStatusText("No session password is saved yet - enter one first.");
        emit unlockFinished(false, m_statusText);
        return;
    }
    runUnlock(containerPath, *m_sessionPassword);
}

void MainViewModel::runLock(const QString& folderPath, const QString& password)
{
    if (m_busy) return;

    setBusy(true);
    setStatusText("Deriving key and locking folder, please wait...");

    std::string pwStd = password.toStdString();
    std::string folderStd = folderPath.toStdString();

    auto* watcher = new QFutureWatcher<OperationResult>(this);
    connect(watcher, &QFutureWatcher<OperationResult>::finished, this, [this, watcher]() {
        OperationResult result = watcher->result();
        watcher->deleteLater();
        setBusy(false);
        setStatusText(result.message);
        emit lockFinished(result.success, result.message);
    });

    QFuture<OperationResult> future = QtConcurrent::run([folderStd, pwStd]() -> OperationResult {
        auto saltResult = CryptoEngine::randomBytes(CryptoEngine::SaltSizeBytes);
        if (!saltResult) {
            return {false, QString::fromStdString(saltResult.errorMessage())};
        }

        auto keyResult = CryptoEngine::deriveKey(pwStd, saltResult.value());
        if (!keyResult) {
            return {false, QString::fromStdString(keyResult.errorMessage())};
        }

        std::string containerPath = folderStd + ".blk";
        auto lockResult = FolderPacker::lockFolder(folderStd, containerPath, keyResult.value(), saltResult.value());
        if (!lockResult) {
            return {false, QString::fromStdString(lockResult.errorMessage())};
        }

        return {true, QString("Folder locked successfully.\nContainer: %1").arg(QString::fromStdString(containerPath))};
    });

    watcher->setFuture(future);
}

void MainViewModel::runUnlock(const QString& containerPath, const QString& password)
{
    if (m_busy) return;

    setBusy(true);
    setStatusText("Decrypting and unlocking folder, please wait...");

    std::string pwStd = password.toStdString();
    std::string containerStd = containerPath.toStdString();

    QString destination = containerPath;
    if (destination.endsWith(".blk")) {
        destination.chop(4);
    }
    std::string destinationStd = destination.toStdString();

    auto* watcher = new QFutureWatcher<OperationResult>(this);
    connect(watcher, &QFutureWatcher<OperationResult>::finished, this, [this, watcher]() {
        OperationResult result = watcher->result();
        watcher->deleteLater();
        setBusy(false);
        setStatusText(result.message);
        emit unlockFinished(result.success, result.message);
    });

    QFuture<OperationResult> future = QtConcurrent::run([containerStd, destinationStd, pwStd]() -> OperationResult {
        auto saltResult = FolderPacker::peekContainerSalt(containerStd);
        if (!saltResult) {
            return {false, QString::fromStdString(saltResult.errorMessage())};
        }

        auto keyResult = CryptoEngine::deriveKey(pwStd, saltResult.value());
        if (!keyResult) {
            return {false, QString::fromStdString(keyResult.errorMessage())};
        }

        auto unlockResult = FolderPacker::unlockFolder(containerStd, destinationStd, keyResult.value());
        if (!unlockResult) {
            return {false, QString::fromStdString(unlockResult.errorMessage())};
        }

        return {true, QString("Folder unlocked successfully.\nRestored to: %1").arg(QString::fromStdString(destinationStd))};
    });

    watcher->setFuture(future);
}

void MainViewModel::requestRunCryptoSelfTest()
{
    if (m_busy) return;

    setBusy(true);
    setStatusText("Running crypto self-test...");

    auto* watcher = new QFutureWatcher<OperationResult>(this);
    connect(watcher, &QFutureWatcher<OperationResult>::finished, this, [this, watcher]() {
        OperationResult result = watcher->result();
        watcher->deleteLater();
        setBusy(false);
        setStatusText(result.message);
        emit selfTestFinished(result.success, result.message);
    });

    QFuture<OperationResult> future = QtConcurrent::run([]() -> OperationResult {
        QStringList log;

        auto saltResult = CryptoEngine::randomBytes(CryptoEngine::SaltSizeBytes);
        if (!saltResult) {
            return {false, "FAILED generating salt: " + QString::fromStdString(saltResult.errorMessage())};
        }

        auto keyResult = CryptoEngine::deriveKey("TestPassword123!", saltResult.value());
        if (!keyResult) {
            return {false, "FAILED deriving key: " + QString::fromStdString(keyResult.errorMessage())};
        }
        log << "[OK] Argon2id key derived successfully";

        const std::string original = "Hello, ByteLock! This proves AES-256-GCM round-trips correctly.";
        std::vector<uint8_t> plaintext(original.begin(), original.end());

        auto encryptResult = CryptoEngine::encryptBuffer(plaintext, keyResult.value());
        if (!encryptResult) {
            return {false, "FAILED encrypting: " + QString::fromStdString(encryptResult.errorMessage())};
        }
        log << QString("[OK] Encrypted %1 bytes -> %2 bytes").arg(plaintext.size()).arg(encryptResult.value().size());

        auto decryptResult = CryptoEngine::decryptBuffer(encryptResult.value(), keyResult.value());
        if (!decryptResult) {
            return {false, "FAILED decrypting: " + QString::fromStdString(decryptResult.errorMessage())};
        }

        std::string roundTrip(decryptResult.value().begin(), decryptResult.value().end());
        if (roundTrip != original) {
            log << "[FAIL] Round-trip mismatch!";
        } else {
            log << "[OK] Round-trip verified";
        }

        std::vector<uint8_t> tampered = encryptResult.value();
        tampered[CryptoEngine::IvSizeBytes] ^= 0xFF;
        auto tamperResult = CryptoEngine::decryptBuffer(tampered, keyResult.value());
        if (tamperResult.isOk()) {
            log << "[FAIL] Tampered data was NOT rejected!";
        } else if (tamperResult.error() == ErrorCode::AuthenticationFailed) {
            log << "[OK] Tampered ciphertext correctly rejected";
        } else {
            log << "[WARN] Unexpected error on tampered data";
        }

        return {true, log.join("\n")};
    });

    watcher->setFuture(future);
}
