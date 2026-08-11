#pragma once

#include "types/scan_result.h"
#include "file_collector.h"
#include "scan_progress.h"

#include <QStringList>
#include <stop_token>

class ScanWorkflow
{
public:
    virtual ~ScanWorkflow() = default;

    [[nodiscard]] virtual ScanResult execute(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const ScanProgressCallback& progressCallback) const = 0;
};

class DuplicateGroupMetrics
{
public:
    void addFile(const FileRecord& file)
    {
        ++filesCount_;

        if (file.getSizeBytes() > 0)
        {
            totalBytes_ += static_cast<quint64>(file.getSizeBytes());
        }
    }

    [[nodiscard]] quint64 getFilesCount() const { return filesCount_; }
    [[nodiscard]] quint64 getTotalBytes() const { return totalBytes_; }

private:
    quint64 filesCount_{};
    quint64 totalBytes_{};
};

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

[[nodiscard]] inline ScanOutcome classifySuccessfulScan(const FileCollectionMetrics& collectionMetrics, const QList<DuplicateGroup>& duplicateGroups)
{
    if (collectionMetrics.getScannedFilesCount() == 0)
    {
        return ScanOutcome::NoFilesFound;
    }

    if (duplicateGroups.isEmpty())
    {
        return ScanOutcome::CompletedWithoutDuplicates;
    }

    return ScanOutcome::CompletedWithDuplicates;
}
