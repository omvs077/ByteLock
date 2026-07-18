#include "ui/MainWindow.h"
#include "MasterConfig.h"
#include "SettingsDialog.h"
#include "PasswordPromptDialog.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QWidget>
#include <QStyle>
#include <QFont>
#include <QIcon>
#include <QMessageBox>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#include <openssl/opensslv.h>

#include "engine/CryptoEngine.h"
#include "engine/FolderPacker.h"
#include <windows.h>

using namespace bytelock;

namespace {

QPushButton* makeCardButton(QWidget* parent, QStyle::StandardPixmap icon, const QString& title, const QString& subtitle)
{
    auto* btn = new QPushButton(QString("  %1\n  %2").arg(title, subtitle), parent);
    QIcon ic = parent->style()->standardIcon(icon);
    btn->setIcon(ic);
    btn->setIconSize(ic.actualSize(QSize(48, 48)));
    btn->setMinimumHeight(72);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton {"
        "  text-align: left;"
        "  padding: 12px 16px;"
        "  border: 1px solid #d7dae0;"
        "  border-radius: 8px;"
        "  background-color: #f7f9fc;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover { background-color: #eaf1ff; border-color: #7aa7ff; }"
        "QPushButton:pressed { background-color: #dbe7ff; }"
        "QPushButton:focus { outline: none; }"
    );
    return btn;
}

QPushButton* makeListButton(QWidget* parent, const QString& iconPath, const QString& title)
{
    auto* btn = new QPushButton("   " + title, parent);
    QIcon ic(iconPath);
    btn->setIcon(ic);
    btn->setIconSize(ic.actualSize(QSize(24, 24)));
    btn->setMinimumHeight(46);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton {"
        "  text-align: left;"
        "  padding: 6px 12px;"
        "  border: 1px solid #e2e5ea;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #f2f5fa; }"
        "QPushButton:pressed { background-color: #e6eaf0; }"
        "QPushButton:focus { outline: none; }"
    );
    return btn;
}

} // namespace

MainWindow::MainWindow(const QString& startupContainerPath, QWidget* parent, const QString& lockFolderPath)
    : QMainWindow(parent)
    , m_startupContainerPath(startupContainerPath)
{
    setupUi();
    connect(this, &MainWindow::progressChanged, this, &MainWindow::onProgressChanged);
    if (!m_startupContainerPath.isEmpty()) showStartupUnlockPrompt();
    if (!lockFolderPath.isEmpty()) { lockContainer(lockFolderPath, true); }
}

MainWindow::~MainWindow() = default;

void MainWindow::onProgressChanged(quint64 done, quint64 total)
{
    if (!m_progressDialog || total == 0) return;
    m_progressDialog->setValue(static_cast<int>((done * 100) / total));
}

