#pragma once

#include "types/file_record.h"

#include <QString>
#include <QtTypes>

#include <functional>
#include <stop_token>

class FileCollectionMetrics
{
public:
    void incrementScannedDirectoriesCount() { ++scannedDirectoriesCount_; }
    void incrementScannedFilesCount() { ++scannedFilesCount_; }

    void mergeFileCollectionMetrics(const FileCollectionMetrics& otherFileCollectionMetrics)
    {
        scannedDirectoriesCount_ += otherFileCollectionMetrics.scannedDirectoriesCount_;
        scannedFilesCount_ += otherFileCollectionMetrics.scannedFilesCount_;
        totalScannedBytes_ += otherFileCollectionMetrics.totalScannedBytes_;
    }

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

enum class FileCollectionStatus
{
    Completed,
    Cancelled,
    InvalidRootDirectory
};

class FileCollectionResult
{
public:
    FileCollectionResult(const FileCollectionStatus status, const FileCollectionMetrics& metrics) : status_(status), metrics_(metrics)
    {}

    [[nodiscard]] FileCollectionStatus getStatus() const { return status_; }
    [[nodiscard]] const FileCollectionMetrics& getMetrics() const { return metrics_; }

private:
    FileCollectionStatus status_;
    FileCollectionMetrics metrics_;
};

class FileCollector final
{
public:
    using FileVisitor = std::function<void(FileRecord)>;

    [[nodiscard]] static FileCollectionResult collectRecursively(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const FileVisitor& fileVisitor);

private:
    [[nodiscard]] static FileCollectionResult collectSingleRootRecursively(const QString& rootDirectoryPath, const std::stop_token& stopToken, const FileVisitor& fileVisitor);
};
