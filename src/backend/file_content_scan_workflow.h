#pragma once

#include "file_tree_enumeration/file_tree_enumeration_result.h"
#include "abstract_scan_workflow.h"

#include <QByteArray>
#include <QHash>
#include <functional>
#include <optional>

/// @brief Finds exact-content duplicates through size filtering, hashing, and byte-for-byte verification.
///
/// File size eliminates impossible matches without reading contents. Large equal-size candidates may be prefix-hashed
/// before full hashing, and all matching full-hash candidates are compared byte by byte so hash collisions cannot
/// produce false duplicate groups.
class FileContentScanWorkflow final : public AbstractScanWorkflow
{
public:
    /// Hash calculator returning a digest, or std::nullopt after cancellation or failure.
    using FileHashCalculator = std::function<std::optional<QByteArray> (const FileRecord&, const std::stop_token&)>;
    /// Prefix-hash calculator used to prune large files before full hashing.
    using FileSampleHashCalculator = FileHashCalculator;

    /// Injectable hashing dependencies and sampling threshold, primarily used by focused workflow tests.
    struct HashingConfiguration
    {
        /// Calculator used to hash complete files.
        FileHashCalculator fileHashCalculator_;
        /// Calculator used to hash a prefix of large files.
        FileSampleHashCalculator fileSampleHashCalculator_;
        /// Minimum file size at which prefix sampling is attempted before full hashing.
        qint64 fileSamplingThresholdBytes_;
    };

    /// Creates a workflow using FileHasher and the production sampling threshold.
    FileContentScanWorkflow();
    /// Creates a workflow using the supplied hashing functions and threshold.
    /// @param[in] hashingConfiguration Non-null calculators and a positive sampling threshold.
    explicit FileContentScanWorkflow(HashingConfiguration hashingConfiguration);

    /// Executes every stage of an exact-content scan synchronously.
    /// @param[in] rootDirectoryPaths Root directory trees to scan.
    /// @param[in] stopToken Cancellation token observed throughout enumeration, hashing, and verification.
    /// @param[in] scanProgressCallback Callback receiving progress updates as stages advance.
    /// @return Exact duplicate groups, final outcome, and file-content scan summary.
    [[nodiscard]] ScanResult execute(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const ScanProgressCallback& scanProgressCallback) const override;

private:
    /// Files at or above this size are prefix-hashed before full hashing.
    static constexpr qint64 defaultFileSamplingThresholdBytes_ = 64 * 1024 * 1024;

    /// Internal terminal state shared by hashing and verification stages.
    enum class ContentScanStageStatus
    {
        /// The stage processed all of its inputs.
        Completed,
        /// A cancellation request stopped the stage.
        Cancelled,
        /// File access or a hashing/comparison operation failed.
        Failed
    };

    /// Enumerated files indexed by their recorded size in bytes.
    using FilesGroupedBySize = QHash<qint64, QList<FileRecord>>;
    /// Full-hash buckets that still require exact byte verification.
    using MatchingHashCandidateGroups = QList<QList<FileRecord>>;

    /// Output of file enumeration and simultaneous size grouping.
    struct FileCollectionStageResult
    {
        /// Enumeration status and metrics.
        const FileTreeEnumerationResult fileTreeEnumerationResult_;
        /// Successfully enumerated files grouped by size.
        const FilesGroupedBySize filesGroupedBySize_;
    };

    /// Output of counting files that belong to non-unique size groups.
    struct EqualSizeCandidateStageResult
    {
        /// Whether counting completed or was cancelled.
        const ContentScanStageStatus status_;
        /// Files in size groups containing at least two records.
        const quint64 candidateFilesCount_;
    };

    /// Output of prefix-hash pruning for one large equal-size group.
    struct SampleHashCandidateSelectionResult
    {
        /// Sampling stage outcome.
        const ContentScanStageStatus status_{};
        /// Files whose prefix hash was shared and therefore require full hashing.
        const QList<FileRecord> candidateFiles_;
    };

    /// Output of full hashing across all equal-size candidates.
    struct MatchingHashCandidateStageResult
    {
        /// Hashing stage outcome.
        const ContentScanStageStatus status_{};
        /// Full-hash buckets containing at least two files.
        const MatchingHashCandidateGroups candidateGroups_;
        /// Combined number of records in candidateGroups_.
        const quint64 candidateFilesCount_{};
    };

    /// Output of partitioning matching-hash buckets by exact file contents.
    struct ContentVerificationStageResult
    {
        /// Verification stage outcome.
        const ContentScanStageStatus status_{};
        /// Exact-content groups completed before the stage ended.
        const QList<DuplicateGroup> duplicateGroups_;
    };

