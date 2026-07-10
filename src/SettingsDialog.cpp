#include "SettingsDialog.h"
#include "MasterConfig.h"
#include "engine/CryptoEngine.h"
#include "engine/FolderPacker.h"
#include "engine/SecureBytes.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

using namespace bytelock;

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("ByteLock — Settings");
    setModal(true);
    resize(420, 200);

    auto* layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("Master Recovery Settings", this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_recoverButton = new QPushButton("Recover a Folder...", this);
    m_exportButton = new QPushButton("Export Recovery Token...", this);
    layout->addWidget(m_recoverButton);
    layout->addWidget(m_exportButton);

    connect(m_recoverButton, &QPushButton::clicked, this, &SettingsDialog::onRecoverFolderClicked);
    connect(m_exportButton, &QPushButton::clicked, this, &SettingsDialog::onExportTokenClicked);
}

void SettingsDialog::onRecoverFolderClicked()
{
    QString sidecarPath = QFileDialog::getOpenFileName(this, "Select .blk_recovery sidecar", "", "ByteLock Recovery (*.blk_recovery)");
    if (sidecarPath.isEmpty()) return;

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

    QString path = QFileDialog::getSaveFileName(this, "Save Recovery Token",
                                                  "bytelock_recovery_key.txt", "Text Files (*.txt)");
    if (path.isEmpty()) return;

    QMessageBox::warning(this, "Security Warning",
        "This file, combined with your Master Recovery Key, can recover ANY locked folder.\n"
        "Never store it on this computer. Keep it on a USB drive or other secure offline location.");

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "ByteLock Master Recovery Key\n";
        out << "Generated once. Never stored by ByteLock. Keep this file safe.\n\n";
        out << recoveryKey << "\n";
        out << "\n---\n";
        out << "Encrypted Escrow Data (required for recovery, keep with the key above):\n";
        out << QString::fromLatin1(MasterConfig::wrapEscrowKeyWithRecoveryKey(recoveryKey)) << "\n";
        m_statusLabel->setText("Recovery token exported successfully.");
    } else {
        QMessageBox::warning(this, "Save Failed", "Could not write the recovery token file.");
    }
}
