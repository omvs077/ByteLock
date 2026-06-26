#include "ui/DashboardPage.h"
#include "ui/AuthGateDialog.h"
#include "viewmodel/MainViewModel.h"
#include "engine/DefaultPasswordStore.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace {
constexpr int kColName = 0;
constexpr int kColLocation = 1;
constexpr int kColStatus = 2;
constexpr int kColModified = 3;
constexpr int kColActions = 4;
}

DashboardPage::DashboardPage(MainViewModel* viewModel, QWidget* parent)
    : QWidget(parent)
    , m_viewModel(viewModel)
{
    setupUi();

    connect(m_viewModel, &MainViewModel::busyChanged, this, &DashboardPage::onBusyChanged);
    connect(m_viewModel, &MainViewModel::statusTextChanged, this, &DashboardPage::onStatusTextChanged);
    connect(m_viewModel, &MainViewModel::lockFinished, this, &DashboardPage::onLockFinished);
    connect(m_viewModel, &MainViewModel::unlockFinished, this, &DashboardPage::onUnlockFinished);

    refreshTable();
}

void DashboardPage::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto* cardsRow = new QHBoxLayout();
    m_lockFolderCard = new QPushButton("Lock a Folder...", this);
    m_unlockFolderCard = new QPushButton("Unlock a .blk Container...", this);
    for (QPushButton* card : {m_lockFolderCard, m_unlockFolderCard}) {
        card->setMinimumHeight(64);
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet(R"(
            QPushButton {
                background-color: white;
                border: 1px solid #E2E5EC;
                border-radius: 8px;
                font-weight: 600;
                font-size: 13px;
                color: #10193D;
            }
            QPushButton:hover { border-color: #2F6FED; }
        )");
    }
    cardsRow->addWidget(m_lockFolderCard);
    cardsRow->addWidget(m_unlockFolderCard);
    layout->addLayout(cardsRow);

    m_statusLabel = new QLabel("", this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({"Folder Name", "Location", "Status", "Last Modified", ""});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(kColLocation, QHeaderView::Stretch);
    m_table->setColumnWidth(kColActions, 50);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table, 1);

    m_checkButton = new QPushButton("Check OpenSSL Link", this);
    m_selfTestButton = new QPushButton("Run Crypto Self-Test", this);
    layout->addWidget(m_checkButton);
    layout->addWidget(m_selfTestButton);

    connect(m_lockFolderCard, &QPushButton::clicked, this, &DashboardPage::onLockFolderClicked);
    connect(m_unlockFolderCard, &QPushButton::clicked, this, &DashboardPage::onUnlockFolderClicked);
    connect(m_checkButton, &QPushButton::clicked, this, &DashboardPage::onCheckOpenSslClicked);
    connect(m_selfTestButton, &QPushButton::clicked, this, &DashboardPage::onRunCryptoSelfTestClicked);
}

void DashboardPage::refreshTable()
{
    const QVector<TrackedFolder> entries = m_registry.list();
    m_table->setRowCount(entries.size());

    for (int row = 0; row < entries.size(); ++row) {
        const TrackedFolder& e = entries[row];

        m_table->setItem(row, kColName, new QTableWidgetItem(QFileInfo(e.originalPath).fileName()));
        m_table->setItem(row, kColLocation, new QTableWidgetItem(e.originalPath));

        auto* statusItem = new QTableWidgetItem(e.locked ? "Locked" : "Unlocked");
        statusItem->setForeground(QBrush(e.locked ? QColor("#F5A623") : QColor("#16A085")));
        m_table->setItem(row, kColStatus, statusItem);

        m_table->setItem(row, kColModified, new QTableWidgetItem(e.lastModified.toString("MMM d, yyyy h:mm AP")));

        auto* menuButton = new QPushButton("...", m_table);
        menuButton->setCursor(Qt::PointingHandCursor);
        menuButton->setFixedWidth(40);
        connect(menuButton, &QPushButton::clicked, this, [this, menuButton, row]() {
            showRowMenu(row, menuButton->mapToGlobal(QPoint(0, menuButton->height())));
        });
        m_table->setCellWidget(row, kColActions, menuButton);
    }
}

