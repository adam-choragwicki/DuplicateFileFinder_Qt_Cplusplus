#include "file_content_scan_workflow.h"
#include <QElapsedTimer>

ScanResult FileContentScanWorkflow::execute(const QString& rootDirectoryPath, const std::stop_token& stopToken) const
{
    QElapsedTimer durationTimer;
    durationTimer.start();

    QList<DuplicateGroup> duplicateGroups;
    ScanOutcome outcome = ScanOutcome::Failed;
    QHash<qint64, QList<FileRecord>> filesBySize;

    qInfo() << "File content scan stage 1 started: collecting files and grouping them by size";
    const FileCollectionResult collectionResult = FileCollector::collectRecursively(
        rootDirectoryPath,
        stopToken,
        [&filesBySize](FileRecord file)
        {
            // collect files and group them by size
            filesBySize[file.getSizeBytes()].append(std::move(file));
        });

    if (collectionResult.getStatus() == FileCollectionStatus::InvalidRootDirectory)
    {
        outcome = ScanOutcome::Failed;
    }
    else if (collectionResult.getStatus() == FileCollectionStatus::Cancelled || stopToken.stop_requested())
    {
        outcome = ScanOutcome::Cancelled;
    }
    else
    {
        qInfo() << "File content scan stage 2 started: hashing candidates and verifying matching hashes";

        bool fileAccessFailed = false;

        for (auto sizeIterator = filesBySize.cbegin(); sizeIterator != filesBySize.cend() && !fileAccessFailed; ++sizeIterator)
        {
            if (stopToken.stop_requested())
            {
                break;
            }

            const QList<FileRecord>& equalSizeFiles = sizeIterator.value();

            // A unique file size cannot have a duplicate, so do not perform any file I/O for it.
            if (equalSizeFiles.size() < 2)
            {
                continue;
            }

            QHash<QByteArray, QList<FileRecord>> filesByHash;

            for (const FileRecord& file: equalSizeFiles)
            {
                if (stopToken.stop_requested())
                {
                    break;
                }

                // TODO add hashing step
            }

            if (stopToken.stop_requested() || fileAccessFailed)
            {
                break;
            }

            // TODO add verification
        }

        if (stopToken.stop_requested())
        {
            outcome = ScanOutcome::Cancelled;
        }
        else if (fileAccessFailed)
        {
            outcome = ScanOutcome::Failed;
        }
        else
        {
            outcome = classifySuccessfulScan(collectionResult.getMetrics(), duplicateGroups);
        }
    }

    const DuplicateGroupMetrics duplicateMetrics = calculateDuplicateGroupMetrics(duplicateGroups);
    ScanSummary summary{
        QDateTime::currentDateTimeUtc(),
        std::chrono::milliseconds(durationTimer.elapsed()),
        collectionResult.getMetrics().getScannedDirectoriesCount(),
        collectionResult.getMetrics().getScannedFilesCount(),
        collectionResult.getMetrics().getTotalScannedBytes(),
        static_cast<quint64>(duplicateGroups.size()),
        duplicateMetrics.getFilesCount(),
        duplicateMetrics.getTotalBytes()
    };

    return ScanResult(std::move(duplicateGroups), outcome, std::move(summary));
}
