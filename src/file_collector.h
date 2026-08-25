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
    void incrementProblematicFilesCount() { ++problematicFilesCount_; }

    void mergeFileCollectionMetrics(const FileCollectionMetrics& otherFileCollectionMetrics)
    {
        scannedDirectoriesCount_ += otherFileCollectionMetrics.scannedDirectoriesCount_;
        scannedFilesCount_ += otherFileCollectionMetrics.scannedFilesCount_;
        problematicFilesCount_ += otherFileCollectionMetrics.problematicFilesCount_;
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
    [[nodiscard]] quint64 getProblematicFilesCount() const { return problematicFilesCount_; }
    [[nodiscard]] quint64 getTotalScannedBytes() const { return totalScannedBytes_; }

private:
    quint64 scannedDirectoriesCount_{};
    quint64 scannedFilesCount_{};
    quint64 problematicFilesCount_{};
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
    struct RootDirectory
    {
        QString originalPath;
        QString normalizedPath;
        bool isValid;
    };

    [[nodiscard]] static Qt::CaseSensitivity getScanRootPathCaseSensitivity();
    [[nodiscard]] static bool isProperSubdirectoryOf(const QString& directoryPath, const QString& possibleParentPath);
    [[nodiscard]] static QStringList getNonOverlappingRootDirectoryPaths(const QStringList& rootDirectoryPaths);
    [[nodiscard]] static FileCollectionResult collectSingleRootRecursively(const QString& rootDirectoryPath, const std::stop_token& stopToken, const FileVisitor& fileVisitor);
};
