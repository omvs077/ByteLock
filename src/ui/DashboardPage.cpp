#include "ui/DashboardPage.h"
#include "viewmodel/MainViewModel.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

DashboardPage::DashboardPage(MainViewModel* viewModel, QWidget* parent)
    : QWidget(parent)
    , m_viewModel(viewModel)
{
    setupUi();

    connect(m_viewModel, &MainViewModel::busyChanged, this, &DashboardPage::onBusyChanged);
    connect(m_viewModel, &MainViewModel::statusTextChanged, this, &DashboardPage::onStatusTextChanged);
}

void DashboardPage::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("ByteLock scaffold running.", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);

    m_checkButton = new QPushButton("Check OpenSSL Link", this);
    m_selfTestButton = new QPushButton("Run Crypto Self-Test", this);
    m_lockFolderButton = new QPushButton("Lock a Folder...", this);
    m_unlockFolderButton = new QPushButton("Unlock a .blk Container...", this);

    layout->addStretch();
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_checkButton);
    layout->addWidget(m_selfTestButton);
    layout->addWidget(m_lockFolderButton);
    layout->addWidget(m_unlockFolderButton);
    layout->addStretch();

    connect(m_checkButton, &QPushButton::clicked, this, &DashboardPage::onCheckOpenSslClicked);
    connect(m_selfTestButton, &QPushButton::clicked, this, &DashboardPage::onRunCryptoSelfTestClicked);
    connect(m_lockFolderButton, &QPushButton::clicked, this, &DashboardPage::onLockFolderClicked);
    connect(m_unlockFolderButton, &QPushButton::clicked, this, &DashboardPage::onUnlockFolderClicked);
}

void DashboardPage::setButtonsEnabled(bool enabled)
{
    m_checkButton->setEnabled(enabled);
    m_selfTestButton->setEnabled(enabled);
    m_lockFolderButton->setEnabled(enabled);
    m_unlockFolderButton->setEnabled(enabled);
}

void DashboardPage::onBusyChanged(bool busy)
{
    setButtonsEnabled(!busy);
}

void DashboardPage::onStatusTextChanged(const QString& text)
{
    m_statusLabel->setText(text);
}

void DashboardPage::onCheckOpenSslClicked()
{
    m_statusLabel->setText("OpenSSL linked successfully:\n" + m_viewModel->opensslVersionText());
}

void DashboardPage::onRunCryptoSelfTestClicked()
{
    m_viewModel->requestRunCryptoSelfTest();
}

void DashboardPage::onLockFolderClicked()
{
    QString folder = QFileDialog::getExistingDirectory(this, "Select folder to lock");
    if (folder.isEmpty()) return;

    if (m_viewModel->hasSessionPassword()) {
        auto choice = QMessageBox::question(this, "Use Saved Password?",
            "Use your saved session password for this folder?\n\n"
            "(Choose No to set a different password for just this folder.)",
            QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::Yes) {
            m_viewModel->requestLockFolderWithSessionPassword(folder);
            return;
        }
    }

    bool ok = false;
    QString password = QInputDialog::getText(this, "Set Password", "Enter a password to lock this folder:",
                                              QLineEdit::Password, "", &ok);
    if (!ok || password.isEmpty()) return;

    m_viewModel->requestLockFolder(folder, password, true);
}

void DashboardPage::onUnlockFolderClicked()
{
    QString containerPath = QFileDialog::getOpenFileName(this, "Select .blk container to unlock", "", "ByteLock Container (*.blk)");
    if (containerPath.isEmpty()) return;

    if (m_viewModel->hasSessionPassword()) {
        auto choice = QMessageBox::question(this, "Use Saved Password?",
            "Use your saved session password to unlock this folder?\n\n"
            "(Choose No to enter a different password.)",
            QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::Yes) {
            m_viewModel->requestUnlockFolderWithSessionPassword(containerPath);
            return;
        }
    }

    bool ok = false;
    QString password = QInputDialog::getText(this, "Enter Password", "Enter the password for this folder:",
                                              QLineEdit::Password, "", &ok);
    if (!ok || password.isEmpty()) return;

    m_viewModel->requestUnlockFolder(containerPath, password, true);
}
