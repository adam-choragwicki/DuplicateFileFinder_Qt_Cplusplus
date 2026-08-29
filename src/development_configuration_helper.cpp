#include "development_configuration_helper.h"

#include <QCoreApplication>
#include <QDir>

#include <stdexcept>

QStringList DevelopmentConfigurationHelper::getInitialDirectoryScanPaths()
{
    enum class DevelopmentScenario
    {
        None,
        SmokeTest,
        ResultPresentationTest,
        DemoTest,
        BigDirectoryTest // TODO remove before release
    };

    constexpr DevelopmentScenario testType = DevelopmentScenario::None; // Adjust the development scenario here.

    const QDir fileSystemScenariosDirectory(QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../tests/file_system_scenarios"))));

    switch (testType)
    {
        case DevelopmentScenario::None:
            return {};

        case DevelopmentScenario::SmokeTest:
            return {fileSystemScenariosDirectory.filePath(QStringLiteral("smoke_test"))};

        case DevelopmentScenario::ResultPresentationTest:
            return {fileSystemScenariosDirectory.filePath(QStringLiteral("result_presentation_test"))};

        case DevelopmentScenario::DemoTest:
        {
            const QDir demoScenarioDirectory(fileSystemScenariosDirectory.filePath(QStringLiteral("demo_test")));

            return {
                demoScenarioDirectory.filePath(QStringLiteral("Creative Studio")),
                demoScenarioDirectory.filePath(QStringLiteral("Product Team")),
                demoScenarioDirectory.filePath(QStringLiteral("Company Archive"))
            };
        }

        case DevelopmentScenario::BigDirectoryTest:
            return {QStringLiteral("C:/FULL_EXTERNAL_DISK")}; // TODO remove before release
    }

    throw std::runtime_error("Invalid development test type");
}
