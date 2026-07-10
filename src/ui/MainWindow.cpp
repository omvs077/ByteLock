#include "ui/MainWindow.h"
#include "MasterConfig.h"
#include "SettingsDialog.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
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

QPushButton* makeListButton(QWidget* parent, QStyle::StandardPixmap icon, const QString& title)
{
    auto* btn = new QPushButton("   " + title, parent);
    QIcon ic = parent->style()->standardIcon(icon);
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
    if (!m_startupContainerPath.isEmpty()) showStartupUnlockPrompt();
    if (!lockFolderPath.isEmpty()) { lockContainer(lockFolderPath); close(); }
}

MainWindow::~MainWindow() = default;

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
    m_checkButton = makeListButton(central, QStyle::SP_MessageBoxInformation, "Check OpenSSL Link");
    m_selfTestButton = makeListButton(central, QStyle::SP_DialogApplyButton, "Run Crypto Self-Test");
    m_settingsButton = makeListButton(central, QStyle::SP_FileDialogDetailedView, "Settings...");
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

void MainWindow::lockContainer(const QString& folder)
{
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
    MasterConfig::trackLockedFolder(folder);
    SetFileAttributesW(containerPath.toStdWString().c_str(), FILE_ATTRIBUTE_HIDDEN);
    auto escrowKey = MasterConfig::getEscrowKey();
    if (!escrowKey.empty()) {
        std::vector<uint8_t> folderKeyBytes(keyResult.value().data(), keyResult.value().data() + keyResult.value().size());
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
    MasterConfig::untrackLockedFolder(destination);

    m_statusLabel->setText("Folder unlocked successfully.\nRestored to: " + destination);
}

void MainWindow::showStartupUnlockPrompt()
{
    QString blk = m_startupContainerPath;
    if (blk.endsWith(".blocked")) blk.chop(8);
    unlockContainer(blk + ".blk");
    close();
}

void MainWindow::onSettingsClicked()
{
    SettingsDialog dialog(this);
    dialog.exec();
}
