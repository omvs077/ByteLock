#include "ui/MainWindow.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <openssl/opensslv.h>

#include "engine/CryptoEngine.h"
#include "engine/FolderPacker.h"
#include <windows.h>

using namespace bytelock;

MainWindow::MainWindow(const QString& startupContainerPath, QWidget* parent)
    : QMainWindow(parent)
    , m_startupContainerPath(startupContainerPath)
{
    setupUi();
    if (!m_startupContainerPath.isEmpty()) showStartupUnlockPrompt();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle("ByteLock - Scaffold");
    resize(560, 400);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    m_statusLabel = new QLabel("ByteLock scaffold running.", central);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);

    m_checkButton = new QPushButton("Check OpenSSL Link", central);
    m_selfTestButton = new QPushButton("Run Crypto Self-Test", central);
    m_lockFolderButton = new QPushButton("Lock a Folder...", central);
    m_unlockFolderButton = new QPushButton("Unlock a .blk Container...", central);

    layout->addStretch();
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_checkButton);
    layout->addWidget(m_selfTestButton);
    layout->addWidget(m_lockFolderButton);
    layout->addWidget(m_unlockFolderButton);
    layout->addStretch();

    setCentralWidget(central);

    connect(m_checkButton, &QPushButton::clicked, this, &MainWindow::onCheckOpenSslClicked);
    connect(m_selfTestButton, &QPushButton::clicked, this, &MainWindow::onRunCryptoSelfTestClicked);
    connect(m_lockFolderButton, &QPushButton::clicked, this, &MainWindow::onLockFolderClicked);
    connect(m_unlockFolderButton, &QPushButton::clicked, this, &MainWindow::onUnlockFolderClicked);
}

void MainWindow::onCheckOpenSslClicked()
{
    QString versionText = QString("OpenSSL linked successfully:\n%1").arg(OPENSSL_VERSION_TEXT);
    m_statusLabel->setText(versionText);
}

void MainWindow::onRunCryptoSelfTestClicked()
{
    QStringList log;

    auto saltResult = CryptoEngine::randomBytes(CryptoEngine::SaltSizeBytes);
    if (!saltResult) {
        m_statusLabel->setText("FAILED generating salt: " + QString::fromStdString(saltResult.errorMessage()));
        return;
    }

    QElapsedTimer timer;
    timer.start();
    auto keyResult = CryptoEngine::deriveKey("TestPassword123!", saltResult.value());
    qint64 kdfMs = timer.elapsed();

    if (!keyResult) {
        m_statusLabel->setText("FAILED deriving key: " + QString::fromStdString(keyResult.errorMessage()));
        return;
    }
    log << QString("[OK] Argon2id key derived in %1 ms").arg(kdfMs);

    const std::string original = "Hello, ByteLock! This proves AES-256-GCM round-trips correctly.";
    std::vector<uint8_t> plaintext(original.begin(), original.end());

    auto encryptResult = CryptoEngine::encryptBuffer(plaintext, keyResult.value());
    if (!encryptResult) {
        m_statusLabel->setText("FAILED encrypting: " + QString::fromStdString(encryptResult.errorMessage()));
        return;
    }
    log << QString("[OK] Encrypted %1 bytes -> %2 bytes (IV + ciphertext + tag)")
               .arg(plaintext.size())
               .arg(encryptResult.value().size());

    auto decryptResult = CryptoEngine::decryptBuffer(encryptResult.value(), keyResult.value());
    if (!decryptResult) {
        m_statusLabel->setText("FAILED decrypting: " + QString::fromStdString(decryptResult.errorMessage()));
        return;
    }

    std::string roundTrip(decryptResult.value().begin(), decryptResult.value().end());
    if (roundTrip != original) {
        log << "[FAIL] Round-trip mismatch! Decrypted text does not match original.";
    } else {
        log << "[OK] Round-trip verified: decrypted text matches original exactly";
    }

    std::vector<uint8_t> tampered = encryptResult.value();
    size_t tamperIndex = CryptoEngine::IvSizeBytes;
    tampered[tamperIndex] ^= 0xFF;

    auto tamperResult = CryptoEngine::decryptBuffer(tampered, keyResult.value());
    if (tamperResult.isOk()) {
        log << "[FAIL] Tampered data was NOT rejected - this should never happen!";
    } else if (tamperResult.error() == ErrorCode::AuthenticationFailed) {
        log << "[OK] Tampered ciphertext correctly rejected (AuthenticationFailed)";
    } else {
        log << QString("[WARN] Tampered data rejected, but with unexpected error: %1")
                   .arg(QString::fromStdString(tamperResult.errorMessage()));
    }

    m_statusLabel->setText(log.join("\n"));
}

