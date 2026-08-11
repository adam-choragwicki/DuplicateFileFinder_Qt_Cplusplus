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

    FileContentScanWorkflow();
    explicit FileContentScanWorkflow(const FileHashCalculator& fileHashCalculator);

    [[nodiscard]] ScanResult execute(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const ScanProgressCallback& scanProgressCallback) const override;

private:
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
};
