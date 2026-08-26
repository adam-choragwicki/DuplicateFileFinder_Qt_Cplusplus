#include "file_content_scan_workflow.h"
#include "file_comparator.h"
#include "file_hasher.h"
#include "scan_summary/file_content_scan_summary.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QtGlobal>

#include <utility>

FileContentScanWorkflow::FileContentScanWorkflow()
    : fileHashCalculator_(&FileHasher::calculateFileHash) // dependency injection for easy hash collision testing
{}

FileContentScanWorkflow::FileContentScanWorkflow(const FileHashCalculator& fileHashCalculator)
    : fileHashCalculator_(fileHashCalculator)
{
    Q_ASSERT(fileHashCalculator_ != nullptr);
}

ScanResult FileContentScanWorkflow::execute(const QStringList& rootDirectoryPaths,
                                            const std::stop_token& stopToken,
                                            const ScanProgressCallback& scanProgressCallback) const
{
    QElapsedTimer durationTimer;
    durationTimer.start();

    qInfo() << "Started scan based on file content";

    const FileCollectionStageResult collectedFiles = collectFilesGroupedBySize(rootDirectoryPaths, stopToken, scanProgressCallback);

    const FileCollectionMetrics& fileCollectionMetrics = collectedFiles.collectionResult_.getMetrics();

    if (collectedFiles.collectionResult_.getStatus() == FileCollectionStatus::InvalidRootDirectory)
    {
        return createFinalScanResult({}, ScanOutcome::Failed, fileCollectionMetrics, durationTimer.elapsed());
    }

    if (collectedFiles.collectionResult_.getStatus() == FileCollectionStatus::Cancelled || stopToken.stop_requested())
    {
        return createFinalScanResult({}, ScanOutcome::Cancelled, fileCollectionMetrics, durationTimer.elapsed());
    }

    const EqualSizeCandidateStageResult equalSizeCandidates = identifyEqualSizeCandidates(collectedFiles.filesGroupedBySize_,
                                                                                          fileCollectionMetrics.getScannedFilesCount(),
                                                                                          stopToken,
                                                                                          scanProgressCallback);

    if (equalSizeCandidates.status_ == ContentScanStageStatus::Cancelled)
    {
        return createFinalScanResult({}, ScanOutcome::Cancelled, fileCollectionMetrics, durationTimer.elapsed());
    }

    const MatchingHashCandidateStageResult matchingHashCandidates = hashEqualSizeCandidates(collectedFiles.filesGroupedBySize_,
                                                                                            equalSizeCandidates.candidateFilesCount_,
                                                                                            stopToken,
                                                                                            scanProgressCallback);

    if (matchingHashCandidates.status_ != ContentScanStageStatus::Completed)
    {
        const ScanOutcome outcome = matchingHashCandidates.status_ == ContentScanStageStatus::Cancelled
                                        ? ScanOutcome::Cancelled
                                        : ScanOutcome::Failed;

        return createFinalScanResult({}, outcome, fileCollectionMetrics, durationTimer.elapsed());
    }

    ContentVerificationStageResult verifiedDuplicates = verifyMatchingHashCandidates(matchingHashCandidates.candidateGroups_,
                                                                                     matchingHashCandidates.candidateFilesCount_,
                                                                                     stopToken,
                                                                                     scanProgressCallback);

    if (verifiedDuplicates.status_ != ContentScanStageStatus::Completed)
    {
        const ScanOutcome outcome = verifiedDuplicates.status_ == ContentScanStageStatus::Cancelled
                                        ? ScanOutcome::Cancelled
                                        : ScanOutcome::Failed;

        return createFinalScanResult(verifiedDuplicates.duplicateGroups_,
                                     outcome,
                                     fileCollectionMetrics,
                                     durationTimer.elapsed());
    }

    const ScanOutcome outcome = classifySuccessfulScan(fileCollectionMetrics, verifiedDuplicates.duplicateGroups_);

    scanProgressCallback({
        .scanPhase = FileContentScanPhase::BuildingScanResult,
        .processedFilesCount = fileCollectionMetrics.getScannedFilesCount(),
        .totalFilesCount = fileCollectionMetrics.getScannedFilesCount()
    });

    return createFinalScanResult(verifiedDuplicates.duplicateGroups_,
                                 outcome,
                                 fileCollectionMetrics,
                                 durationTimer.elapsed());
}

