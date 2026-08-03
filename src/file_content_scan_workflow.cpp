#include "file_content_scan_workflow.h"
#include "file_hasher.h"
#include <QElapsedTimer>

ScanResult FileContentScanWorkflow::execute(const QString& rootDirectoryPath, const std::stop_token& stopToken) const
{
    QElapsedTimer durationTimer;
    durationTimer.start();

    QList<DuplicateGroup> duplicateGroups;
    ScanOutcome outcome = ScanOutcome::Failed;
    QHash<qint64, QList<FileRecord>> filesBySize;

    qInfo() << "Started scan based on file content";

    // Stage 1: Collecting files and grouping them by size
    const FileCollectionResult collectionResult = FileCollector::collectRecursively(
        rootDirectoryPath,
        stopToken,
        [&filesBySize](FileRecord file)
        {
            // visitor collecting files and grouping them by size
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
        bool fileAccessFailed = false;

        for (auto sizeIterator = filesBySize.cbegin(); sizeIterator != filesBySize.cend() && !fileAccessFailed; ++sizeIterator)
        {
            if (stopToken.stop_requested())
            {
                break;
            }

            const QList<FileRecord>& equalSizeFiles = sizeIterator.value();

            // Stage 2: Discard unique-size groups (1-element lists). A unique file size cannot have a duplicate, so do not perform any file I/O for it.
            if (equalSizeFiles.size() < 2)
            {
                continue;
            }

            // Stage 3 hash candidate files and group the files by hash
            QHash<QByteArray, QList<FileRecord>> filesByHash;

            for (const FileRecord& file: equalSizeFiles)
            {
                if (stopToken.stop_requested())
                {
                    break;
                }

                const std::optional<QByteArray> fileHashValue = FileHasher::calculateFileHash(file, stopToken);

                if (!fileHashValue.has_value())
                {
                    if (!stopToken.stop_requested())
                    {
                        qWarning() << "Content scan cannot produce a complete result; hashing failed for:" << file.getAbsoluteFilePath();
                        fileAccessFailed = true;
                    }

                    break;
                }

                filesByHash[*fileHashValue].append(file);
            }

            if (stopToken.stop_requested() || fileAccessFailed)
            {
                break;
            }

            for (auto hashIterator = filesByHash.cbegin(); hashIterator != filesByHash.cend(); ++hashIterator)
            {
                // Stage 4: Discard unique hash groups
                if (hashIterator.value().size() < 2)
                {
                    continue;
                }

                const QList<FileRecord>& filesWithMatchingHash = hashIterator.value();

                // assume files with matching hash are always identical
                // TODO what in case of very unlikely cryptographic hash collision?
                // TODO add byte-by-byte file comparison

                // Stage 5: Finalize duplicate groups
                DuplicateGroup duplicateGroup;

                for (const FileRecord& file: filesWithMatchingHash)
                {
                    duplicateGroup.addFile(file);
                }

                duplicateGroups.append(duplicateGroup);
            }
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
