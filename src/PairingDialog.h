#pragma once
#include <QDialog>

class QLabel;
class QCamera;
class QMediaCaptureSession;
class QVideoSink;
class QVideoFrame;

class PairingDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PairingDialog(QWidget* parent = nullptr);
    ~PairingDialog() override;

private slots:
    void onVideoFrame(const QVideoFrame& frame);

private:
    QLabel* m_qrLabel;
    QLabel* m_previewLabel;
    QLabel* m_statusLabel;
    QCamera* m_camera;
    QMediaCaptureSession* m_captureSession;
    QVideoSink* m_videoSink;
    bool m_paired;
};