void MainWindow::setupUi()
{
    setWindowTitle("ByteLock");
    setWindowIcon(QIcon(":/icons/app.ico"));
    resize(640, 560);
    setStyleSheet("QMainWindow { background-color: #ffffff; }");

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(28, 24, 28, 20);
    layout->setSpacing(16);

    auto* headerRow = new QHBoxLayout();
    auto* iconLabel = new QLabel(central);
    iconLabel->setPixmap(QIcon(":/icons/app.ico").pixmap(44, 44));
    auto* titleBlock = new QVBoxLayout();
    auto* titleLabel = new QLabel("<span style='font-size:21px; font-weight:600;'>ByteLock</span>", central);
    auto* taglineLabel = new QLabel("Your digital safe", central);
    taglineLabel->setStyleSheet("color: #6b7280; font-size: 12px;");
    titleBlock->addWidget(titleLabel);
    titleBlock->addWidget(taglineLabel);
    titleBlock->setSpacing(2);
    headerRow->addWidget(iconLabel);
    headerRow->addSpacing(12);
    headerRow->addLayout(titleBlock);
    headerRow->addStretch();
    layout->addLayout(headerRow);

    m_statusLabel = new QLabel("Ready.", central);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(
        "background-color: #f4f6f9; border: 1px solid #e2e5ea; border-radius: 6px; padding: 12px; font-size: 12px;");
    layout->addWidget(m_statusLabel);

    auto* quickLabel = new QLabel("Quick Actions", central);
    quickLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #374151; margin-top: 6px;");
    layout->addWidget(quickLabel);

    auto* grid = new QGridLayout();
    grid->setSpacing(12);
    m_lockFolderButton = makeCardButton(central, QStyle::SP_DialogSaveButton, "Lock a Folder", "Protect your data");
    m_unlockFolderButton = makeCardButton(central, QStyle::SP_DialogOpenButton, "Unlock a .blk Container", "Access a locked container file");
    grid->addWidget(m_lockFolderButton, 0, 0);
    grid->addWidget(m_unlockFolderButton, 0, 1);
    layout->addLayout(grid);

    auto* toolsLabel = new QLabel("Other Tools", central);
    toolsLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #374151; margin-top: 10px;");
    layout->addWidget(toolsLabel);

    auto* toolsList = new QVBoxLayout();
    toolsList->setSpacing(8);
    m_checkButton = makeListButton(central, ":/icons/ui/link.svg", "Check OpenSSL Link");
    m_selfTestButton = makeListButton(central, ":/icons/ui/flask.svg", "Run Crypto Self-Test");
    m_settingsButton = makeListButton(central, ":/icons/ui/gear.svg", "Settings...");
    toolsList->addWidget(m_checkButton);
    toolsList->addWidget(m_selfTestButton);
    toolsList->addWidget(m_settingsButton);
    layout->addLayout(toolsList);

    layout->addStretch();

    auto* watermark = new QLabel(
        "<div style='font-size:11px;'>"
        "Crafted by <b>Dvvyom</b> &nbsp;|&nbsp; "
        "<a href='https://github.com/omvs077/ByteLock' style='color:#2f6fed;'>GitHub</a> &nbsp;|&nbsp; "
        "<span style='color:#2f6fed;'>omvs077@gmail.com</span>"
        "</div>", central);
    watermark->setOpenExternalLinks(true);
    watermark->setAlignment(Qt::AlignCenter);
    watermark->setStyleSheet("color: #374151; padding-top: 4px;");
    layout->addWidget(watermark);

    setCentralWidget(central);

    connect(m_checkButton, &QPushButton::clicked, this, &MainWindow::onCheckOpenSslClicked);
    connect(m_selfTestButton, &QPushButton::clicked, this, &MainWindow::onRunCryptoSelfTestClicked);
    connect(m_lockFolderButton, &QPushButton::clicked, this, &MainWindow::onLockFolderClicked);
    connect(m_unlockFolderButton, &QPushButton::clicked, this, &MainWindow::onUnlockFolderClicked);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
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
    lockContainer(folder);
}

