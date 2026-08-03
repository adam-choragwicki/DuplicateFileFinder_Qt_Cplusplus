#pragma once

#include <QDateTime>
#include <QtTypes>

#include <chrono>

class ScanSummary
{
public:
    ScanSummary() = default;

    ScanSummary(
        const QDateTime& completedAt,
        const std::chrono::milliseconds duration,
        const quint64 scannedDirectoriesCount,
        const quint64 scannedFilesCount,
        const quint64 totalScannedBytes,
        const quint64 duplicateGroupsCount,
        const quint64 totalFilesInDuplicateGroupsCount,
        const quint64 totalBytesOccupiedByFilesInDuplicateGroups)
        : completedAt_(completedAt),
          duration_(duration),
          scannedDirectoriesCount_(scannedDirectoriesCount),
          scannedFilesCount_(scannedFilesCount),
          totalScannedBytes_(totalScannedBytes),
          duplicateGroupsCount_(duplicateGroupsCount),
          totalFilesInDuplicateGroupsCount_(totalFilesInDuplicateGroupsCount),
          totalBytesOccupiedByFilesInDuplicateGroups_(totalBytesOccupiedByFilesInDuplicateGroups)
    {}

    [[nodiscard]] const QDateTime& getCompletedAt() const { return completedAt_; }
    [[nodiscard]] std::chrono::milliseconds getDuration() const { return duration_; }
    [[nodiscard]] quint64 getScannedDirectoriesCount() const { return scannedDirectoriesCount_; }
    [[nodiscard]] quint64 getScannedFilesCount() const { return scannedFilesCount_; }
    [[nodiscard]] quint64 getTotalScannedBytes() const { return totalScannedBytes_; }
    [[nodiscard]] quint64 getDuplicateGroupsCount() const { return duplicateGroupsCount_; }
    [[nodiscard]] quint64 getTotalFilesInDuplicateGroupsCount() const { return totalFilesInDuplicateGroupsCount_; }
    [[nodiscard]] quint64 getTotalBytesOccupiedByFilesInDuplicateGroups() const { return totalBytesOccupiedByFilesInDuplicateGroups_; }

private:
    QDateTime completedAt_;
    std::chrono::milliseconds duration_{};
    quint64 scannedDirectoriesCount_{};
    quint64 scannedFilesCount_{};
    quint64 totalScannedBytes_{};
    quint64 duplicateGroupsCount_{};
    quint64 totalFilesInDuplicateGroupsCount_{};
    quint64 totalBytesOccupiedByFilesInDuplicateGroups_{};
};
