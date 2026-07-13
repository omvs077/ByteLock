#include "RecoveryDialog.h"
#include "MobilePairing.h"
#include "LocalPairingServer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QTimer>

RecoveryDialog::RecoveryDialog(QWidget* parent)
    : QDialog(parent), m_verified(false)
{
    setWindowTitle("Recover via Phone");
    setFixedWidth(380);
    setStyleSheet("QDialog { background: #ffffff; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(14);

    auto* heading = new QLabel("Recover via Phone", this);
    heading->setStyleSheet("font-weight: 600; font-size: 16px; color: #1c2430;");
    layout->addWidget(heading);

    m_stepLabel = new QLabel("Scan this code with your paired phone to confirm it's you.", this);
    m_stepLabel->setWordWrap(true);
    m_stepLabel->setStyleSheet("font-size: 13px; color: #4a5568;");
    layout->addWidget(m_stepLabel);

    m_qrLabel = new QLabel(this);
    m_qrLabel->setFixedSize(220, 220);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->setStyleSheet("background: #f4f6f9; border-radius: 8px;");
    auto* qrRow = new QHBoxLayout();
    qrRow->addStretch();
    qrRow->addWidget(m_qrLabel);
    qrRow->addStretch();
    layout->addLayout(qrRow);

    m_statusPanel = new QLabel("Waiting for your phone...", this);
    m_statusPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_statusPanel->setWordWrap(true);
    m_statusPanel->setStyleSheet(
        "background: #eef3fe; color: #2f6fed; font-size: 12px; padding: 12px; border-radius: 6px;");
    layout->addWidget(m_statusPanel);

    m_closeButton = new QPushButton("Cancel", this);
    m_closeButton->setFixedHeight(44);
    m_closeButton->setStyleSheet(
        "QPushButton { background: #f4f6f9; color: #1c2430; border: none; border-radius: 6px; font-size: 13px; }"
        "QPushButton:hover { background: #e8ecf2; }");
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(m_closeButton);

    if (!MobilePairing::isPaired()) {
        showErrorState("No phone is paired with ByteLock yet.");
        m_server = nullptr;
        return;
    }

    m_server = new LocalPairingServer(LocalPairingServer::Mode::Recovery, this);
    connect(m_server, &LocalPairingServer::recoverySignatureReceived, this, &RecoveryDialog::onSignatureReceived);
    connect(m_server, &LocalPairingServer::errorOccurred, this, &RecoveryDialog::onServerError);

    if (m_server->start()) {
        QString url = m_server->pairingUrl();
        if (url.isEmpty()) {
            showErrorState("Could not detect a network connection.");
        } else {
            QPixmap qrPix;
            qrPix.loadFromData(MobilePairing::generateQrPng(url), "PNG");
            m_qrLabel->setPixmap(qrPix.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            showWaitingState();
            QTimer::singleShot(150000, this, &RecoveryDialog::onTimeout);
        }
    } else {
        showErrorState("Could not start the local recovery server.");
    }
}

RecoveryDialog::~RecoveryDialog()
{
    if (m_server) m_server->stop();
}

void RecoveryDialog::showWaitingState()
{
    m_stepLabel->setText("Scan this code with your paired phone to confirm it's you.");
    m_statusPanel->setStyleSheet(
        "background: #eef3fe; color: #2f6fed; font-size: 12px; padding: 12px; border-radius: 6px;");
    m_statusPanel->setText("Waiting for your phone...");
}

void RecoveryDialog::showSuccessState()
{
    m_stepLabel->setText("Your identity has been confirmed.");
    m_statusPanel->setStyleSheet(
        "background: #e7f7ee; color: #1c7a3d; font-size: 12px; padding: 12px; border-radius: 6px;");
    m_statusPanel->setText("Verified successfully.");
    m_closeButton->setText("Done");
}

void RecoveryDialog::showErrorState(const QString& message)
{
    m_stepLabel->setText("Recovery could not be completed.");
    m_statusPanel->setStyleSheet(
        "background: #fdeceb; color: #c0392b; font-size: 12px; padding: 12px; border-radius: 6px;");
    m_statusPanel->setText(message);
    m_closeButton->setText("Close");
}

void RecoveryDialog::onSignatureReceived(const QByteArray& signatureBase64)
{
    if (m_verified) return;

    QByteArray token = m_server->token().toUtf8();
    QByteArray publicKey = MobilePairing::pairedPhonePublicKey();
    if (!MobilePairing::verifySignature(token, signatureBase64, publicKey)) {
        showErrorState("Signature verification failed.");
        return;
    }

    m_verified = true;
    showSuccessState();
    QTimer::singleShot(1200, this, &QDialog::accept);
}

void RecoveryDialog::onServerError(const QString& message)
{
    showErrorState(message);
}

void RecoveryDialog::onTimeout()
{
    if (m_verified) return;
    showErrorState("This is taking a while \u2014 check your phone's WiFi connection and try again.");
}
