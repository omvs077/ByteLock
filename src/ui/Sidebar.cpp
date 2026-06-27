#include "ui/Sidebar.h"
#include "ui/IconUtils.h"

#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kSidebarWidth = 220;
}

Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(kSidebarWidth);
    setObjectName("Sidebar");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 24, 0, 24);
    layout->setSpacing(4);

    auto* brandLabel = new QLabel("ByteLock", this);
    brandLabel->setObjectName("SidebarBrandLabel");
    auto* taglineLabel = new QLabel("Your Digital Safe", this);
    taglineLabel->setObjectName("SidebarTaglineLabel");

    layout->addWidget(brandLabel);
    layout->addWidget(taglineLabel);
    layout->addSpacing(24);

    m_dashboardButton = new QPushButton(loadSvgIcon(":/icons/dashboard.svg", QSize(18, 18)), "  Dashboard", this);
    m_settingsButton = new QPushButton(loadSvgIcon(":/icons/settings.svg", QSize(18, 18)), "  Settings", this);
    m_aboutButton = new QPushButton(loadSvgIcon(":/icons/about.svg", QSize(18, 18)), "  About", this);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    int id = 0;
    for (QPushButton* btn : {m_dashboardButton, m_settingsButton, m_aboutButton}) {
        btn->setCheckable(true);
        btn->setObjectName("SidebarNavButton");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(44);
        btn->setIconSize(QSize(18, 18));
        m_buttonGroup->addButton(btn, id++);
        layout->addWidget(btn);
    }

    m_dashboardButton->setChecked(true);
    layout->addStretch();

    connect(m_dashboardButton, &QPushButton::clicked, this, [this]() { emit pageRequested(Page::Dashboard); });
    connect(m_settingsButton, &QPushButton::clicked, this, [this]() { emit pageRequested(Page::Settings); });
    connect(m_aboutButton, &QPushButton::clicked, this, [this]() { emit pageRequested(Page::About); });

    setStyleSheet(R"(
        #Sidebar {
            background-color: #10193D;
        }
        #SidebarBrandLabel {
            color: #FFFFFF;
            font-size: 18px;
            font-weight: 700;
            padding-left: 20px;
        }
        #SidebarTaglineLabel {
            color: #8A93A6;
            font-size: 11px;
            padding-left: 20px;
        }
        #SidebarNavButton {
            background-color: transparent;
            color: #C4CAD9;
            border: none;
            text-align: left;
            padding-left: 20px;
            font-size: 13px;
            font-weight: 500;
        }
        #SidebarNavButton:hover {
            background-color: #1A2654;
        }
        #SidebarNavButton:checked {
            background-color: #2F6FED;
            color: #FFFFFF;
            font-weight: 600;
        }
    )");
}

void Sidebar::setActivePage(Page page)
{
    switch (page) {
        case Page::Dashboard: m_dashboardButton->setChecked(true); break;
        case Page::Settings: m_settingsButton->setChecked(true); break;
        case Page::About: m_aboutButton->setChecked(true); break;
    }
}
