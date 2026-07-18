#include "SetupDialog.h"
#include "MasterConfig.h"
#include "engine/CryptoEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QClipboard>
#include <QApplication>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

using namespace bytelock;

namespace {

QString formatKey(const std::vector<uint8_t>& raw)
{
    QByteArray ba(reinterpret_cast<const char*>(raw.data()), static_cast<int>(raw.size()));
    QString hex = QString::fromLatin1(ba.toHex()).toUpper();
    QString grouped;
    for (int i = 0; i < hex.length(); i += 4) {
        if (!grouped.isEmpty()) grouped += "-";
        grouped += hex.mid(i, 4);
    }
    return grouped;
}

} // namespace

SetupDialog::SetupDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("ByteLock — Master Recovery Key Setup");
    setModal(true);

    auto keyBytes = CryptoEngine::randomBytes(32);
    m_recoveryKey = keyBytes ? formatKey(keyBytes.value()) : QString();
    MasterConfig::setup(m_recoveryKey);

    auto* layout = new QVBoxLayout(this);

    auto* info = new QLabel(
        "ByteLock has generated a unique Master Recovery Key.\n"
        "This key can recover ANY locked folder if you forget its password.\n"
        "It is shown only once and is never stored by ByteLock.\n"
        "Save it now — copy it or download it as a text file.");
    info->setWordWrap(true);
    layout->addWidget(info);

    m_keyField = new QLineEdit(m_recoveryKey);
    m_keyField->setReadOnly(true);
    m_keyField->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_keyField);

    auto* btnRow = new QHBoxLayout();
    auto* copyBtn = new QPushButton("Copy to Clipboard");
    auto* saveBtn = new QPushButton("Save to File...");
    btnRow->addWidget(copyBtn);
    btnRow->addWidget(saveBtn);
    layout->addLayout(btnRow);

    m_confirmCheck = new QCheckBox("I have saved this key securely");
    layout->addWidget(m_confirmCheck);

    auto* liabilityLabel = new QLabel(
        "I have read the README and understand that ByteLock is zero-knowledge: if I "
        "lose a folder password, lose my Master Recovery Key, and have not paired a "
        "recovery phone, my data cannot be recovered by anyone, including the developer.");
    liabilityLabel->setWordWrap(true);
    liabilityLabel->setStyleSheet("color: #6b7280; font-size: 11px;");
    layout->addWidget(liabilityLabel);

    m_liabilityCheck = new QCheckBox("I accept sole responsibility for safeguarding these.");
    layout->addWidget(m_liabilityCheck);

    m_okButton = new QPushButton("Continue");
    m_okButton->setEnabled(false);
    layout->addWidget(m_okButton);

    connect(copyBtn, &QPushButton::clicked, this, &SetupDialog::onCopyClicked);
    connect(saveBtn, &QPushButton::clicked, this, &SetupDialog::onSaveClicked);
    connect(m_confirmCheck, &QCheckBox::toggled, this, &SetupDialog::onCheckboxToggled);
    connect(m_liabilityCheck, &QCheckBox::toggled, this, &SetupDialog::onCheckboxToggled);
    connect(m_okButton, &QPushButton::clicked, this, [this]() {

        accept();
    });

    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
}

void SetupDialog::onCopyClicked()
{
    QApplication::clipboard()->setText(m_recoveryKey);
}

void SetupDialog::onSaveClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Choose folder to save recovery files");
    if (dir.isEmpty()) return;

    QFile keyFile(dir + "/bytelock_master_key.txt");
    if (keyFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&keyFile);
        out << "ByteLock Master Recovery Key\n";
        out << "Generated once. Never stored by ByteLock. Keep this file safe.\n\n";
        out << m_recoveryKey << "\n";
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
        out << QString::fromLatin1(MasterConfig::wrapEscrowKeyWithRecoveryKey(m_recoveryKey)) << "\n";
    } else {
        QMessageBox::warning(this, "Save Failed", "Could not write the escrow data file.");
    }
}

void SetupDialog::onCheckboxToggled(bool)
{
    m_okButton->setEnabled(m_confirmCheck->isChecked() && m_liabilityCheck->isChecked());
}

