#include "application.h"
#include "log_manager.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    int exitCode = 0;

    try
    {
#if defined(QT_DEBUG)
        LogManager::initialize(LogManager::Mode::LogToFileAndConsole, LogManager::Verbosity::Debug);
#else
        LogManager::initialize(LogManager::Mode::LogToFileOnly, LogManager::Verbosity::Info);
#endif

        QApplication app(argc, argv);

        Application application;

        exitCode = QApplication::exec();
    }
    catch (const std::exception& e)
    {
        qCritical() << "Unhandled exception:" << e.what();
        exitCode = 1;
    }
    catch (...)
    {
        qCritical() << "Unhandled unknown exception";
        exitCode = 1;
    }

    LogManager::shutdown();

    return exitCode;
}
