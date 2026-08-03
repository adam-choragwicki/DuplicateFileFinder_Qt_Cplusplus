#include "file_name_scan_workflow.h"
#include "scan_summary/file_name_scan_summary.h"
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>

ScanResult FileNameScanWorkflow::execute(const QString& rootDirectoryPath, const std::stop_token& stopToken) const
{
    QElapsedTimer durationTimer;
    durationTimer.start();

    QList<DuplicateGroup> duplicateGroups;
    ScanOutcome outcome = ScanOutcome::Failed;
    QHash<QString, QList<FileRecord>> filesByName;

    qInfo() << "Started scan based on file name";

    // Stage 1: Collecting files and grouping them by name
    const FileCollectionResult fileCollectionResult = FileCollector::collectRecursively(
        rootDirectoryPath,
        stopToken,
        [&filesByName](FileRecord file)
        {
            // visitor collecting files and grouping them by name
            const QString fileName = file.getFileName();
            filesByName[fileName].append(std::move(file));
        });

    if (fileCollectionResult.getStatus() == FileCollectionStatus::InvalidRootDirectory)
    {
        outcome = ScanOutcome::Failed;
    }
    else if (fileCollectionResult.getStatus() == FileCollectionStatus::Cancelled || stopToken.stop_requested())
    {
        outcome = ScanOutcome::Cancelled;
    }
    else
    {
        // Stage 2: Grouping files with duplicate names
        for (auto iterator = filesByName.cbegin(); iterator != filesByName.cend(); ++iterator)
        {
            if (stopToken.stop_requested())
            {
                outcome = ScanOutcome::Cancelled;
                break;
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

            duplicateGroups.append(std::move(duplicateGroup));
        }

        if (!stopToken.stop_requested())
        {
            outcome = classifySuccessfulScan(fileCollectionResult.getMetrics(), duplicateGroups);
        }
    }

    const DuplicateGroupMetrics duplicateMetrics = calculateDuplicateGroupMetrics(duplicateGroups);

    FileNameScanSummary summary{
        QDateTime::currentDateTimeUtc(),
        std::chrono::milliseconds(durationTimer.elapsed()),
        fileCollectionResult.getMetrics().getScannedDirectoriesCount(),
        fileCollectionResult.getMetrics().getScannedFilesCount(),
        fileCollectionResult.getMetrics().getTotalScannedBytes(),
        static_cast<quint64>(duplicateGroups.size()),
        duplicateMetrics.getFilesCount(),
        duplicateMetrics.getTotalBytes()
    };

    return ScanResult(std::move(duplicateGroups), outcome, std::move(summary));
}
