#include "SettingsDialog.h"
#include "MasterConfig.h"
#include "PairingDialog.h"
#include "engine/CryptoEngine.h"
#include "engine/FolderPacker.h"
#include "engine/SecureBytes.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QListWidget>
#include <QStackedWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QFrame>
#include <QIcon>
#include <QStyle>

using namespace bytelock;

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("ByteLock — Settings");
    setWindowIcon(QIcon(":/icons/app.ico"));
    setModal(true);
    resize(620, 440);

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_sidebar = new QListWidget(this);
    m_sidebar->setFixedWidth(170);
    m_sidebar->setIconSize(QSize(20, 20));

    auto addItem = [&](const QString& iconPath, const QString& text) {
        auto* item = new QListWidgetItem(QIcon(iconPath), text);
        m_sidebar->addItem(item);
    };
    addItem(":/icons/ui/key.svg", "Master Recovery");
    addItem(":/icons/ui/sliders.svg", "General");
    addItem(":/icons/ui/shield.svg", "Security");
    addItem(":/icons/ui/info.svg", "About");
    m_sidebar->setCurrentRow(0);
    m_sidebar->setStyleSheet(
        "QListWidget { border: none; background-color: #f4f6f9; font-size: 13px; outline: none; }"
        "QListWidget::item { padding: 12px 14px; }"
        "QListWidget::item:selected { background-color: #dbe7ff; color: #1a3b8f; border-radius: 6px; }"
        "QListWidget::item:focus { outline: none; border: none; }"
    );

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildMasterRecoveryPage());
    m_stack->addWidget(buildGeneralPage());
    m_stack->addWidget(buildSecurityPage());
    m_stack->addWidget(buildAboutPage());

    connect(m_sidebar, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);

    outer->addWidget(m_sidebar);
    outer->addWidget(m_stack);
}

QWidget* SettingsDialog::buildMasterRecoveryPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(14);

    auto* heading = new QLabel("Master Recovery Settings", page);
    heading->setStyleSheet("font-weight: 600; font-size: 16px;");
    layout->addWidget(heading);

    auto* desc = new QLabel("Set up and manage your Master Recovery Key. This key is the only way to recover your data if you forget a folder's password.", page);
    desc->setWordWrap(true);
    desc->setStyleSheet("color: #6b7280; font-size: 12px; line-height: 140%;");
    layout->addWidget(desc);

    layout->addSpacing(8);

    m_recoverButton = new QPushButton("   Recover a Folder...", page);
    m_recoverButton->setIcon(QIcon(":/icons/ui/key.svg"));
    m_recoverButton->setIconSize(QSize(20, 20));
    m_recoverButton->setMinimumHeight(44);
    m_recoverButton->setCursor(Qt::PointingHandCursor);
    m_recoverButton->setAutoDefault(false);
    m_recoverButton->setDefault(false);
    m_recoverButton->setStyleSheet(
        "QPushButton { text-align: left; padding: 8px 14px; border: 1px solid #d7dae0; border-radius: 6px; background-color: #f7f9fc; font-size: 12px; color: #1f2937; }"
        "QPushButton:hover { background-color: #eaf1ff; color: #1f2937; }"
        "QPushButton:pressed { background-color: #dbe7ff; color: #1f2937; }"
        "QPushButton:focus { outline: none; color: #1f2937; }");

    m_exportButton = new QPushButton("   Export Recovery Token...", page);
    m_exportButton->setIcon(QIcon(":/icons/ui/download.svg"));
    m_exportButton->setIconSize(QSize(20, 20));
    m_exportButton->setMinimumHeight(44);
    m_exportButton->setCursor(Qt::PointingHandCursor);
    m_exportButton->setAutoDefault(false);
    m_exportButton->setDefault(false);
    m_exportButton->setStyleSheet(
        "QPushButton { text-align: left; padding: 8px 14px; border: 1px solid #d7dae0; border-radius: 6px; background-color: #f7f9fc; font-size: 12px; color: #1f2937; }"
        "QPushButton:hover { background-color: #eaf1ff; color: #1f2937; }"
        "QPushButton:pressed { background-color: #dbe7ff; color: #1f2937; }"
        "QPushButton:focus { outline: none; color: #1f2937; }");

    layout->addWidget(m_recoverButton);
    layout->addWidget(m_exportButton);

    m_pairButton = new QPushButton("   Pair Mobile Device...", page);
    m_pairButton->setIcon(QIcon(":/icons/ui/key.svg"));
    m_pairButton->setIconSize(QSize(20, 20));
    m_pairButton->setMinimumHeight(44);
    m_pairButton->setCursor(Qt::PointingHandCursor);
    m_pairButton->setAutoDefault(false);
    m_pairButton->setDefault(false);
    m_pairButton->setStyleSheet(
        "QPushButton { text-align: left; padding: 8px 14px; border: 1px solid #d7dae0; border-radius: 6px; background-color: #f7f9fc; font-size: 12px; color: #1f2937; }"
        "QPushButton:hover { background-color: #eaf1ff; color: #1f2937; }"
        "QPushButton:pressed { background-color: #dbe7ff; color: #1f2937; }"
        "QPushButton:focus { outline: none; color: #1f2937; }");
    layout->addWidget(m_pairButton);

    m_statusLabel = new QLabel("", page);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("color: #374151; font-size: 12px; margin-top: 10px;");
    layout->addWidget(m_statusLabel);

    layout->addStretch();

    connect(m_recoverButton, &QPushButton::clicked, this, &SettingsDialog::onRecoverFolderClicked);
    connect(m_exportButton, &QPushButton::clicked, this, &SettingsDialog::onExportTokenClicked);
    connect(m_pairButton, &QPushButton::clicked, this, &SettingsDialog::onPairMobileClicked);

    return page;
}

