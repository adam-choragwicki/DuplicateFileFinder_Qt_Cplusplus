#include "file_content_scan_workflow.h"
#include "file_hasher.h"
#include "scan_summary/file_content_scan_summary.h"
#include <QElapsedTimer>

ScanResult FileContentScanWorkflow::execute(const QString& rootDirectoryPath, const std::stop_token& stopToken, const ScanProgressCallback& scanProgressCallback) const
{
    QElapsedTimer durationTimer;
    durationTimer.start();

    QList<DuplicateGroup> duplicateGroups;
    ScanOutcome outcome = ScanOutcome::Failed;
    QHash<qint64, QList<FileRecord>> filesBySize;
    quint64 enumeratedFilesCount = 0;

    qInfo() << "Started scan based on file content";

    scanProgressCallback({.phase = FileContentScanPhase::EnumeratingFiles, .processedFilesCount = 0, .totalFilesCount = std::nullopt});

    // Stage 1: Collecting files and grouping them by size
    const FileCollectionResult collectionResult = FileCollector::collectRecursively(
        rootDirectoryPath,
        stopToken,
        [&filesBySize, &enumeratedFilesCount, &scanProgressCallback](FileRecord file)
        {
            // visitor collecting files and grouping them by size
            filesBySize[file.getSizeBytes()].append(std::move(file));
            ++enumeratedFilesCount;

            scanProgressCallback({FileContentScanPhase::EnumeratingFiles, enumeratedFilesCount, std::nullopt});
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
        quint64 duplicateCandidateFilesCount = 0;
        quint64 filesCheckedBySizeCount = 0;
        const quint64 collectedFilesCount = collectionResult.getMetrics().getScannedFilesCount();

        scanProgressCallback({.phase = FileContentScanPhase::IdentifyingEqualSizeCandidates, .processedFilesCount = 0, .totalFilesCount = collectedFilesCount});

        // Count the files that need hashing first. This is an in-memory pass over the size groups,
        // so it gives the hashing phase an exact total without performing any additional file I/O.
        for (auto sizeIterator = filesBySize.cbegin(); sizeIterator != filesBySize.cend(); ++sizeIterator)
        {
            if (stopToken.stop_requested())
            {
                break;
            }

            const auto filesInSizeGroup = static_cast<quint64>(sizeIterator.value().size());

            if (filesInSizeGroup >= 2)
            {
                duplicateCandidateFilesCount += filesInSizeGroup;
            }

            filesCheckedBySizeCount += filesInSizeGroup;

            scanProgressCallback({.phase = FileContentScanPhase::IdentifyingEqualSizeCandidates, .processedFilesCount = filesCheckedBySizeCount, .totalFilesCount = collectedFilesCount});
        }

        quint64 hashedCandidateFilesCount = 0;

        if (!stopToken.stop_requested() && scanProgressCallback)
        {
            scanProgressCallback({.phase = FileContentScanPhase::HashingDuplicateCandidateFiles, .processedFilesCount = 0, .totalFilesCount = duplicateCandidateFilesCount});
        }

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
                ++hashedCandidateFilesCount;

                scanProgressCallback({.phase = FileContentScanPhase::HashingDuplicateCandidateFiles, .processedFilesCount = hashedCandidateFilesCount, .totalFilesCount = duplicateCandidateFilesCount});
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

            scanProgressCallback({.phase = FileContentScanPhase::BuildingScanResult, .processedFilesCount = collectedFilesCount, .totalFilesCount = collectedFilesCount});
        }
    }

    const DuplicateGroupMetrics duplicateMetrics = calculateDuplicateGroupMetrics(duplicateGroups);

    FileContentScanSummary summary{
        QDateTime::currentDateTimeUtc(),
        std::chrono::milliseconds(durationTimer.elapsed()),
        collectionResult.getMetrics().getScannedDirectoriesCount(),
        collectionResult.getMetrics().getScannedFilesCount(),
        collectionResult.getMetrics().getTotalScannedBytes(),
        static_cast<quint64>(duplicateGroups.size()),
        duplicateMetrics.getFilesCount(),
        duplicateMetrics.getTotalBytes(),
        calculatePotentiallyRecoverableBytes(duplicateGroups)
    };

    return ScanResult(std::move(duplicateGroups), outcome, std::move(summary));
}

quint64 FileContentScanWorkflow::calculatePotentiallyRecoverableBytes(const QList<DuplicateGroup>& duplicateGroups)
{
    quint64 recoverableBytes = 0;

    for (const DuplicateGroup& duplicateGroup: duplicateGroups)
    {
        const QList<FileRecord>& files = duplicateGroup.getFiles();

        // Keep one file from every exact-content group; every additional file is potentially recoverable.
        for (qsizetype fileIndex = 1; fileIndex < files.size(); ++fileIndex)
        {
            if (files.at(fileIndex).getSizeBytes() > 0)
            {
                recoverableBytes += static_cast<quint64>(files.at(fileIndex).getSizeBytes());
            }
        }
    }

    return recoverableBytes;
}
