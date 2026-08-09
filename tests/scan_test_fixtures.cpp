#include "scan_test_fixtures.h"

#include "scan_summary/scan_summary_logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

const ScanProgressCallback ScanWorkflowTest::ignoreProgressCallback = [](const ScanProgress&) {};

void ScanWorkflowTest::SetUp()
{
    ASSERT_TRUE(temporaryDirectory_.isValid());
}

bool ScanWorkflowTest::writeFile(const QString& relativePath, const QByteArray& contents) const
{
    const QString absolutePath = temporaryDirectory_.filePath(relativePath);

    if (!QDir().mkpath(QFileInfo(absolutePath).absolutePath()))
    {
        return false;
    }

    QFile file(absolutePath);

    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    return file.write(contents) == contents.size();
}

QString ScanWorkflowTest::getTemporaryScanRootPath() const
{
    return temporaryDirectory_.path();
}

QString ScanWorkflowTest::getSmokeTestScanRootPath()
{
    return QDir(QString::fromUtf8(FILE_SYSTEM_SCENARIOS_DIRECTORY)).filePath("smoke_test");
}

FileRecord ScanWorkflowTest::createExpectedFileRecord(const QString& scanRootPath, const QString& relativeFilePath, const qint64 expectedSizeBytes)
{
    const QFileInfo expectedFileInfo(QDir(scanRootPath).filePath(relativeFilePath));

    return {
        expectedFileInfo.fileName(),
        expectedFileInfo.absolutePath(),
        expectedSizeBytes
    };
}

DuplicateGroup ScanWorkflowTest::createExpectedDuplicateGroup(const std::initializer_list<FileRecord> expectedFiles)
{
    DuplicateGroup expectedDuplicateGroup;

    for (const FileRecord& expectedFile: expectedFiles)
    {
        expectedDuplicateGroup.addFile(expectedFile);
    }

    return expectedDuplicateGroup;
}

bool ScanWorkflowTest::containsDuplicateGroup(const ScanResult& scanResult, const DuplicateGroup& expectedDuplicateGroup)
{
    return std::any_of(
        scanResult.getDuplicateGroups().cbegin(),
        scanResult.getDuplicateGroups().cend(),
        [&expectedDuplicateGroup](const DuplicateGroup& actualDuplicateGroup)
        {
            return matchesExpectedDuplicateGroup(actualDuplicateGroup, expectedDuplicateGroup);
        });
}

bool ScanWorkflowTest::matchesExpectedDuplicateGroup(const DuplicateGroup& actualDuplicateGroup, const DuplicateGroup& expectedDuplicateGroup)
{
    QList<FileRecord> unmatchedActualFiles = actualDuplicateGroup.getFiles();
    const QList<FileRecord>& expectedFiles = expectedDuplicateGroup.getFiles();

    if (unmatchedActualFiles.size() != expectedFiles.size())
    {
        return false;
    }

    for (const FileRecord& expectedFile: expectedFiles)
    {
        const auto matchingActualFile = std::find_if(
            unmatchedActualFiles.cbegin(),
            unmatchedActualFiles.cend(),
            [&expectedFile](const FileRecord& actualFile)
            {
                return matchesExpectedFileRecord(actualFile, expectedFile);
            });

        if (matchingActualFile == unmatchedActualFiles.cend())
        {
            return false;
        }

        unmatchedActualFiles.erase(matchingActualFile);
    }

    return unmatchedActualFiles.isEmpty();
}

bool ScanWorkflowTest::matchesExpectedFileRecord(const FileRecord& actualFile, const FileRecord& expectedFile)
{
    return actualFile.getFileName() == expectedFile.getFileName()
           && actualFile.getDirectoryPath() == expectedFile.getDirectoryPath()
           && actualFile.getSizeBytes() == expectedFile.getSizeBytes();
}

ScanSummaryLoggerTest* ScanSummaryLoggerTest::activeFixture_ = nullptr;

void ScanSummaryLoggerTest::SetUp()
{
    activeFixture_ = this;
    previousMessageHandler_ = qInstallMessageHandler(captureMessage);
}

void ScanSummaryLoggerTest::TearDown()
{
    qInstallMessageHandler(previousMessageHandler_);
    activeFixture_ = nullptr;
}

QString ScanSummaryLoggerTest::captureLog(const ScanResult& scanResult)
{
    capturedMessages_.clear();
    ScanSummaryLogger::log(scanResult);
    return capturedMessages_.join('\n');
}

void ScanSummaryLoggerTest::captureMessage(
    const QtMsgType,
    const QMessageLogContext&,
    const QString& message)
{
    if (activeFixture_)
    {
        activeFixture_->capturedMessages_.append(message);
    }
}
