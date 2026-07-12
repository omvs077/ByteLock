#include "PairingDialog.h"
#include "MobilePairing.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QMediaDevices>
#include <QPixmap>
#include <QTimer>
#include <ZXing/ReadBarcode.h>

PairingDialog::PairingDialog(QWidget* parent)
    : QDialog(parent), m_paired(false)
{
    setWindowTitle("Pair Mobile Device");
    auto* layout = new QVBoxLayout(this);

    auto* instructions = new QLabel(
        "1. Scan this code with your phone's camera to open the recovery page.\n"
        "2. Your phone will show its own code \u2014 hold it up to this computer's camera.", this);
    instructions->setWordWrap(true);
    layout->addWidget(instructions);

    m_qrLabel = new QLabel(this);
    QPixmap qrPix;
    qrPix.loadFromData(MobilePairing::generateUrlQrPng(), "PNG");
    m_qrLabel->setPixmap(qrPix);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_qrLabel);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumSize(320, 240);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_previewLabel);

    m_statusLabel = new QLabel("Waiting for phone's code...", this);
    layout->addWidget(m_statusLabel);

    m_captureSession = new QMediaCaptureSession(this);
    m_camera = new QCamera(QMediaDevices::defaultVideoInput(), this);
    m_videoSink = new QVideoSink(this);
    m_captureSession->setCamera(m_camera);
    m_captureSession->setVideoSink(m_videoSink);

    connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &PairingDialog::onVideoFrame);
    m_camera->start();
}

PairingDialog::~PairingDialog()
{
    if (m_camera) m_camera->stop();
}

void PairingDialog::onVideoFrame(const QVideoFrame& frame)
{
    if (m_paired) return;

    QImage img = frame.toImage().convertToFormat(QImage::Format_RGBA8888);
    if (img.isNull()) return;

    m_previewLabel->setPixmap(QPixmap::fromImage(img).scaled(
        m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    ZXing::ImageView view(img.constBits(), img.width(), img.height(), ZXing::ImageFormat::RGBA);
    auto barcode = ZXing::ReadBarcode(view);

    if (barcode.isValid()) {
        QString text = QString::fromStdString(barcode.text());
        if (!text.isEmpty()) {
            m_paired = true;
            MobilePairing::storePairedPhoneKey(text.toUtf8());
            m_statusLabel->setText("Phone paired successfully.");
            m_camera->stop();
            QTimer::singleShot(1200, this, &QDialog::accept);
        }
    }
}
