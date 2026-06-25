#pragma once

#include <QMainWindow>

class QWidget;
class QStackedWidget;
class MainViewModel;
class TitleBar;
class Sidebar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif
    void changeEvent(QEvent* event) override;

private slots:
    void onTitleBarMinimize();
    void onTitleBarMaximizeRestore();
    void onTitleBarClose();

private:
    void setupUi();

    MainViewModel* m_viewModel = nullptr;
    TitleBar* m_titleBar = nullptr;
    Sidebar* m_sidebar = nullptr;
    QStackedWidget* m_stackedWidget = nullptr;
};
