#include "application.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>

Application::Application()
{
    printAppInfo();

    model_ = std::make_unique<Model>();
    view_ = std::make_unique<MainWindow>();

    controller_ = std::make_unique<Controller>(*model_, *view_);

    view_->show();
}

void Application::printAppInfo()
{
    qInfo() << "Application version:" << APP_VERSION;
    qInfo() << "Qt version:" << QT_VERSION_STR;
    qInfo() << "Build type:" << BUILD_TYPE;

#if defined(Q_OS_WIN)
    qInfo() << "OS: Windows";
#elif defined(Q_OS_LINUX)
    qInfo() << "OS: Linux";
#endif
}
