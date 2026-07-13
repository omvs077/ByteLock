#include "MobilePairing.h"
#include "MasterConfig.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QBuffer>
#include <ZXing/MultiFormatWriter.h>
#include <ZXing/BitMatrix.h>
#include <openssl/evp.h>

namespace MobilePairing {

bool isPaired()
{
    return !pairedPhonePublicKey().isEmpty();
}

QByteArray generateQrPng(const QString& text)
{
    ZXing::MultiFormatWriter writer(ZXing::BarcodeFormat::QRCode);
    writer.setMargin(2);
    auto matrix = writer.encode(text.toStdString(), 300, 300);

    QImage img(matrix.width(), matrix.height(), QImage::Format_RGB32);
    for (int y = 0; y < matrix.height(); ++y) {
        for (int x = 0; x < matrix.width(); ++x) {
            img.setPixel(x, y, matrix.get(x, y) ? qRgb(0, 0, 0) : qRgb(255, 255, 255));
        }
    }

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return png;
}

void storePairedPhoneKey(const QByteArray& publicKeyBase64)
{
    QFile inFile(MasterConfig::configPath());
    QJsonObject root;
    if (inFile.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(inFile.readAll()).object();
        inFile.close();
    }
    QJsonObject mobile = root["mobile_recovery"].toObject();
    mobile["phone_public_key_b64"] = QString::fromUtf8(publicKeyBase64);
    root["mobile_recovery"] = mobile;

    QFile outFile(MasterConfig::configPath());
    if (outFile.open(QIODevice::WriteOnly)) {
        outFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

QByteArray pairedPhonePublicKey()
{
    QFile inFile(MasterConfig::configPath());
    if (!inFile.open(QIODevice::ReadOnly)) return {};
    QJsonObject root = QJsonDocument::fromJson(inFile.readAll()).object();
    inFile.close();
    return root["mobile_recovery"].toObject()["phone_public_key_b64"].toString().toUtf8();
}

void clearPairing()
{
    QFile inFile(MasterConfig::configPath());
    QJsonObject root;
    if (inFile.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(inFile.readAll()).object();
        inFile.close();
    }
    root.remove("mobile_recovery");

    QFile outFile(MasterConfig::configPath());
    if (outFile.open(QIODevice::WriteOnly)) {
        outFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

bool verifySignature(const QByteArray& message, const QByteArray& signatureBase64, const QByteArray& publicKeyBase64)
{
    QByteArray pubKey = QByteArray::fromBase64(publicKeyBase64);
    QByteArray sig = QByteArray::fromBase64(signatureBase64);
    if (pubKey.size() != 32 || sig.size() != 64) return false;

    EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const unsigned char*>(pubKey.constData()), pubKey.size());
    if (!pkey) return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    bool ok = false;
    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1) {
        ok = EVP_DigestVerify(ctx,
            reinterpret_cast<const unsigned char*>(sig.constData()), sig.size(),
            reinterpret_cast<const unsigned char*>(message.constData()), message.size()) == 1;
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ok;
}

}