void DashboardPage::showRowMenu(int row, const QPoint& globalPos)
{
    const QVector<TrackedFolder> entries = m_registry.list();
    if (row < 0 || row >= entries.size()) return;
    const TrackedFolder entry = entries[row];

    QMenu menu(this);
    if (entry.locked) {
        QAction* unlockAction = menu.addAction("Unlock");
        connect(unlockAction, &QAction::triggered, this, [this, entry]() {
            startUnlockFlow(entry.containerPath, entry.usesCustomPassword);
        });
    } else {
        QAction* lockAction = menu.addAction("Lock");
        connect(lockAction, &QAction::triggered, this, [this, entry]() {
            startLockFlow(entry.originalPath);
        });
    }

    QAction* removeAction = menu.addAction("Remove from list");
    connect(removeAction, &QAction::triggered, this, [this, entry]() {
        auto choice = QMessageBox::question(this, "Remove from list?",
            "This only stops ByteLock from tracking this folder - it does not delete or change any files.",
            QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::Yes) {
            m_registry.removeByOriginalPath(entry.originalPath);
            refreshTable();
        }
    });

    menu.exec(globalPos);
}

void DashboardPage::startLockFlow(const QString& folderPath)
{
    auto choice = QMessageBox::question(this, "Choose Password",
        "Use the common password for this folder, or set a different one just for it?\n\n"
        "(Yes = common password, No = set a custom password for this folder)",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    const bool useCustom = (choice == QMessageBox::No);

    AuthGateDialog::Mode mode;
    if (useCustom) {
        mode = AuthGateDialog::Mode::SetCustomPassword;
    } else {
        bytelock::DefaultPasswordStore store;
        mode = store.exists() ? AuthGateDialog::Mode::EnterPassword : AuthGateDialog::Mode::SetNewPassword;
    }

    AuthGateDialog gate(mode, this);
    if (gate.exec() != QDialog::Accepted) return;

    m_pendingFolderPath = folderPath;
    m_pendingIsCustom = useCustom;
    m_viewModel->requestLockFolder(folderPath, gate.verifiedPassword(), false);
}

void DashboardPage::startUnlockFlow(const QString& containerPath, bool usesCustomPassword)
{
    AuthGateDialog::Mode mode;
    if (usesCustomPassword) {
        mode = AuthGateDialog::Mode::EnterCustomPassword;
    } else {
        bytelock::DefaultPasswordStore store;
        mode = store.exists() ? AuthGateDialog::Mode::EnterPassword : AuthGateDialog::Mode::SetNewPassword;
    }

    AuthGateDialog gate(mode, this);
    if (gate.exec() != QDialog::Accepted) return;

    m_pendingContainerPath = containerPath;
    m_viewModel->requestUnlockFolder(containerPath, gate.verifiedPassword(), false);
}

void DashboardPage::onLockFolderClicked()
{
    QString folder = QFileDialog::getExistingDirectory(this, "Select folder to lock");
    if (folder.isEmpty()) return;
    startLockFlow(folder);
}

void DashboardPage::onUnlockFolderClicked()
{
    QString containerPath = QFileDialog::getOpenFileName(this, "Select .blk container to unlock", "", "ByteLock Container (*.blk)");
    if (containerPath.isEmpty()) return;

    TrackedFolder existing;
    bool usesCustom = m_registry.findByAnyPath(containerPath, existing) ? existing.usesCustomPassword : false;
    startUnlockFlow(containerPath, usesCustom);
}

void DashboardPage::onLockFinished(bool success, const QString& message)
{
    Q_UNUSED(message);
    if (success && !m_pendingFolderPath.isEmpty()) {
        TrackedFolder entry;
        entry.originalPath = m_pendingFolderPath;
        entry.containerPath = m_pendingFolderPath + ".blk";
        entry.locked = true;
        entry.usesCustomPassword = m_pendingIsCustom;
        entry.lastModified = QDateTime::currentDateTime();
        m_registry.upsert(entry);
        refreshTable();
    }
    m_pendingFolderPath.clear();
}

void DashboardPage::onUnlockFinished(bool success, const QString& message)
{
    Q_UNUSED(message);
    if (success && !m_pendingContainerPath.isEmpty()) {
        TrackedFolder entry;
        if (m_registry.findByAnyPath(m_pendingContainerPath, entry)) {
            entry.locked = false;
            entry.lastModified = QDateTime::currentDateTime();
            m_registry.upsert(entry);
            refreshTable();
        }
    }
    m_pendingContainerPath.clear();
}

void DashboardPage::setButtonsEnabled(bool enabled)
{
    m_checkButton->setEnabled(enabled);
    m_selfTestButton->setEnabled(enabled);
    m_lockFolderCard->setEnabled(enabled);
    m_unlockFolderCard->setEnabled(enabled);
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