void MainWindow::onLockFolderClicked()
{
    QString folder = QFileDialog::getExistingDirectory(this, "Select folder to lock");
    if (folder.isEmpty()) return;

    bool ok = false;
    QString password = QInputDialog::getText(this, "Set Password", "Enter a password to lock this folder:",
                                              QLineEdit::Password, "", &ok);
    if (!ok || password.isEmpty()) return;

    m_statusLabel->setText("Locking folder, please wait (this can take a moment)...");
    QCoreApplication::processEvents();

    auto saltResult = CryptoEngine::randomBytes(CryptoEngine::SaltSizeBytes);
    if (!saltResult) {
        m_statusLabel->setText("FAILED: " + QString::fromStdString(saltResult.errorMessage()));
        return;
    }

    auto keyResult = CryptoEngine::deriveKey(password.toStdString(), saltResult.value());
    if (!keyResult) {
        m_statusLabel->setText("FAILED: " + QString::fromStdString(keyResult.errorMessage()));
        return;
    }

    QString containerPath = folder + ".blk";
    auto lockResult = FolderPacker::lockFolder(folder.toStdString(), containerPath.toStdString(),
                                                keyResult.value(), saltResult.value());
    if (!lockResult) {
        m_statusLabel->setText("FAILED to lock folder: " + QString::fromStdString(lockResult.errorMessage()));
        return;
    }

    QFile(folder + ".blocked").open(QIODevice::WriteOnly);
    SetFileAttributesW(containerPath.toStdWString().c_str(), FILE_ATTRIBUTE_HIDDEN);
    m_statusLabel->setText("Folder locked successfully.\nContainer: " + containerPath);
}

void MainWindow::onUnlockFolderClicked()
{
    QString containerPath = QFileDialog::getOpenFileName(this, "Select .blk container to unlock", "", "ByteLock Container (*.blk)");
    if (containerPath.isEmpty()) return;
    unlockContainer(containerPath);
}

void MainWindow::unlockContainer(const QString& containerPath)
{
    auto saltResult = FolderPacker::peekContainerSalt(containerPath.toStdString());
    if (!saltResult) {
        m_statusLabel->setText("FAILED: " + QString::fromStdString(saltResult.errorMessage()));
        return;
    }

    bool ok = false;
    QString password = QInputDialog::getText(this, "Enter Password", "Enter the password for this folder:",
                                              QLineEdit::Password, "", &ok);
    if (!ok || password.isEmpty()) return;

    m_statusLabel->setText("Unlocking folder, please wait...");
    QCoreApplication::processEvents();

    auto keyResult = CryptoEngine::deriveKey(password.toStdString(), saltResult.value());
    if (!keyResult) {
        m_statusLabel->setText("FAILED: " + QString::fromStdString(keyResult.errorMessage()));
        return;
    }

    QString destination = containerPath;
    if (destination.endsWith(".blk")) {
        destination.chop(4);
    }

    auto unlockResult = FolderPacker::unlockFolder(containerPath.toStdString(), destination.toStdString(), keyResult.value());
    if (!unlockResult) {
        m_statusLabel->setText("FAILED to unlock: " + QString::fromStdString(unlockResult.errorMessage()));
        return;
    }

    QFile::remove(destination + ".blocked");

    m_statusLabel->setText("Folder unlocked successfully.\nRestored to: " + destination);
}

void MainWindow::showStartupUnlockPrompt()
{
    QString blk = m_startupContainerPath;
    if (blk.endsWith(".blocked")) blk.chop(8);
    unlockContainer(blk + ".blk");
    close();
}