QWidget* SettingsDialog::buildGeneralPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(16);

    auto* heading = new QLabel("General Settings", page);
    heading->setStyleSheet("font-weight: 600; font-size: 16px;");
    layout->addWidget(heading);

    auto* startupCheck = new QCheckBox("Launch ByteLock on Windows startup", page);
    QSettings runKey("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    startupCheck->setChecked(runKey.contains("ByteLock"));
    connect(startupCheck, &QCheckBox::toggled, this, [](bool checked) {
        QSettings key("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        if (checked) {
            key.setValue("ByteLock", QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
        } else {
            key.remove("ByteLock");
        }
    });
    layout->addWidget(startupCheck);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::buildSecurityPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(14);

    auto* heading = new QLabel("Security", page);
    heading->setStyleSheet("font-weight: 600; font-size: 16px;");
    layout->addWidget(heading);

    auto* infoFrame = new QFrame(page);
    infoFrame->setStyleSheet("background-color: #f4f6f9; border: 1px solid #e2e5ea; border-radius: 8px;");
    auto* infoLayout = new QVBoxLayout(infoFrame);
    infoLayout->setContentsMargins(18, 16, 18, 16);
    infoLayout->setSpacing(12);

    auto* specHeading = new QLabel("Engine Specifications", infoFrame);
    specHeading->setStyleSheet("font-weight: 600; font-size: 13px;");
    infoLayout->addWidget(specHeading);

    auto addSpec = [&](const QString& text) {
        auto* label = new QLabel(text, infoFrame);
        label->setStyleSheet("font-size: 12px; color: #374151;");
        label->setWordWrap(true);
        infoLayout->addWidget(label);
    };
    addSpec("• Encryption Standard: AES-256-GCM (Authenticated Encryption)");
    addSpec("• Key Derivation: Argon2id (memory: 64MB, iterations: 3, parallelism: 4)");
    addSpec("• Integrity Validation: Constant-time verification (OpenSSL CRYPTO_memcmp)");
    addSpec("• Escrow Protection: Local Master Recovery key wrapped via Windows DPAPI");

    layout->addWidget(infoFrame);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::buildAboutPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignTop);

    auto* nameLabel = new QLabel("ByteLock — Secure Local Vaults", page);
    nameLabel->setStyleSheet("font-weight: 600; font-size: 16px;");
    layout->addWidget(nameLabel);

    auto* versionLabel = new QLabel("v1.0.0 (Release Build)", page);
    versionLabel->setStyleSheet("color: #6b7280; font-size: 12px;");
    layout->addWidget(versionLabel);

    auto* descLabel = new QLabel("A zero-knowledge, local-first folder encryption tool powered by Qt6, OpenSSL, and WinFsp.", page);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("font-size: 12px; margin-top: 8px;");
    layout->addWidget(descLabel);

    auto* pathLabel = new QLabel(
        "<div style='margin-top:14px; font-size:12px; color:#4b5563;'>"
        "<b>Install location:</b> " + QCoreApplication::applicationDirPath() + "<br>"
        "<b>Uninstall:</b> Start Menu &gt; ByteLock &gt; Uninstall, or run Uninstall.exe in the install folder "
        "(requires your Master Recovery Key)."
        "</div>", page);
    pathLabel->setWordWrap(true);
    pathLabel->setTextFormat(Qt::RichText);
    layout->addWidget(pathLabel);

    auto* watermark = new QLabel(
        "<div style='margin-top:18px; font-size:12px;'>"
        "Crafted by <b>Dvvyom</b><br><br>"
        "<a href='https://github.com/omvs077/ByteLock' style='color:#2f6fed;'>GitHub</a> &nbsp;|&nbsp; "
        "<span style='color:#2f6fed;'>omvs077@gmail.com</span>"
        "</div>", page);
    watermark->setOpenExternalLinks(true);
    layout->addWidget(watermark);

    layout->addStretch();
    return page;
}

