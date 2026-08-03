#include "file_name_scan_workflow.h"
#include "scan_summary/file_name_scan_summary.h"
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>

ScanResult FileNameScanWorkflow::execute(const QString& rootDirectoryPath, const std::stop_token& stopToken, const ScanProgressCallback& scanProgressCallback) const
{
    QElapsedTimer durationTimer;
    durationTimer.start();

    QList<DuplicateGroup> duplicateGroups;
    ScanOutcome outcome = ScanOutcome::Failed;
    QHash<QString, QList<FileRecord>> filesByName;
    quint64 enumeratedFilesCount = 0;

    qInfo() << "Started scan based on file name";

    scanProgressCallback({.phase = ScanPhase::EnumeratingFiles, .processedFilesCount = 0, .totalFilesCount = std::nullopt});

    // Stage 1: Collecting files and grouping them by name
    const FileCollectionResult fileCollectionResult = FileCollector::collectRecursively(
        rootDirectoryPath,
        stopToken,
        [&filesByName, &enumeratedFilesCount, &scanProgressCallback](FileRecord file)
        {
            // visitor collecting files and grouping them by name
            const QString fileName = file.getFileName();
            filesByName[fileName].append(std::move(file));
            ++enumeratedFilesCount;

            scanProgressCallback({.phase = ScanPhase::EnumeratingFiles, .processedFilesCount = enumeratedFilesCount, .totalFilesCount = std::nullopt});
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
        quint64 processedFilesCount = 0;
        const quint64 collectedFilesCount = fileCollectionResult.getMetrics().getScannedFilesCount();

        scanProgressCallback({.phase = ScanPhase::GroupingFilesByName, .processedFilesCount = 0, .totalFilesCount = collectedFilesCount});

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
                ++processedFilesCount;
                scanProgressCallback({.phase = ScanPhase::GroupingFilesByName, .processedFilesCount = processedFilesCount, .totalFilesCount = collectedFilesCount});
                continue;
            }

            DuplicateGroup duplicateGroup;

            for (const FileRecord& file: iterator.value())
            {
                duplicateGroup.addFile(file);
            }

            duplicateGroups.append(std::move(duplicateGroup));
            processedFilesCount += static_cast<quint64>(iterator.value().size());

            scanProgressCallback({.phase = ScanPhase::GroupingFilesByName, .processedFilesCount = processedFilesCount, .totalFilesCount = collectedFilesCount});
        }

        if (!stopToken.stop_requested())
        {
            outcome = classifySuccessfulScan(fileCollectionResult.getMetrics(), duplicateGroups);

            scanProgressCallback({.phase = ScanPhase::BuildingScanResult, .processedFilesCount = collectedFilesCount, .totalFilesCount = collectedFilesCount});
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
