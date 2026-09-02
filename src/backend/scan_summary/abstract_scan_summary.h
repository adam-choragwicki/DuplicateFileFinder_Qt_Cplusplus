#pragma once

#include <QDateTime>
#include <QtTypes>

#include <chrono>

/// @brief Defines the common timing, enumeration, and duplicate metrics stored by every scan summary.
///
/// The pure virtual destructor prevents creation of a workflow-agnostic summary. Results must contain either a
/// FileNameScanSummary or FileContentScanSummary, while consumers can use this base interface for shared fields.
class AbstractScanSummary
{
public:
    /// Initializes every common field to its empty value for default concrete summaries.
    AbstractScanSummary() = default;
    /// Makes the common summary schema abstract and supports safe destruction through a base pointer.
    virtual ~AbstractScanSummary() = 0;

    /// Initializes all metrics shared by the concrete scan-summary types.
    /// @param[in] completedAt UTC timestamp at which result construction completed.
    /// @param[in] duration Total elapsed workflow duration.
    /// @param[in] scannedDirectoriesCount Number of roots and descendant directories visited.
    /// @param[in] scannedFilesCount Number of readable files accepted for duplicate detection.
    /// @param[in] problematicFilesCount Number of files skipped because they could not be accessed reliably.
    /// @param[in] totalScannedBytes Combined size of all scanned files.
    /// @param[in] duplicateGroupsCount Number of duplicate groups produced.
    /// @param[in] totalFilesInDuplicateGroupsCount Combined file count across duplicate groups.
    /// @param[in] totalBytesOccupiedByFilesInDuplicateGroups Combined size of every file in duplicate groups.
    AbstractScanSummary(const QDateTime& completedAt,
                        const std::chrono::milliseconds duration,
                        const quint64 scannedDirectoriesCount,
                        const quint64 scannedFilesCount,
                        const quint64 problematicFilesCount,
                        const quint64 totalScannedBytes,
                        const quint64 duplicateGroupsCount,
                        const quint64 totalFilesInDuplicateGroupsCount,
                        const quint64 totalBytesOccupiedByFilesInDuplicateGroups)
        : completedAt_(completedAt),
          duration_(duration),
          scannedDirectoriesCount_(scannedDirectoriesCount),
          scannedFilesCount_(scannedFilesCount),
          problematicFilesCount_(problematicFilesCount),
          totalScannedBytes_(totalScannedBytes),
          duplicateGroupsCount_(duplicateGroupsCount),
          totalFilesInDuplicateGroupsCount_(totalFilesInDuplicateGroupsCount),
          totalBytesOccupiedByFilesInDuplicateGroups_(totalBytesOccupiedByFilesInDuplicateGroups)
    {}

    /// Returns the UTC completion timestamp.
    [[nodiscard]] const QDateTime& getCompletedAt() const { return completedAt_; }
    /// Returns total elapsed workflow time.
    [[nodiscard]] std::chrono::milliseconds getDuration() const { return duration_; }
    /// Returns the number of roots and descendant directories visited.
    [[nodiscard]] quint64 getScannedDirectoriesCount() const { return scannedDirectoriesCount_; }
    /// Returns the number of readable files accepted for duplicate detection.
    [[nodiscard]] quint64 getScannedFilesCount() const { return scannedFilesCount_; }
    /// Returns the number of files skipped because they could not be accessed reliably.
    [[nodiscard]] quint64 getProblematicFilesCount() const { return problematicFilesCount_; }
    /// Returns the combined size of all scanned files.
    [[nodiscard]] quint64 getTotalScannedBytes() const { return totalScannedBytes_; }
    /// Returns the number of duplicate groups produced.
    [[nodiscard]] quint64 getDuplicateGroupsCount() const { return duplicateGroupsCount_; }
    /// Returns the combined file count across duplicate groups.
    [[nodiscard]] quint64 getTotalFilesInDuplicateGroupsCount() const { return totalFilesInDuplicateGroupsCount_; }
    /// Returns the combined size of every file in duplicate groups.
    [[nodiscard]] quint64 getTotalBytesOccupiedByFilesInDuplicateGroups() const { return totalBytesOccupiedByFilesInDuplicateGroups_; }

private:
    QDateTime completedAt_;
    std::chrono::milliseconds duration_{};
    quint64 scannedDirectoriesCount_{};
    quint64 scannedFilesCount_{};
    quint64 problematicFilesCount_{};
    quint64 totalScannedBytes_{};
    quint64 duplicateGroupsCount_{};
    quint64 totalFilesInDuplicateGroupsCount_{};
    quint64 totalBytesOccupiedByFilesInDuplicateGroups_{};
};

/// Provides the required definition for the pure virtual destructor.
inline AbstractScanSummary::~AbstractScanSummary() = default;