void SettingsDialog::onRecoverFolderClicked()
{
    QString blockedPath = QFileDialog::getOpenFileName(this, "Select the locked folder",
                                                         "", "ByteLock Locked Folder (*.blocked)");
    if (blockedPath.isEmpty()) return;

    QString sidecarPath = blockedPath;
    sidecarPath.replace(".blocked", ".blk_recovery");

    bool ok = false;
    QString recoveryKey = QInputDialog::getText(this, "Master Recovery", "Enter your Master Recovery Key:",
                                                 QLineEdit::Password, "", &ok);
    if (!ok || recoveryKey.isEmpty()) return;

    if (!MasterConfig::verify(recoveryKey)) {
        m_statusLabel->setText("FAILED: Incorrect Master Recovery Key.");
        return;
    }

    auto escrowKey = MasterConfig::getEscrowKey();
    if (escrowKey.empty()) {
        m_statusLabel->setText("FAILED: Escrow key unavailable on this machine.");
        return;
    }

    QFile sidecar(sidecarPath);
    if (!sidecar.open(QIODevice::ReadOnly)) {
        m_statusLabel->setText("FAILED: Could not read sidecar file.");
        return;
    }
    QByteArray wrapped = sidecar.readAll();
    std::vector<uint8_t> wrappedBytes(wrapped.begin(), wrapped.end());

    auto unwrapped = CryptoEngine::decryptBuffer(wrappedBytes, escrowKey);
    if (!unwrapped) {
        m_statusLabel->setText("FAILED: " + QString::fromStdString(unwrapped.errorMessage()));
        return;
    }

    SecureBytes folderKey(unwrapped.value().size());
    memcpy(folderKey.data(), unwrapped.value().data(), unwrapped.value().size());

    QString containerPath = sidecarPath;
    containerPath.replace(".blk_recovery", ".blk");
    QString destination = containerPath;
    if (destination.endsWith(".blk")) destination.chop(4);

    auto unlockResult = FolderPacker::unlockFolder(containerPath.toStdString(), destination.toStdString(), folderKey);
    if (!unlockResult) {
        m_statusLabel->setText("FAILED to unlock via recovery: " + QString::fromStdString(unlockResult.errorMessage()));
        return;
    }

    QFile::remove(QString(containerPath).replace(".blk", ".blocked"));
    MasterConfig::untrackLockedFolder(destination);
    m_statusLabel->setText("Folder recovered successfully via Master Recovery.");
}

void SettingsDialog::onExportTokenClicked()
{
    bool ok = false;
    QString recoveryKey = QInputDialog::getText(this, "Master Recovery", "Re-enter your Master Recovery Key to export a token:",
                                                 QLineEdit::Password, "", &ok);
    if (!ok || recoveryKey.isEmpty()) return;

    if (!MasterConfig::verify(recoveryKey)) {
        m_statusLabel->setText("FAILED: Incorrect Master Recovery Key.");
        return;
    }

    QString dir = QFileDialog::getExistingDirectory(this, "Choose folder to save recovery files");
    if (dir.isEmpty()) return;

    QMessageBox::warning(this, "Security Warning",
        "These files, combined together, can recover ANY locked folder.\n"
        "Never store them on this computer. Keep them on a USB drive or other secure offline location.");

    QFile keyFile(dir + "/bytelock_master_key.txt");
    if (keyFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&keyFile);
        out << "ByteLock Master Recovery Key\n";
        out << "Generated once. Never stored by ByteLock. Keep this file safe.\n\n";
        out << recoveryKey << "\n";
    } else {
        QMessageBox::warning(this, "Save Failed", "Could not write the master key file.");
        return;
    }

    QFile escrowFile(dir + "/bytelock_escrow_data.txt");
    if (escrowFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&escrowFile);
        out << "ByteLock Encrypted Escrow Data\n";
        out << "This file must be kept together with bytelock_master_key.txt.\n";
        out << "Neither file alone can recover a folder - both are required.\n";
        out << "To recover a folder: open ByteLock -> Settings -> Recover a Folder,\n";
        out << "select the folder's .blk_recovery file, then enter the Master Recovery Key\n";
        out << "from bytelock_master_key.txt when prompted.\n\n";
        out << QString::fromLatin1(MasterConfig::wrapEscrowKeyWithRecoveryKey(recoveryKey)) << "\n";
        m_statusLabel->setText("Recovery token exported successfully.");
    } else {
        QMessageBox::warning(this, "Save Failed", "Could not write the escrow data file.");
    }
}

void SettingsDialog::onPairMobileClicked()
{
    bool ok = false;
    QString recoveryKey = QInputDialog::getText(this, "Master Recovery", "Enter your Master Recovery Key to pair a mobile device:",
                                                 QLineEdit::Password, "", &ok);
    if (!ok || recoveryKey.isEmpty()) return;

    if (!MasterConfig::verify(recoveryKey)) {
        m_statusLabel->setText("FAILED: Incorrect Master Recovery Key.");
        return;
    }

    PairingDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_statusLabel->setText("Mobile device paired successfully.");
    }
}


