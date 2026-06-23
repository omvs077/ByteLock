#include "ui/MainWindow.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <openssl/opensslv.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle("ByteLock - Scaffold");
    resize(480, 280);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    m_statusLabel = new QLabel("ByteLock scaffold running.\nClick below to verify OpenSSL is linked.", central);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    m_checkButton = new QPushButton("Check OpenSSL Link", central);

    layout->addStretch();
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_checkButton);
    layout->addStretch();

    setCentralWidget(central);

    connect(m_checkButton, &QPushButton::clicked, this, &MainWindow::onCheckOpenSslClicked);
}

void MainWindow::onCheckOpenSslClicked()
{
    QString versionText = QString("OpenSSL linked successfully:\n%1").arg(OPENSSL_VERSION_TEXT);
    m_statusLabel->setText(versionText);
}