    /// Enumerates readable files and groups each emitted record by its size.
    /// @param[in] rootDirectoryPaths Root directory trees to enumerate.
    /// @param[in] stopToken Cancellation token observed during enumeration.
    /// @param[in] scanProgressCallback Callback receiving indeterminate enumeration progress.
    /// @return Enumeration result and all successfully emitted files grouped by size.
    [[nodiscard]] static FileCollectionStageResult collectFilesGroupedBySize(const QStringList& rootDirectoryPaths,
                                                                             const std::stop_token& stopToken,
                                                                             const ScanProgressCallback& scanProgressCallback);

    /// Counts files requiring content hashing while reporting deterministic stage progress.
    /// @param[in] filesGroupedBySize All enumerated files grouped by size.
    /// @param[in] collectedFilesCount Total number of enumerated files used as the progress maximum.
    /// @param[in] stopToken Cancellation token observed between size groups.
    /// @param[in] scanProgressCallback Callback receiving candidate-identification progress.
    /// @return Candidate count and Completed, or the partial count and Cancelled.
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

    /// Applies optional prefix sampling and full hashing to every non-unique size group.
    /// @param[in] filesGroupedBySize All enumerated files grouped by size.
    /// @param[in] equalSizeCandidateFilesCount Exact hashing-stage progress maximum.
    /// @param[in] stopToken Cancellation token observed throughout hashing.
    /// @param[in] scanProgressCallback Callback receiving hashing progress.
    /// @return Matching full-hash groups accumulated before completion, cancellation, or failure.
    [[nodiscard]] MatchingHashCandidateStageResult hashEqualSizeCandidates(const FilesGroupedBySize& filesGroupedBySize,
                                                                           quint64 equalSizeCandidateFilesCount,
                                                                           const std::stop_token& stopToken,
                                                                           const ScanProgressCallback& scanProgressCallback) const;

    /// Places one matching-hash candidate into an existing exact-content group or starts a new collision partition.
    /// @param[in] duplicateCandidateFile Candidate to compare with each existing group's representative.
    /// @param[in,out] contentVerifiedGroups Exact-content partitions built for one full-hash bucket.
    /// @param[in] stopToken Cancellation token forwarded to byte comparison.
    /// @return Completed after placement, or the cancellation/failure returned by comparison.
    [[nodiscard]] static ContentScanStageStatus assignCandidateToVerifiedContentGroup(const FileRecord& duplicateCandidateFile,
                                                                                      QList<DuplicateGroup>& contentVerifiedGroups,
                                                                                      const std::stop_token& stopToken);

    /// Partitions every matching full-hash bucket by exact contents and removes collision-created singletons.
    /// @param[in] matchingHashCandidateGroups Full-hash buckets requiring byte verification.
    /// @param[in] matchingHashCandidateFilesCount Exact verification-stage progress maximum.
    /// @param[in] stopToken Cancellation token observed throughout verification.
    /// @param[in] scanProgressCallback Callback receiving byte-verification progress.
    /// @return Exact duplicate groups accumulated before completion, cancellation, or failure.
    [[nodiscard]] static ContentVerificationStageResult verifyMatchingHashCandidates(const MatchingHashCandidateGroups& matchingHashCandidateGroups,
                                                                                     quint64 matchingHashCandidateFilesCount, const std::stop_token& stopToken,
                                                                                     const ScanProgressCallback& scanProgressCallback);

    /// Sums the size of every file after the first reference record in each duplicate group.
    /// @param[in] duplicateGroups Exact-content groups used for the estimate.
    /// @return Potentially recoverable bytes when one file per group is retained.
    [[nodiscard]] static quint64 calculatePotentiallyRecoverableBytes(const QList<DuplicateGroup>& duplicateGroups);

    /// Builds the content-specific summary and final result from completed or partial workflow state.
    /// @param[in] duplicateGroups Exact groups to move into the result.
    /// @param[in] outcome Final workflow outcome.
    /// @param[in] fileTreeEnumerationMetrics Enumeration metrics to embed in the summary.
    /// @param[in] elapsedMilliseconds Total elapsed workflow time in milliseconds.
    /// @return Final file-content scan result.
    [[nodiscard]] static ScanResult createFinalScanResult(QList<DuplicateGroup> duplicateGroups,
                                                          ScanOutcome outcome,
                                                          const FileTreeEnumerationMetrics& fileTreeEnumerationMetrics,
                                                          qint64 elapsedMilliseconds);

    /// Complete-file hash implementation selected at construction.
    FileHashCalculator fileHashCalculator_;
    /// Prefix-hash implementation selected at construction.
    FileSampleHashCalculator fileSampleHashCalculator_;

    /// Files at or above this threshold are sample-hashed before matching samples proceed to full hashing.
    qint64 fileSamplingThresholdBytes_;
};
