#include "ui/AboutPage.h"

#include <QLabel>
#include <QVBoxLayout>

AboutPage::AboutPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel("ByteLock", this);
    titleLabel->setStyleSheet("font-size: 22px; font-weight: 700;");
    titleLabel->setAlignment(Qt::AlignCenter);

    auto* versionLabel = new QLabel("Version 0.1.0 (in development)", this);
    versionLabel->setAlignment(Qt::AlignCenter);

    auto* descriptionLabel = new QLabel(
        "Zero-knowledge AES-256-GCM folder encryption for Windows.\n"
        "Built with C++20, Qt6 Widgets, and OpenSSL.",
        this);
    descriptionLabel->setAlignment(Qt::AlignCenter);
    descriptionLabel->setWordWrap(true);

    auto* licenseLabel = new QLabel(
        "This application uses Qt6 under the LGPLv3 license (dynamically linked).\n"
        "See qt.io/licensing for details.",
        this);
    licenseLabel->setAlignment(Qt::AlignCenter);
    licenseLabel->setWordWrap(true);
    licenseLabel->setStyleSheet("color: #8A93A6; font-size: 11px;");

    layout->addStretch();
    layout->addWidget(titleLabel);
    layout->addWidget(versionLabel);
    layout->addSpacing(16);
    layout->addWidget(descriptionLabel);
    layout->addStretch();
    layout->addWidget(licenseLabel);
}