void MainWindow::lockContainer(const QString& folder, bool closeWhenDone)
{
    PasswordPromptDialog dlg;
    dlg.setWindowTitle("Set Password");
    dlg.setLabelText("Enter a password to lock this folder:");
    dlg.setMaxLength(128);
    bool ok = (dlg.exec() == QDialog::Accepted);
    QString password = dlg.textValue();
    if (!ok || password.isEmpty()) return;

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
    auto keyPtr = std::make_shared<SecureBytes>(std::move(keyResult.value()));
    std::vector<uint8_t> salt = saltResult.value();

    m_statusLabel->setText("Locking folder, please wait...");
    m_progressDialog = new QProgressDialog("Locking folder...", QString(), 0, 100, this);
    m_progressDialog->setWindowModality(Qt::WindowModal);
    m_progressDialog->setCancelButton(nullptr);
    m_progressDialog->setMinimumDuration(0);
    m_progressDialog->setValue(0);

    auto* watcher = new QFutureWatcher<Result<void>>(this);
    connect(watcher, &QFutureWatcher<Result<void>>::finished, this, [this, watcher, folder, containerPath, keyPtr, closeWhenDone]() {
        auto lockResult = watcher->result();
        m_progressDialog->close();
        m_progressDialog->deleteLater();
        m_progressDialog = nullptr;
        watcher->deleteLater();

        if (!lockResult) {
            m_statusLabel->setText("FAILED to lock folder: " + QString::fromStdString(lockResult.errorMessage()));
            if (closeWhenDone) close();
            return;
        }

        bool blockedOk = QFile(folder + ".blocked").open(QIODevice::WriteOnly);
        MasterConfig::trackLockedFolder(folder);
        bool hiddenOk = SetFileAttributesW(containerPath.toStdWString().c_str(), FILE_ATTRIBUTE_HIDDEN);
        if (!blockedOk || !hiddenOk) {
            QMessageBox::warning(this, "Lock Warning",
                QString("Folder locked, but a post-step failed (blockedOk=%1, hiddenOk=%2, lastError=%3). Recovery via .blk_recovery still works.")
                    .arg(blockedOk).arg(hiddenOk).arg(GetLastError()));
        }
        auto escrowKey = MasterConfig::getEscrowKey();
        if (!escrowKey.empty()) {
            std::vector<uint8_t> folderKeyBytes(keyPtr->data(), keyPtr->data() + keyPtr->size());
            auto wrapped = CryptoEngine::encryptBuffer(folderKeyBytes, escrowKey);
            if (wrapped) {
                QString sidecarPath = folder + ".blk_recovery";
                QFile sidecar(sidecarPath);
                if (sidecar.open(QIODevice::WriteOnly)) {
                    sidecar.write(reinterpret_cast<const char*>(wrapped.value().data()), static_cast<qint64>(wrapped.value().size()));
                    sidecar.close();
                    SetFileAttributesW(sidecarPath.toStdWString().c_str(), FILE_ATTRIBUTE_HIDDEN);
                }
            }
        }
        m_statusLabel->setText("Folder locked successfully.\nContainer: " + containerPath);
        if (closeWhenDone) close();
    });

    QFuture<Result<void>> future = QtConcurrent::run([this, folder, containerPath, keyPtr, salt]() {
        return FolderPacker::lockFolder(folder.toStdString(), containerPath.toStdString(), *keyPtr, salt,
                                         FolderPacker::DefaultStreamingThresholdBytes,
                                         [this](uint64_t done, uint64_t total) {
                                             emit progressChanged(done, total);
                                         });
    });
    watcher->setFuture(future);
}

void MainWindow::onUnlockFolderClicked()
{
    QString containerPath = QFileDialog::getOpenFileName(this, "Select container to unlock", "", "ByteLock Container (*.blk *.blocked)");
    if (containerPath.isEmpty()) return;
    if (containerPath.endsWith(".blocked")) {
        containerPath.chop(8);
        containerPath += ".blk";
    }
    unlockContainer(containerPath);
}

void MainWindow::unlockContainer(const QString& containerPath)
{
    unlockContainerAttempt(containerPath, QString());
}

