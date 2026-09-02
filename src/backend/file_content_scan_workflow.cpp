#include "file_content_scan_workflow.h"
#include "file_comparator.h"
#include "file_hasher.h"
#include "file_tree_enumeration/file_tree_enumerator.h"
#include "scan_summary/file_content_scan_summary.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QtGlobal>

#include <utility>

FileContentScanWorkflow::FileContentScanWorkflow()
    : FileContentScanWorkflow(HashingConfiguration{.fileHashCalculator_ = &FileHasher::calculateFullFileHash,
                                                   .fileSampleHashCalculator_ = &FileHasher::calculateFileSampleHash,
                                                   .fileSamplingThresholdBytes_ = defaultFileSamplingThresholdBytes_})
{}

FileContentScanWorkflow::FileContentScanWorkflow(HashingConfiguration hashingConfiguration)
    : fileHashCalculator_(std::move(hashingConfiguration.fileHashCalculator_)),
      fileSampleHashCalculator_(std::move(hashingConfiguration.fileSampleHashCalculator_)),
      fileSamplingThresholdBytes_(hashingConfiguration.fileSamplingThresholdBytes_)
{
    Q_ASSERT(fileHashCalculator_ != nullptr);
    Q_ASSERT(fileSampleHashCalculator_ != nullptr);
    Q_ASSERT(fileSamplingThresholdBytes_ > 0);
}

