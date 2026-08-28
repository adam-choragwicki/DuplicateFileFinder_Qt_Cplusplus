#include "development_configuration_helper.h"

#include <QCoreApplication>
#include <QDir>

#include <stdexcept>

QString DevelopmentConfigurationHelper::getInitialDirectoryScanPath()
{
    enum class TestType
    {
        SmokeTest,
        ResultPresentationTest,
        BigDirectoryTest
    };

    constexpr TestType testType = TestType::ResultPresentationTest; // Adjust the development scenario here.

    const QDir fileSystemScenariosDirectory(QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../tests/file_system_scenarios"))));

    switch (testType)
    {
        case TestType::SmokeTest:
            return fileSystemScenariosDirectory.filePath(QStringLiteral("smoke_test"));

        case TestType::ResultPresentationTest:
            return fileSystemScenariosDirectory.filePath(QStringLiteral("result_presentation_test"));

        case TestType::BigDirectoryTest:
            return QStringLiteral("C:/FULL_EXTERNAL_DISK"); // TODO remove eventually
    }

    throw std::runtime_error("Invalid development test type");
}
