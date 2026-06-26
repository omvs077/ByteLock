#include "ui/TitleBar.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace {
constexpr int kTitleBarHeight = 40;
constexpr int kButtonWidth = 46;
}

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(kTitleBarHeight);
    setObjectName("TitleBar");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 0, 0);
    layout->setSpacing(0);

    m_titleLabel = new QLabel("ByteLock", this);
    m_titleLabel->setObjectName("TitleBarLabel");
    {
        QPalette pal = m_titleLabel->palette();
        pal.setColor(QPalette::WindowText, QColor("#FFFFFF"));
        m_titleLabel->setPalette(pal);
        QFont f = m_titleLabel->font();
        f.setPointSize(11);
        f.setBold(true);
        m_titleLabel->setFont(f);
    }

    m_minimizeButton = new QPushButton(QString::fromUtf8("\xE2\x80\x94"), this);
    m_maximizeButton = new QPushButton(QString::fromUtf8("\xE2\x96\xA1"), this);
    m_closeButton = new QPushButton(QString::fromUtf8("\xE2\x9C\x95"), this);

    for (QPushButton* btn : {m_minimizeButton, m_maximizeButton, m_closeButton}) {
        btn->setFixedSize(kButtonWidth, kTitleBarHeight);
        btn->setFlat(true);
        btn->setCursor(Qt::ArrowCursor);
    }

    m_minimizeButton->setObjectName("TitleBarMinimizeButton");
    m_maximizeButton->setObjectName("TitleBarMaximizeButton");
    m_closeButton->setObjectName("TitleBarCloseButton");

    layout->addWidget(m_titleLabel);
    layout->addStretch();
    layout->addWidget(m_minimizeButton);
    layout->addWidget(m_maximizeButton);
    layout->addWidget(m_closeButton);

    setStyleSheet(R"(
        #TitleBar {
            background-color: #10193D;
        }
        #TitleBarMinimizeButton, #TitleBarMaximizeButton {
            background-color: transparent;
            color: #C4CAD9;
            border: none;
            font-size: 14px;
        }
        #TitleBarMinimizeButton:hover, #TitleBarMaximizeButton:hover {
            background-color: #1F2B5C;
        }
        #TitleBarCloseButton {
            background-color: transparent;
            color: #C4CAD9;
            border: none;
            font-size: 14px;
        }
        #TitleBarCloseButton:hover {
            background-color: #E5484D;
            color: #FFFFFF;
        }
    )");

    connect(m_minimizeButton, &QPushButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maximizeButton, &QPushButton::clicked, this, &TitleBar::maximizeRestoreRequested);
    connect(m_closeButton, &QPushButton::clicked, this, &TitleBar::closeRequested);
}

void TitleBar::setMaximizedState(bool maximized)
{
    m_maximizeButton->setText(maximized
        ? QString::fromUtf8("\xE2\x9D\x90")
        : QString::fromUtf8("\xE2\x96\xA1"));
}

