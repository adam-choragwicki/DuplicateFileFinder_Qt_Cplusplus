#include "file_name_scan_workflow.h"

#include <QDateTime>
#include <QDebug>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>

ScanResult FileNameScanWorkflow::execute(const QString& rootDirectoryPath, const std::stop_token& stopToken) const
{
    ScanResult scanResult;
    FileCollectionMetrics fileCollectionMetrics;
    QElapsedTimer minimumDurationTimer;
    minimumDurationTimer.start();

    const QFileInfo rootDirectoryInfo(rootDirectoryPath);

    if (!rootDirectoryInfo.exists() || !rootDirectoryInfo.isDir() || !rootDirectoryInfo.isReadable())
    {
        qCritical() << "Cannot scan directory:" << rootDirectoryPath;
        scanResult.setOutcome(ScanOutcome::Failed);
    }
    else
    {
        // Stage 1 - collect files by name recursively
        qInfo() << "Scan stage 1 started: collecting files recursively";
        const QMap<QString, QList<FileRecord>> filesByName = collectFilesByNameRecursively(rootDirectoryPath, stopToken, fileCollectionMetrics);

        if (stopToken.stop_requested())
        {
            scanResult.setOutcome(ScanOutcome::Cancelled);
        }
        else
        {
            // Stage 2 - find which files have non-unique names and group them
            qInfo() << "Scan stage 2 started: grouping files with duplicate names";
            scanResult = findDuplicateGroupsByFileName(filesByName, stopToken);

            if (stopToken.stop_requested())
            {
                scanResult.setOutcome(ScanOutcome::Cancelled);
            }
            else if (fileCollectionMetrics.getScannedFilesCount() == 0)
            {
                scanResult.setOutcome(ScanOutcome::NoFilesFound);
            }
            else if (scanResult.getDuplicateGroups().isEmpty())
            {
                scanResult.setOutcome(ScanOutcome::CompletedWithoutDuplicates);
            }
            else
            {
                scanResult.setOutcome(ScanOutcome::CompletedWithDuplicates);
            }
        }
    }

    scanResult.setScanSummary(createScanSummary(fileCollectionMetrics, scanResult, std::chrono::milliseconds(minimumDurationTimer.elapsed())));

    return scanResult;
}

ScanResult FileNameScanWorkflow::findDuplicateGroupsByFileName(const QMap<QString, QList<FileRecord>>& filesByName, const std::stop_token& stopToken)
{
    ScanResult scanResult;

    for (auto iterator = filesByName.cbegin(); iterator != filesByName.cend(); ++iterator)
    {
        if (stopToken.stop_requested())
        {
            scanResult.setOutcome(ScanOutcome::Cancelled);
            return scanResult;
        }

        if (iterator.value().size() < 2)
        {
            continue;
        }

        DuplicateGroup duplicateGroup;

        for (const FileRecord& file: iterator.value())
        {
            duplicateGroup.addFile(file);
        }

        scanResult.appendDuplicateGroup(duplicateGroup);
    }

    return scanResult;
}

ScanSummary FileNameScanWorkflow::createScanSummary(const FileCollectionMetrics& collectionMetrics, const ScanResult& scanResult, const std::chrono::milliseconds duration)
{
    quint64 totalFilesInDuplicateGroupsCount = 0;
    quint64 totalBytesOccupiedByFilesInDuplicateGroups = 0;

    for (const DuplicateGroup& duplicateGroup: scanResult.getDuplicateGroups())
    {
        totalFilesInDuplicateGroupsCount += static_cast<quint64>(duplicateGroup.getFiles().size());

        for (const FileRecord& file: duplicateGroup.getFiles())
        {
            totalBytesOccupiedByFilesInDuplicateGroups += static_cast<quint64>(file.getSizeBytes());
        }
    }

    // Duplicate filenames do not prove identical content, so no bytes are considered safely recoverable in this scan mode.
    constexpr quint64 totalAmountOfPotentiallyRecoverableBytes = 0;

    return ScanSummary{
        QDateTime::currentDateTimeUtc(),
        duration,
        collectionMetrics.getScannedDirectoriesCount(),
        collectionMetrics.getScannedFilesCount(),
        collectionMetrics.getTotalScannedBytes(),
        static_cast<quint64>(scanResult.getDuplicateGroups().size()),
        totalFilesInDuplicateGroupsCount,
        totalBytesOccupiedByFilesInDuplicateGroups,
        totalAmountOfPotentiallyRecoverableBytes
    };
}

QMap<QString, QList<FileRecord>> FileNameScanWorkflow::collectFilesByNameRecursively(const QString& rootDirectoryPath, const std::stop_token& stopToken, FileCollectionMetrics& fileCollectionMetrics)
{
    QMap<QString, QList<FileRecord>> filesByName;
    const QDir rootDirectory(rootDirectoryPath);

    if (rootDirectory.exists())
    {
        fileCollectionMetrics.incrementScannedDirectoriesCount();
    }

    QDirIterator iterator(rootDirectoryPath,
                          QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);

    while (iterator.hasNext())
    {
        if (stopToken.stop_requested())
        {
            break;
        }

        iterator.next();
        const QFileInfo fileInfo = iterator.fileInfo();

        if (fileInfo.isDir())
        {
            fileCollectionMetrics.incrementScannedDirectoriesCount();
            continue;
        }

        if (!fileInfo.isFile())
        {
            continue;
        }

        fileCollectionMetrics.incrementScannedFilesCount();
        fileCollectionMetrics.addToTotalScannedBytes(fileInfo.size());
        filesByName[fileInfo.fileName()].append(FileRecord(fileInfo.fileName(), fileInfo.absolutePath(), fileInfo.size()));
    }

    return filesByName;
}
