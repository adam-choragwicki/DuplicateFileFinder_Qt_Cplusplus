#include "application.h"
#include "log_manager.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    try
    {
#if defined(QT_DEBUG)
        LogManager::initialize(LogManager::Mode::LogToFileAndConsole, LogManager::Verbosity::Debug);
#else
        LogManager::initialize(LogManager::Mode::LogToFileOnly, LogManager::Verbosity::Info);
#endif

        QApplication app(argc, argv);

        Application application;

        return QApplication::exec();
    }
    catch (const std::exception& e)
    {
        qCritical() << "Unhandled exception:" << e.what();
        return 1;
    }
    catch (...)
    {
        qCritical() << "Unhandled unknown exception";
        return 1;
    }
}