FileContentScanWorkflow::FileCollectionStageResult FileContentScanWorkflow::collectFilesGroupedBySize(const QStringList& rootDirectoryPaths,
                                                                                                      const std::stop_token& stopToken,
                                                                                                      const ScanProgressCallback& scanProgressCallback)
{
    FilesGroupedBySize filesGroupedBySize;
    quint64 enumeratedFilesCount = 0;

    scanProgressCallback({
        .scanPhase = FileContentScanPhase::EnumeratingFiles,
        .processedFilesCount = 0,
        .totalFilesCount = std::nullopt
    });

    const FileCollectionResult collectionResult = FileCollector::collectRecursively(rootDirectoryPaths,
                                                                                    stopToken,
                                                                                    [&filesGroupedBySize, &enumeratedFilesCount, &scanProgressCallback](FileRecord file)
                                                                                    {
                                                                                        // visitor collecting files and grouping them by size
                                                                                        filesGroupedBySize[file.getSizeBytes()].append(std::move(file));
                                                                                        ++enumeratedFilesCount;

                                                                                        scanProgressCallback({
                                                                                            .scanPhase = FileContentScanPhase::EnumeratingFiles,
                                                                                            .processedFilesCount = enumeratedFilesCount,
                                                                                            .totalFilesCount = std::nullopt
                                                                                        });
                                                                                    });

    return {collectionResult, std::move(filesGroupedBySize)};
}

FileContentScanWorkflow::EqualSizeCandidateStageResult FileContentScanWorkflow::identifyEqualSizeCandidates(const FilesGroupedBySize& filesGroupedBySize,
                                                                                                            const quint64 collectedFilesCount,
                                                                                                            const std::stop_token& stopToken,
                                                                                                            const ScanProgressCallback& scanProgressCallback)
{
    quint64 candidateFilesCount = 0;
    quint64 checkedFilesCount = 0;

    scanProgressCallback({
        .scanPhase = FileContentScanPhase::IdentifyingEqualSizeCandidates,
        .processedFilesCount = 0,
        .totalFilesCount = collectedFilesCount
    });

    // Count the files that need hashing. This in-memory pass gives the next stage an exact progress total without performing any additional file I/O.
    for (auto sizeGroupIterator = filesGroupedBySize.cbegin(); sizeGroupIterator != filesGroupedBySize.cend(); ++sizeGroupIterator)
    {
        if (stopToken.stop_requested())
        {
            return {ContentScanStageStatus::Cancelled, candidateFilesCount};
        }

        const auto filesInSizeGroup = static_cast<quint64>(sizeGroupIterator.value().size());

        if (filesInSizeGroup >= 2)
        {
            candidateFilesCount += filesInSizeGroup;
        }

        checkedFilesCount += filesInSizeGroup;

        scanProgressCallback({
            .scanPhase = FileContentScanPhase::IdentifyingEqualSizeCandidates,
            .processedFilesCount = checkedFilesCount,
            .totalFilesCount = collectedFilesCount
        });
    }

    return {
        stopToken.stop_requested() ? ContentScanStageStatus::Cancelled : ContentScanStageStatus::Completed,
        candidateFilesCount
    };
}

