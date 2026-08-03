#pragma once

#include "scan_summary/scan_summary.h"

class FileContentScanSummary final : public ScanSummary
{
public:
    FileContentScanSummary() = default;

    FileContentScanSummary(
        const QDateTime& completedAt,
        const std::chrono::milliseconds duration,
        const quint64 scannedDirectoriesCount,
        const quint64 scannedFilesCount,
        const quint64 totalScannedBytes,
        const quint64 duplicateGroupsCount,
        const quint64 totalFilesInDuplicateGroupsCount,
        const quint64 totalBytesOccupiedByFilesInDuplicateGroups,
        const quint64 totalAmountOfPotentiallyRecoverableBytes)
        : ScanSummary(
              completedAt,
              duration,
              scannedDirectoriesCount,
              scannedFilesCount,
              totalScannedBytes,
              duplicateGroupsCount,
              totalFilesInDuplicateGroupsCount,
              totalBytesOccupiedByFilesInDuplicateGroups),
          totalAmountOfPotentiallyRecoverableBytes_(totalAmountOfPotentiallyRecoverableBytes)
    {}

    [[nodiscard]] quint64 getTotalAmountOfPotentiallyRecoverableBytes() const
    {
        return totalAmountOfPotentiallyRecoverableBytes_;
    }

private:
    quint64 totalAmountOfPotentiallyRecoverableBytes_{};
};
