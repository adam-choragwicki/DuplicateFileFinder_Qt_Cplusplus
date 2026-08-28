#pragma once

#include "scan_workflow.h"

#include <QByteArray>
#include <QHash>
#include <functional>
#include <optional>

class FileContentScanWorkflow final : public ScanWorkflow
{
public:
    using FileHashCalculator = std::function<std::optional<QByteArray> (const FileRecord&, const std::stop_token&)>;
    using FileSampleHashCalculator = FileHashCalculator;

    struct HashingConfiguration
    {
        FileHashCalculator fileHashCalculator_;
        FileSampleHashCalculator fileSampleHashCalculator_;
        qint64 fileSamplingThresholdBytes_;
    };

    FileContentScanWorkflow();
    explicit FileContentScanWorkflow(HashingConfiguration hashingConfiguration);

    [[nodiscard]] ScanResult execute(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const ScanProgressCallback& scanProgressCallback) const override;

private:
    // Sampling a 1 MiB prefix is worthwhile above this size while remaining negligible for true duplicates.
    static constexpr qint64 defaultFileSamplingThresholdBytes_ = 64 * 1024 * 1024;

    enum class ContentScanStageStatus
    {
        Completed,
        Cancelled,
        Failed
    };

    using FilesGroupedBySize = QHash<qint64, QList<FileRecord>>;
    using MatchingHashCandidateGroups = QList<QList<FileRecord>>;

    struct FileCollectionStageResult
    {
        const FileCollectionResult collectionResult_;
        const FilesGroupedBySize filesGroupedBySize_;
    };

    struct EqualSizeCandidateStageResult
    {
        const ContentScanStageStatus status_;
        const quint64 candidateFilesCount_;
    };

    struct SampleHashCandidateSelectionResult
    {
        const ContentScanStageStatus status_{};
        const QList<FileRecord> candidateFiles_;
    };

    struct MatchingHashCandidateStageResult
    {
        const ContentScanStageStatus status_{};
        const MatchingHashCandidateGroups candidateGroups_;
        const quint64 candidateFilesCount_{};
    };

    struct ContentVerificationStageResult
    {
        const ContentScanStageStatus status_{};
        const QList<DuplicateGroup> duplicateGroups_;
    };

    [[nodiscard]] static FileCollectionStageResult collectFilesGroupedBySize(const QStringList& rootDirectoryPaths,
                                                                             const std::stop_token& stopToken,
                                                                             const ScanProgressCallback& scanProgressCallback);

    [[nodiscard]] static EqualSizeCandidateStageResult identifyEqualSizeCandidates(const FilesGroupedBySize& filesGroupedBySize,
                                                                                   quint64 collectedFilesCount,
                                                                                   const std::stop_token& stopToken,
                                                                                   const ScanProgressCallback& scanProgressCallback);

    /// @brief Prefix-hashes one large equal-size group and selects files that still require full-file hashing.
    ///
    /// Files are grouped by their prefix hash. Files sharing a sample hash remain candidates, while a file with a
    /// unique sample is ruled out and counted as processed. Sample-group boundaries are not retained because all
    /// remaining files proceed through full hashing and exact byte comparison. A matching sample is only a
    /// prefilter result and does not establish that files have identical contents.
    ///
    /// @param equalSizeFiles Non-empty group of files known to have the same size.
    /// @param equalSizeCandidateFilesCount Total number of equal-size candidates in the hashing stage, used as the
    /// progress maximum.
    /// @param processedCandidateFilesCount Running hashing-stage counter. Incremented for files eliminated by a
    /// unique sample hash.
    /// @param stopToken Cancellation request observed before and during sampling.
    /// @param scanProgressCallback Receives progress after each sampled file is eliminated.
    /// @return Files whose sample hash is shared and `Completed`, or no files with `Cancelled` or `Failed` when
    /// sampling cannot finish.
    [[nodiscard]] SampleHashCandidateSelectionResult selectSampleHashCandidates(const QList<FileRecord>& equalSizeFiles,
                                                                                 quint64 equalSizeCandidateFilesCount,
                                                                                 quint64& processedCandidateFilesCount,
                                                                                 const std::stop_token& stopToken,
                                                                                 const ScanProgressCallback& scanProgressCallback) const;

    /// @brief Fully hashes files that could not be eliminated by size or prefix sampling.
    ///
    /// Every successfully hashed file advances the shared hashing-stage progress counter. The returned collection
    /// retains only full-hash groups containing at least two files; exact byte comparison is still required because
    /// equal cryptographic hashes are not treated as proof of identical contents.
    ///
    /// @param fullHashCandidates Equal-size files that require a complete hash calculation.
    /// @param equalSizeCandidateFilesCount Total number of equal-size candidates in the hashing stage, used as the
    /// progress maximum.
    /// @param processedCandidateFilesCount Running hashing-stage counter. Incremented after each successful
    /// full-file hash calculation.
    /// @param stopToken Cancellation request observed while hashing and grouping the results.
    /// @param scanProgressCallback Receives progress after each complete file is hashed.
    /// @return Matching full-hash groups, their combined file count, and `Completed`; returns `Cancelled` or
    /// `Failed` without candidate groups when full hashing cannot finish.
    [[nodiscard]] MatchingHashCandidateStageResult hashFullFileCandidates(const QList<FileRecord>& fullHashCandidates,
                                                                          quint64 equalSizeCandidateFilesCount,
                                                                          quint64& processedCandidateFilesCount,
                                                                          const std::stop_token& stopToken,
                                                                          const ScanProgressCallback& scanProgressCallback) const;

    [[nodiscard]] MatchingHashCandidateStageResult hashEqualSizeCandidates(const FilesGroupedBySize& filesGroupedBySize,
                                                                           quint64 equalSizeCandidateFilesCount,
                                                                           const std::stop_token& stopToken,
                                                                           const ScanProgressCallback& scanProgressCallback) const;

    [[nodiscard]] static ContentScanStageStatus assignCandidateToVerifiedContentGroup(const FileRecord& duplicateCandidateFile,
                                                                                      QList<DuplicateGroup>& contentVerifiedGroups,
                                                                                      const std::stop_token& stopToken);

    [[nodiscard]] static ContentVerificationStageResult verifyMatchingHashCandidates(const MatchingHashCandidateGroups& matchingHashCandidateGroups,
                                                                                     quint64 matchingHashCandidateFilesCount, const std::stop_token& stopToken,
                                                                                     const ScanProgressCallback& scanProgressCallback);

    [[nodiscard]] static quint64 calculatePotentiallyRecoverableBytes(const QList<DuplicateGroup>& duplicateGroups);

    [[nodiscard]] static ScanResult createFinalScanResult(QList<DuplicateGroup> duplicateGroups,
                                                          ScanOutcome outcome,
                                                          const FileCollectionMetrics& collectionMetrics,
                                                          qint64 elapsedMilliseconds);

    FileHashCalculator fileHashCalculator_;
    FileSampleHashCalculator fileSampleHashCalculator_;

    // Files at or above this threshold are sample-hashed first instead of being fully hashed immediately.
    // Only files with matching sample hashes proceed to full-file hashing.
    qint64 fileSamplingThresholdBytes_;
};