FileContentScanWorkflow::MatchingHashCandidateStageResult FileContentScanWorkflow::hashEqualSizeCandidates(const FilesGroupedBySize& filesGroupedBySize,
                                                                                                           const quint64 equalSizeCandidateFilesCount,
                                                                                                           const std::stop_token& stopToken,
                                                                                                           const ScanProgressCallback& scanProgressCallback) const
{
    MatchingHashCandidateGroups matchingHashCandidateGroups;
    quint64 hashedCandidateFilesCount = 0;
    quint64 matchingHashCandidateFilesCount = 0;

    scanProgressCallback({
        .scanPhase = FileContentScanPhase::HashingDuplicateCandidateFiles,
        .processedFilesCount = 0,
        .totalFilesCount = equalSizeCandidateFilesCount
    });

    for (auto sizeGroupIterator = filesGroupedBySize.cbegin(); sizeGroupIterator != filesGroupedBySize.cend(); ++sizeGroupIterator)
    {
        if (stopToken.stop_requested())
        {
            return {
                .status_ = ContentScanStageStatus::Cancelled,
                .candidateGroups_ = std::move(matchingHashCandidateGroups),
                .candidateFilesCount_ = matchingHashCandidateFilesCount
            };
        }

        const QList<FileRecord>& equalSizeFiles = sizeGroupIterator.value();

        // A file with a unique size cannot have a duplicate, so do not read it at all.
        if (equalSizeFiles.size() < 2)
        {
            continue;
        }

        QHash<QByteArray, QList<FileRecord>> filesGroupedByHash;

        for (const FileRecord& file: equalSizeFiles)
        {
            if (stopToken.stop_requested())
            {
                return {
                    .status_ = ContentScanStageStatus::Cancelled,
                    .candidateGroups_ = std::move(matchingHashCandidateGroups),
                    .candidateFilesCount_ = matchingHashCandidateFilesCount
                };
            }

            const std::optional<QByteArray> fileHash = fileHashCalculator_(file, stopToken);

            if (!fileHash.has_value())
            {
                if (stopToken.stop_requested())
                {
                    return {
                        .status_ = ContentScanStageStatus::Cancelled,
                        .candidateGroups_ = std::move(matchingHashCandidateGroups),
                        .candidateFilesCount_ = matchingHashCandidateFilesCount
                    };
                }

                qWarning() << "Content scan cannot produce a complete result; hashing failed for:" << file.getAbsoluteFilePath();
                return {
                    .status_ = ContentScanStageStatus::Failed,
                    .candidateGroups_ = std::move(matchingHashCandidateGroups),
                    .candidateFilesCount_ = matchingHashCandidateFilesCount
                };
            }

            filesGroupedByHash[*fileHash].append(file);
            ++hashedCandidateFilesCount;

            scanProgressCallback({
                .scanPhase = FileContentScanPhase::HashingDuplicateCandidateFiles,
                .processedFilesCount = hashedCandidateFilesCount,
                .totalFilesCount = equalSizeCandidateFilesCount
            });
        }

        if (stopToken.stop_requested())
        {
            return {
                .status_ = ContentScanStageStatus::Cancelled,
                .candidateGroups_ = std::move(matchingHashCandidateGroups),
                .candidateFilesCount_ = matchingHashCandidateFilesCount
            };
        }

        for (auto hashGroupIterator = filesGroupedByHash.cbegin(); hashGroupIterator != filesGroupedByHash.cend(); ++hashGroupIterator)
        {
            if (stopToken.stop_requested())
            {
                return {
                    .status_ = ContentScanStageStatus::Cancelled,
                    .candidateGroups_ = std::move(matchingHashCandidateGroups),
                    .candidateFilesCount_ = matchingHashCandidateFilesCount
                };
            }

            // A hash held by only one file cannot represent a duplicate group.
            if (hashGroupIterator.value().size() < 2)
            {
                continue;
            }

            matchingHashCandidateFilesCount += static_cast<quint64>(hashGroupIterator.value().size());
            matchingHashCandidateGroups.append(hashGroupIterator.value());
        }
    }

    return {
        .status_ = ContentScanStageStatus::Completed,
        .candidateGroups_ = std::move(matchingHashCandidateGroups),
        .candidateFilesCount_ = matchingHashCandidateFilesCount
    };
}

FileContentScanWorkflow::ContentScanStageStatus FileContentScanWorkflow::assignCandidateToVerifiedContentGroup(const FileRecord& duplicateCandidateFile,
                                                                                                               QList<DuplicateGroup>& contentVerifiedGroups,
                                                                                                               const std::stop_token& stopToken)
{
    if (stopToken.stop_requested())
    {
        return ContentScanStageStatus::Cancelled;
    }

    // A colliding hash bucket may contain several different exact contents. Compare the
    // candidate with one representative from each group already proven byte-identical.
    for (DuplicateGroup& contentVerifiedGroup: contentVerifiedGroups)
    {
        const FileRecord& representativeFile = contentVerifiedGroup.getFiles().constFirst();
        const FileComparisonResult comparisonResult = FileComparator::compareFileContents(
            representativeFile,
            duplicateCandidateFile,
            stopToken);

        switch (comparisonResult)
        {
            case FileComparisonResult::Equal:
                contentVerifiedGroup.addFile(duplicateCandidateFile);
                return ContentScanStageStatus::Completed;

            case FileComparisonResult::Different:
                // Try the representative of the next exact-content group.
                continue;

            case FileComparisonResult::Cancelled:
                return ContentScanStageStatus::Cancelled;

            case FileComparisonResult::Failed:
                return ContentScanStageStatus::Failed;
        }
    }

    // No existing group had the same contents. This is either the bucket's first file or a
    // different file that collided on the hash, so it starts a new exact-content group.
    DuplicateGroup newContentGroup;
    newContentGroup.addFile(duplicateCandidateFile);
    contentVerifiedGroups.append(std::move(newContentGroup));

    return ContentScanStageStatus::Completed;
}