ScanResult FileContentScanWorkflow::execute(const QStringList& rootDirectoryPaths,
                                            const std::stop_token& stopToken,
                                            const ScanProgressCallback& scanProgressCallback) const
{
    QElapsedTimer durationTimer;
    durationTimer.start();

    qInfo() << "Started scan based on file content";

    const FileCollectionStageResult collectedFiles = collectFilesGroupedBySize(rootDirectoryPaths, stopToken, scanProgressCallback);

    const FileTreeEnumerationMetrics& fileTreeEnumerationMetrics = collectedFiles.fileTreeEnumerationResult_.getMetrics();

    if (collectedFiles.fileTreeEnumerationResult_.getStatus() == FileTreeEnumerationStatus::InvalidRootDirectory)
    {
        return createFinalScanResult({}, ScanOutcome::Failed, fileTreeEnumerationMetrics, durationTimer.elapsed());
    }

    if (collectedFiles.fileTreeEnumerationResult_.getStatus() == FileTreeEnumerationStatus::Cancelled || stopToken.stop_requested())
    {
        return createFinalScanResult({}, ScanOutcome::Cancelled, fileTreeEnumerationMetrics, durationTimer.elapsed());
    }

    const EqualSizeCandidateStageResult equalSizeCandidates = identifyEqualSizeCandidates(collectedFiles.filesGroupedBySize_,
                                                                                          fileTreeEnumerationMetrics.getScannedFilesCount(),
                                                                                          stopToken,
                                                                                          scanProgressCallback);

    if (equalSizeCandidates.status_ == ContentScanStageStatus::Cancelled)
    {
        return createFinalScanResult({}, ScanOutcome::Cancelled, fileTreeEnumerationMetrics, durationTimer.elapsed());
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

        return createFinalScanResult({}, outcome, fileTreeEnumerationMetrics, durationTimer.elapsed());
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
                                     fileTreeEnumerationMetrics,
                                     durationTimer.elapsed());
    }

    const ScanOutcome outcome = classifySuccessfulScan(fileTreeEnumerationMetrics, verifiedDuplicates.duplicateGroups_);

    scanProgressCallback({
        .scanPhase = FileContentScanPhase::BuildingScanResult,
        .processedFilesCount = fileTreeEnumerationMetrics.getScannedFilesCount(),
        .totalFilesCount = fileTreeEnumerationMetrics.getScannedFilesCount()
    });

    return createFinalScanResult(verifiedDuplicates.duplicateGroups_,
                                 outcome,
                                 fileTreeEnumerationMetrics,
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

    const FileTreeEnumerationResult enumerationResult = FileTreeEnumerator::enumerateRecursively(rootDirectoryPaths,
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

    return {.fileTreeEnumerationResult_ = enumerationResult, .filesGroupedBySize_ = std::move(filesGroupedBySize)};
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

FileContentScanWorkflow::SampleHashCandidateSelectionResult FileContentScanWorkflow::selectSampleHashCandidates(const QList<FileRecord>& equalSizeFiles,
                                                                                                                const quint64 equalSizeCandidateFilesCount,
                                                                                                                quint64& processedCandidateFilesCount,
                                                                                                                const std::stop_token& stopToken,
                                                                                                                const ScanProgressCallback& scanProgressCallback) const
{
    QHash<QByteArray, QList<FileRecord>> filesGroupedBySampleHash;

    // Prefix hashes are only a prefilter: a unique value rules out an identical peer, while every matching value
    // remains inconclusive and proceeds to full-file hashing.
    for (const FileRecord& file: equalSizeFiles)
    {
        if (stopToken.stop_requested())
        {
            return {.status_ = ContentScanStageStatus::Cancelled, .candidateFiles_ = {}};
        }

        const std::optional<QByteArray> fileSampleHash = fileSampleHashCalculator_(file, stopToken);

        if (!fileSampleHash.has_value())
        {
            if (stopToken.stop_requested())
            {
                return {.status_ = ContentScanStageStatus::Cancelled, .candidateFiles_ = {}};
            }

            qWarning() << "Content scan cannot produce a complete result; sampling failed for:" << file.getAbsoluteFilePath();
            return {.status_ = ContentScanStageStatus::Failed, .candidateFiles_ = {}};
        }

        filesGroupedBySampleHash[*fileSampleHash].append(file);
    }

    QList<FileRecord> sampleHashCandidates;

    for (auto sampleHashGroupIterator = filesGroupedBySampleHash.begin(); sampleHashGroupIterator != filesGroupedBySampleHash.end(); ++sampleHashGroupIterator)
    {
        QList<FileRecord>& filesWithMatchingSampleHash = sampleHashGroupIterator.value();

        if (filesWithMatchingSampleHash.size() >= 2)
        {
            sampleHashCandidates.append(std::move(filesWithMatchingSampleHash));
            continue;
        }

        ++processedCandidateFilesCount;

        scanProgressCallback({
            .scanPhase = FileContentScanPhase::HashingDuplicateCandidateFiles,
            .processedFilesCount = processedCandidateFilesCount,
            .totalFilesCount = equalSizeCandidateFilesCount
        });

        if (stopToken.stop_requested())
        {
            return {.status_ = ContentScanStageStatus::Cancelled, .candidateFiles_ = {}};
        }
    }

    return {.status_ = ContentScanStageStatus::Completed, .candidateFiles_ = std::move(sampleHashCandidates)};
}

FileContentScanWorkflow::MatchingHashCandidateStageResult FileContentScanWorkflow::hashFullFileCandidates(const QList<FileRecord>& fullHashCandidates,
                                                                                                          const quint64 equalSizeCandidateFilesCount,
                                                                                                          quint64& processedCandidateFilesCount,
                                                                                                          const std::stop_token& stopToken,
                                                                                                          const ScanProgressCallback& scanProgressCallback) const
{
    QHash<QByteArray, QList<FileRecord>> filesGroupedByFullHash;

    for (const FileRecord& file: fullHashCandidates)
    {
        if (stopToken.stop_requested())
        {
            return {.status_ = ContentScanStageStatus::Cancelled};
        }

        const std::optional<QByteArray> fullFileHash = fileHashCalculator_(file, stopToken);

        if (!fullFileHash.has_value())
        {
            if (stopToken.stop_requested())
            {
                return {.status_ = ContentScanStageStatus::Cancelled};
            }

            qWarning() << "Content scan cannot produce a complete result; hashing failed for:" << file.getAbsoluteFilePath();
            return {.status_ = ContentScanStageStatus::Failed};
        }

        filesGroupedByFullHash[*fullFileHash].append(file);
        ++processedCandidateFilesCount;

        scanProgressCallback({
            .scanPhase = FileContentScanPhase::HashingDuplicateCandidateFiles,
            .processedFilesCount = processedCandidateFilesCount,
            .totalFilesCount = equalSizeCandidateFilesCount
        });
    }

    MatchingHashCandidateGroups matchingHashCandidateGroups;
    quint64 matchingHashCandidateFilesCount = 0;

    for (auto fullHashGroupIterator = filesGroupedByFullHash.cbegin(); fullHashGroupIterator != filesGroupedByFullHash.cend(); ++fullHashGroupIterator)
    {
        if (stopToken.stop_requested())
        {
            return {.status_ = ContentScanStageStatus::Cancelled};
        }

        // A full hash held by only one file cannot represent a duplicate group.
        if (fullHashGroupIterator.value().size() < 2)
        {
            continue;
        }

        matchingHashCandidateFilesCount += static_cast<quint64>(fullHashGroupIterator.value().size());
        matchingHashCandidateGroups.append(fullHashGroupIterator.value());
    }

    return {
        .status_ = ContentScanStageStatus::Completed,
        .candidateGroups_ = std::move(matchingHashCandidateGroups),
        .candidateFilesCount_ = matchingHashCandidateFilesCount
    };
}

FileContentScanWorkflow::MatchingHashCandidateStageResult FileContentScanWorkflow::hashEqualSizeCandidates(const FilesGroupedBySize& filesGroupedBySize,
                                                                                                           const quint64 equalSizeCandidateFilesCount,
                                                                                                           const std::stop_token& stopToken,
                                                                                                           const ScanProgressCallback& scanProgressCallback) const
{
    MatchingHashCandidateGroups matchingHashCandidateGroups;
    quint64 processedCandidateFilesCount = 0;
    quint64 matchingHashCandidateFilesCount = 0;

    // Every exit path preserves the matching-hash candidate groups collected before cancellation or failure.
    const auto finishScanStage = [&](const ContentScanStageStatus status)
    {
        return MatchingHashCandidateStageResult{
            .status_ = status,
            .candidateGroups_ = std::move(matchingHashCandidateGroups),
            .candidateFilesCount_ = matchingHashCandidateFilesCount
        };
    };

    scanProgressCallback({
        .scanPhase = FileContentScanPhase::HashingDuplicateCandidateFiles,
        .processedFilesCount = 0,
        .totalFilesCount = equalSizeCandidateFilesCount
    });

    for (auto sizeGroupIterator = filesGroupedBySize.cbegin(); sizeGroupIterator != filesGroupedBySize.cend(); ++sizeGroupIterator)
    {
        if (stopToken.stop_requested())
        {
            return finishScanStage(ContentScanStageStatus::Cancelled);
        }

        const QList<FileRecord>& equalSizeFiles = sizeGroupIterator.value();

        // A file with a unique size cannot have a duplicate, so do not read it at all.
        if (equalSizeFiles.size() < 2)
        {
            continue;
        }

        // Direct full-file hashing is the normal path. Only large files first pass through the sample-hash
        // prefilter, which removes candidates whose prefix hash is unique within their equal-size group.
        QList<FileRecord> filesToFullyHash = equalSizeFiles;

        if (equalSizeFiles.constFirst().getSizeBytes() >= fileSamplingThresholdBytes_)
        {
            const SampleHashCandidateSelectionResult sampleHashCandidates = selectSampleHashCandidates(equalSizeFiles,
                                                                                                       equalSizeCandidateFilesCount,
                                                                                                       processedCandidateFilesCount,
                                                                                                       stopToken,
                                                                                                       scanProgressCallback);

            if (sampleHashCandidates.status_ != ContentScanStageStatus::Completed)
            {
                return finishScanStage(sampleHashCandidates.status_);
            }

            filesToFullyHash = sampleHashCandidates.candidateFiles_;
        }

        const MatchingHashCandidateStageResult matchingHashCandidates = hashFullFileCandidates(filesToFullyHash,
                                                                                               equalSizeCandidateFilesCount,
                                                                                               processedCandidateFilesCount,
                                                                                               stopToken,
                                                                                               scanProgressCallback);

        if (matchingHashCandidates.status_ != ContentScanStageStatus::Completed)
        {
            return finishScanStage(matchingHashCandidates.status_);
        }

        matchingHashCandidateFilesCount += matchingHashCandidates.candidateFilesCount_;
        matchingHashCandidateGroups.append(matchingHashCandidates.candidateGroups_);
    }

    return finishScanStage(ContentScanStageStatus::Completed);
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
                                                          const FileTreeEnumerationMetrics& fileTreeEnumerationMetrics,
                                                          const qint64 elapsedMilliseconds)
{
    const DuplicateGroupMetrics duplicateMetrics = calculateDuplicateGroupMetrics(duplicateGroups);

    FileContentScanSummary summary{
        QDateTime::currentDateTimeUtc(),
        std::chrono::milliseconds(elapsedMilliseconds),
        fileTreeEnumerationMetrics.getScannedDirectoriesCount(),
        fileTreeEnumerationMetrics.getScannedFilesCount(),
        fileTreeEnumerationMetrics.getProblematicFilesCount(),
        fileTreeEnumerationMetrics.getTotalScannedBytes(),
        static_cast<quint64>(duplicateGroups.size()),
        duplicateMetrics.getFilesCount(),
        duplicateMetrics.getTotalBytes(),
        calculatePotentiallyRecoverableBytes(duplicateGroups)
    };

    return ScanResult(std::move(duplicateGroups), outcome, std::move(summary));
}
