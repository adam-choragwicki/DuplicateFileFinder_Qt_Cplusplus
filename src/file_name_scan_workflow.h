#pragma once

#include "scan_workflow.h"

class FileCollectionMetrics
{
public:
    void incrementScannedDirectoriesCount() { ++scannedDirectoriesCount_; }
    void incrementScannedFilesCount() { ++scannedFilesCount_; }

    void addToTotalScannedBytes(const qint64 bytes)
    {
        if (bytes > 0)
        {
            totalScannedBytes_ += static_cast<quint64>(bytes);
        }
    }

    [[nodiscard]] quint64 getScannedDirectoriesCount() const { return scannedDirectoriesCount_; }
    [[nodiscard]] quint64 getScannedFilesCount() const { return scannedFilesCount_; }
    [[nodiscard]] quint64 getTotalScannedBytes() const { return totalScannedBytes_; }

private:
    quint64 scannedDirectoriesCount_{};
    quint64 scannedFilesCount_{};
    quint64 totalScannedBytes_{};
};

class FileNameScanWorkflow final : public ScanWorkflow
{
public:
    [[nodiscard]] ScanResult execute(const QString& rootDirectoryPath, const std::stop_token& stopToken) const override;

    [[nodiscard]] static QMap<QString, QList<FileRecord>> collectFilesByNameRecursively(const QString& rootDirectoryPath, const std::stop_token& stopToken, FileCollectionMetrics& fileCollectionMetrics);
    [[nodiscard]] static ScanResult findDuplicateGroupsByFileName(const QMap<QString, QList<FileRecord>>& filesByName, const std::stop_token& stopToken);
    [[nodiscard]] static ScanSummary createScanSummary(const FileCollectionMetrics& collectionMetrics, const ScanResult& scanResult, std::chrono::milliseconds duration);
};
