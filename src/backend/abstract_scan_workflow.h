#pragma once

#include "file_tree_enumeration/file_tree_enumeration_metrics.h"
#include "scan_progress.h"
#include "types/scan_result.h"

#include <stop_token>

/// @brief Defines the common execution contract implemented by duplicate-detection workflows.
class AbstractScanWorkflow
{
public:
    /// Enables safe destruction through a workflow base pointer.
    virtual ~AbstractScanWorkflow() = default;

    /// Executes the workflow synchronously on the calling thread.
    /// @param[in] rootDirectoryPaths Root directory trees to scan.
    /// @param[in] stopToken Cancellation token observed throughout the workflow.
    /// @param[in] progressCallback Callback receiving progress updates as stages advance.
    /// @return Duplicate groups, final outcome, and a summary matching the concrete workflow.
    [[nodiscard]] virtual ScanResult execute(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const ScanProgressCallback& progressCallback) const = 0;
};

/// @brief Accumulates file and byte totals across a collection of duplicate groups.
class DuplicateGroupMetrics
{
public:
    /// Counts one file and adds its positive recorded size to the byte total.
    /// @param[in] file File record to include.
    void addFile(const FileRecord& file)
    {
        ++filesCount_;

        if (file.getSizeBytes() > 0)
        {
            totalBytes_ += static_cast<quint64>(file.getSizeBytes());
        }
    }

    /// Returns the number of files added, including empty files.
    [[nodiscard]] quint64 getFilesCount() const { return filesCount_; }
    /// Returns the sum of positive recorded file sizes.
    [[nodiscard]] quint64 getTotalBytes() const { return totalBytes_; }

private:
    quint64 filesCount_{};
    quint64 totalBytes_{};
};

/// Calculates combined file and byte totals for every record in the supplied groups.
/// @param[in] duplicateGroups Groups whose records should be counted.
/// @return Accumulated duplicate-group metrics.
[[nodiscard]] inline DuplicateGroupMetrics calculateDuplicateGroupMetrics(const QList<DuplicateGroup>& duplicateGroups)
{
    DuplicateGroupMetrics metrics;

    for (const DuplicateGroup& duplicateGroup: duplicateGroups)
    {
        for (const FileRecord& file: duplicateGroup.getFiles())
        {
            metrics.addFile(file);
        }
    }

    return metrics;
}

/// Classifies a successfully completed workflow from its enumeration and duplicate results.
/// @param[in] fileTreeEnumerationMetrics Enumeration metrics used to distinguish an empty scan from a unique-file scan.
/// @param[in] duplicateGroups Duplicate groups produced by the workflow.
/// @return NoFilesFound, CompletedWithoutDuplicates, or CompletedWithDuplicates.
[[nodiscard]] inline ScanOutcome classifySuccessfulScan(const FileTreeEnumerationMetrics& fileTreeEnumerationMetrics, const QList<DuplicateGroup>& duplicateGroups)
{
    if (fileTreeEnumerationMetrics.getScannedFilesCount() == 0)
    {
        return ScanOutcome::NoFilesFound;
    }

    if (duplicateGroups.isEmpty())
    {
        return ScanOutcome::CompletedWithoutDuplicates;
    }

    return ScanOutcome::CompletedWithDuplicates;
}
