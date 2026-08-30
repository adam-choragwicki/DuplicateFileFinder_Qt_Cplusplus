#include "application.h"
#include "development_configuration_helper.h"

#include <QApplication>
#include <QDebug>
#include <QStyleFactory>
#include <QStyleHints>

Application::Application()
{
    printAppInfo();
    applyLightTheme();

    model_ = std::make_unique<Model>();

    for (const QString& initialScanDirectoryPath: DevelopmentConfigurationHelper::getInitialDirectoryScanPaths())
    {
        model_->addScanDirectory(initialScanDirectoryPath);
    }

    view_ = std::make_unique<MainWindow>();

    controller_ = std::make_unique<Controller>(*model_, *view_);

    view_->show();
}

void Application::applyLightTheme()
{
    // Ask the platform integration to use light colors for elements outside the widget palette as well, such as native window decorations where supported.
    // This overrides dark theme on Windows
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
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
