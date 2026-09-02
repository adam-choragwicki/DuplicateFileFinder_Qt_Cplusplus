#pragma once

#include "scan_summary/abstract_scan_summary.h"

/// @brief Concrete summary produced by a file-content-based scan.
///
/// In addition to the common metrics, this summary reports how many bytes could potentially be recovered by retaining
/// one reference file from each exact-content duplicate group and removing its additional copies.
class FileContentScanSummary final : public AbstractScanSummary
{
public:
    /// Initializes an empty file-content summary.
    FileContentScanSummary() = default;

    /// Initializes all common scan metrics and the content-specific recoverable-byte estimate.
    /// @param[in] completedAt UTC timestamp at which result construction completed.
    /// @param[in] duration Total elapsed workflow duration.
    /// @param[in] scannedDirectoriesCount Number of roots and descendant directories visited.
    /// @param[in] scannedFilesCount Number of readable files accepted for duplicate detection.
    /// @param[in] problematicFilesCount Number of files skipped because they could not be accessed reliably.
    /// @param[in] totalScannedBytes Combined size of all scanned files.
    /// @param[in] duplicateGroupsCount Number of exact-content duplicate groups produced.
    /// @param[in] totalFilesInDuplicateGroupsCount Combined file count across duplicate groups.
    /// @param[in] totalBytesOccupiedByFilesInDuplicateGroups Combined size of every file in duplicate groups.
    /// @param[in] totalAmountOfPotentiallyRecoverableBytes Size of every duplicate except one reference per group.
    FileContentScanSummary(
        const QDateTime& completedAt,
        const std::chrono::milliseconds duration,
        const quint64 scannedDirectoriesCount,
        const quint64 scannedFilesCount,
        const quint64 problematicFilesCount,
        const quint64 totalScannedBytes,
        const quint64 duplicateGroupsCount,
        const quint64 totalFilesInDuplicateGroupsCount,
        const quint64 totalBytesOccupiedByFilesInDuplicateGroups,
        const quint64 totalAmountOfPotentiallyRecoverableBytes)
        : AbstractScanSummary(
              completedAt,
              duration,
              scannedDirectoriesCount,
              scannedFilesCount,
              problematicFilesCount,
              totalScannedBytes,
              duplicateGroupsCount,
              totalFilesInDuplicateGroupsCount,
              totalBytesOccupiedByFilesInDuplicateGroups),
          totalAmountOfPotentiallyRecoverableBytes_(totalAmountOfPotentiallyRecoverableBytes)
    {}

    /// Returns the estimated bytes recoverable by retaining one file from each duplicate group.
    [[nodiscard]] quint64 getTotalAmountOfPotentiallyRecoverableBytes() const
    {
        return totalAmountOfPotentiallyRecoverableBytes_;
    }

private:
    quint64 totalAmountOfPotentiallyRecoverableBytes_{};
};