FileContentScanWorkflow::ContentVerificationStageResult FileContentScanWorkflow::verifyMatchingHashCandidates(const MatchingHashCandidateGroups& matchingHashCandidateGroups,
                                                                                                              const quint64 matchingHashCandidateFilesCount,
                                                                                                              const std::stop_token& stopToken,
                                                                                                              const ScanProgressCallback& scanProgressCallback)
{
    QList<DuplicateGroup> duplicateGroups;
    quint64 verifiedCandidateFilesCount = 0;

    scanProgressCallback({
        .scanPhase = FileContentScanPhase::VerifyingMatchingHashCandidates,
        .processedFilesCount = 0,
        .totalFilesCount = matchingHashCandidateFilesCount
    });

    for (const QList<FileRecord>& filesWithMatchingHash: matchingHashCandidateGroups)
    {
        if (stopToken.stop_requested())
        {
            return {.status_ = ContentScanStageStatus::Cancelled, .duplicateGroups_ = std::move(duplicateGroups)};
        }

        // Every group in this temporary partition contains files already proven identical.
        // A real hash collision can therefore produce multiple groups for one hash bucket.
        QList<DuplicateGroup> contentVerifiedGroups;

        for (const FileRecord& duplicateCandidateFile: filesWithMatchingHash)
        {
            const ContentScanStageStatus placementStatus = assignCandidateToVerifiedContentGroup(duplicateCandidateFile,
                                                                                                 contentVerifiedGroups,
                                                                                                 stopToken);

            if (placementStatus != ContentScanStageStatus::Completed)
            {
                return {placementStatus, std::move(duplicateGroups)};
            }

            ++verifiedCandidateFilesCount;

            scanProgressCallback({
                .scanPhase = FileContentScanPhase::VerifyingMatchingHashCandidates,
                .processedFilesCount = verifiedCandidateFilesCount,
                .totalFilesCount = matchingHashCandidateFilesCount
            });
        }

        if (stopToken.stop_requested())
        {
            return {.status_ = ContentScanStageStatus::Cancelled, .duplicateGroups_ = std::move(duplicateGroups)};
        }

        // Collision-created singleton groups are ordinary files, not duplicate groups.
        for (DuplicateGroup& contentVerifiedGroup: contentVerifiedGroups)
        {
            if (contentVerifiedGroup.getFiles().size() >= 2)
            {
                duplicateGroups.append(std::move(contentVerifiedGroup));
            }
        }
    }

    return {.status_ = ContentScanStageStatus::Completed, .duplicateGroups_ = std::move(duplicateGroups)};
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

ScanResult FileContentScanWorkflow::createFinalScanResult(QList<DuplicateGroup> duplicateGroups,
                                                          const ScanOutcome outcome,
                                                          const FileCollectionMetrics& collectionMetrics,
                                                          const qint64 elapsedMilliseconds)
{
    const DuplicateGroupMetrics duplicateMetrics = calculateDuplicateGroupMetrics(duplicateGroups);

    FileContentScanSummary summary{
        QDateTime::currentDateTimeUtc(),
        std::chrono::milliseconds(elapsedMilliseconds),
        collectionMetrics.getScannedDirectoriesCount(),
        collectionMetrics.getScannedFilesCount(),
        collectionMetrics.getProblematicFilesCount(),
        collectionMetrics.getTotalScannedBytes(),
        static_cast<quint64>(duplicateGroups.size()),
        duplicateMetrics.getFilesCount(),
        duplicateMetrics.getTotalBytes(),
        calculatePotentiallyRecoverableBytes(duplicateGroups)
    };

    return ScanResult(std::move(duplicateGroups), outcome, std::move(summary));
}
