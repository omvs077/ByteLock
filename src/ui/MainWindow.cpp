#include "ui/MainWindow.h"
#include "ui/AboutPage.h"
#include "ui/DashboardPage.h"
#include "ui/SettingsPage.h"
#include "ui/Sidebar.h"
#include "ui/TitleBar.h"
#include "viewmodel/MainViewModel.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#ifdef Q_OS_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#endif

namespace {
constexpr int kResizeBorderPx = 6;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_viewModel = new MainViewModel(this);
    setupUi();

#ifdef Q_OS_WIN
    winId();
#endif
}

MainWindow::~MainWindow() = default;

void MainWindow::seedSessionPassword(const QString& password)
{
    m_viewModel->seedSessionPassword(password);
}

void MainWindow::setupUi()
{
    resize(1000, 650);
    setWindowTitle("ByteLock");

    auto* outer = new QWidget(this);
    auto* outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_titleBar = new TitleBar(outer);
    outerLayout->addWidget(m_titleBar);

    auto* body = new QWidget(outer);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    m_sidebar = new Sidebar(body);
    bodyLayout->addWidget(m_sidebar);

    m_stackedWidget = new QStackedWidget(body);
    m_stackedWidget->addWidget(new DashboardPage(m_viewModel, m_stackedWidget));
    m_stackedWidget->addWidget(new SettingsPage(m_stackedWidget));
    m_stackedWidget->addWidget(new AboutPage(m_stackedWidget));
    bodyLayout->addWidget(m_stackedWidget, 1);

    outerLayout->addWidget(body, 1);

    setCentralWidget(outer);

    connect(m_titleBar, &TitleBar::minimizeRequested, this, &MainWindow::onTitleBarMinimize);
    connect(m_titleBar, &TitleBar::maximizeRestoreRequested, this, &MainWindow::onTitleBarMaximizeRestore);
    connect(m_titleBar, &TitleBar::closeRequested, this, &MainWindow::onTitleBarClose);

    connect(m_sidebar, &Sidebar::pageRequested, this, [this](Sidebar::Page page) {
        m_stackedWidget->setCurrentIndex(static_cast<int>(page));
    });
}

void MainWindow::onTitleBarMinimize()
{
    showMinimized();
}

void MainWindow::onTitleBarMaximizeRestore()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void MainWindow::onTitleBarClose()
{
    close();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && m_titleBar) {
        m_titleBar->setMaximizedState(isMaximized());
    }
    QMainWindow::changeEvent(event);
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    if (eventType != "windows_generic_MSG") {
        return QMainWindow::nativeEvent(eventType, message, result);
    }

    MSG* msg = static_cast<MSG*>(message);

    switch (msg->message) {
    case WM_NCCALCSIZE: {
        if (msg->wParam == TRUE) {
            auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
            if (::IsZoomed(msg->hwnd)) {
                const int borderThickness = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                params->rgrc[0].left += borderThickness;
                params->rgrc[0].right -= borderThickness;
                params->rgrc[0].bottom -= borderThickness;
                params->rgrc[0].top += borderThickness;
            }
            *result = 0;
            return true;
        }
        return false;
    }
    case WM_NCHITTEST: {
        const long x = GET_X_LPARAM(msg->lParam);
        const long y = GET_Y_LPARAM(msg->lParam);

        RECT winRect;
        ::GetWindowRect(msg->hwnd, &winRect);

        const LONG border = static_cast<LONG>(kResizeBorderPx * devicePixelRatioF());

        const bool onLeft = x < winRect.left + border;
        const bool onRight = x >= winRect.right - border;
        const bool onTop = y < winRect.top + border;
        const bool onBottom = y >= winRect.bottom - border;

        if (!isMaximized()) {
            if (onTop && onLeft) { *result = HTTOPLEFT; return true; }
            if (onTop && onRight) { *result = HTTOPRIGHT; return true; }
            if (onBottom && onLeft) { *result = HTBOTTOMLEFT; return true; }
            if (onBottom && onRight) { *result = HTBOTTOMRIGHT; return true; }
            if (onLeft) { *result = HTLEFT; return true; }
            if (onRight) { *result = HTRIGHT; return true; }
            if (onTop) { *result = HTTOP; return true; }
            if (onBottom) { *result = HTBOTTOM; return true; }
        }

        if (m_titleBar) {
            const qreal dpr = devicePixelRatioF();
            const QPoint titleBarOriginLogical = m_titleBar->mapToGlobal(QPoint(0, 0));
            const QRect titleBarPhysicalRect(
                static_cast<int>(titleBarOriginLogical.x() * dpr),
                static_cast<int>(titleBarOriginLogical.y() * dpr),
                static_cast<int>(m_titleBar->width() * dpr),
                static_cast<int>(m_titleBar->height() * dpr));

            if (titleBarPhysicalRect.contains(QPoint(x, y))) {
                const int localX = static_cast<int>((x - titleBarPhysicalRect.x()) / dpr);
                const int localY = static_cast<int>((y - titleBarPhysicalRect.y()) / dpr);
                QWidget* child = m_titleBar->childAt(localX, localY);
                if (child == nullptr) {
                    *result = HTCAPTION;
                    return true;
                }
                return false;
            }
        }

        return false;
    }
    case WM_NCLBUTTONDBLCLK: {
        if (msg->wParam == HTCAPTION) {
            if (isMaximized()) {
                showNormal();
            } else {
                showMaximized();
            }
            *result = 0;
            return true;
        }
        return false;
    }
    default:
        return QMainWindow::nativeEvent(eventType, message, result);
    }
}
#endif