void MainWindow::unlockContainerAttempt(const QString& containerPath, const QString& warningText)
{
    qint64 waitSecs = MasterConfig::secondsUntilNextAttempt();
    if (waitSecs > 0) {
        QMessageBox::warning(this, "Too Many Attempts", QString("Too many failed attempts. Try again in %1 seconds.").arg(waitSecs));
        return;
    }

    auto saltResult = FolderPacker::peekContainerSalt(containerPath.toStdString());
    if (!saltResult) {
        QMessageBox::warning(this, "Unlock Failed", "FAILED: " + QString::fromStdString(saltResult.errorMessage()));
        return;
    }

    PasswordPromptDialog dlg(this);
    dlg.setWindowTitle("Enter Password");
    QString label = "Enter the password for this folder:";
    if (!warningText.isEmpty()) {
        label = "<span style='color:#dc2626; font-weight:600;'>" + warningText + "</span><br>" + label;
    }
    dlg.setLabelText(label);
    dlg.setMaxLength(128);
    if (dlg.exec() != QDialog::Accepted) return;
    QString password = dlg.textValue();
    if (password.isEmpty()) return;

    auto keyResult = CryptoEngine::deriveKey(password.toStdString(), saltResult.value());
    if (!keyResult) {
        QMessageBox::warning(this, "Unlock Failed", "FAILED: " + QString::fromStdString(keyResult.errorMessage()));
        return;
    }

    QString destination = containerPath;
    if (destination.endsWith(".blk")) {
        destination.chop(4);
    }

    auto keyPtr = std::make_shared<SecureBytes>(std::move(keyResult.value()));

    m_statusLabel->setText("Unlocking folder, please wait...");
    m_progressDialog = new QProgressDialog("Unlocking folder...", QString(), 0, 100, this);
    m_progressDialog->setWindowModality(Qt::WindowModal);
    m_progressDialog->setCancelButton(nullptr);
    m_progressDialog->setMinimumDuration(0);
    m_progressDialog->setValue(0);

    auto* watcher = new QFutureWatcher<Result<void>>(this);
    connect(watcher, &QFutureWatcher<Result<void>>::finished, this, [this, watcher, containerPath, destination]() {
        auto unlockResult = watcher->result();
        m_progressDialog->close();
        m_progressDialog->deleteLater();
        m_progressDialog = nullptr;
        watcher->deleteLater();

        if (!unlockResult) {
            m_statusLabel->setText("Ready.");
            if (unlockResult.error() == ErrorCode::AuthenticationFailed) {
                MasterConfig::recordFailedAttempt();
                unlockContainerAttempt(containerPath, "Wrong password. Please try again.");
            } else {
                QMessageBox::warning(this, "Unlock Failed", "FAILED to unlock: " + QString::fromStdString(unlockResult.errorMessage()));
            }
            return;
        }

        MasterConfig::recordSuccessfulAttempt();
        QFile::remove(destination + ".blocked");
        MasterConfig::untrackLockedFolder(destination);

        m_statusLabel->setText("Folder unlocked successfully.\nRestored to: " + destination);
    });

    QFuture<Result<void>> future = QtConcurrent::run([this, containerPath, destination, keyPtr]() {
        return FolderPacker::unlockFolder(containerPath.toStdString(), destination.toStdString(), *keyPtr,
                                           [this](uint64_t done, uint64_t total) {
                                               emit progressChanged(done, total);
                                           });
    });
    watcher->setFuture(future);
}

void MainWindow::unlockContainerSilent(const QString& containerPath)
{
    QString warningText;
    while (true) {
        qint64 waitSecs = MasterConfig::secondsUntilNextAttempt();
        if (waitSecs > 0) {
            QMessageBox::warning(this, "Too Many Attempts", QString("Too many failed attempts. Try again in %1 seconds.").arg(waitSecs));
            return;
        }

        auto saltResult = FolderPacker::peekContainerSalt(containerPath.toStdString());
        if (!saltResult) return;

        PasswordPromptDialog dlg(this);
        dlg.setWindowTitle("Enter Password");
        QString label = "Enter the password for this folder:";
        if (!warningText.isEmpty()) {
            label = "<span style='color:#dc2626; font-weight:600;'>" + warningText + "</span><br>" + label;
        }
        dlg.setLabelText(label);
        dlg.setMaxLength(128);
        if (dlg.exec() != QDialog::Accepted) return;
        QString password = dlg.textValue();
        if (password.isEmpty()) return;

        auto keyResult = CryptoEngine::deriveKey(password.toStdString(), saltResult.value());
        if (!keyResult) return;

        QString destination = containerPath;
        if (destination.endsWith(".blk")) {
            destination.chop(4);
        }

        auto unlockResult = FolderPacker::unlockFolder(containerPath.toStdString(), destination.toStdString(), keyResult.value());
        if (!unlockResult) {
            if (unlockResult.error() == ErrorCode::AuthenticationFailed) {
                MasterConfig::recordFailedAttempt();
                warningText = "Wrong password. Please try again.";
                continue;
            }
            return;
        }

        MasterConfig::recordSuccessfulAttempt();
        QFile::remove(destination + ".blocked");
        MasterConfig::untrackLockedFolder(destination);
        return;
    }
}
void MainWindow::showStartupUnlockPrompt()
{
    QString blk = m_startupContainerPath;
    if (blk.endsWith(".blocked")) blk.chop(8);
    unlockContainerSilent(blk + ".blk");
    close();
}
void MainWindow::onSettingsClicked()
{
    SettingsDialog dialog(this);
    dialog.exec();
}



