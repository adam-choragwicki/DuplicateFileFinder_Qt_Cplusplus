#pragma once

#include <QtTypes>

/// @brief Accumulates statistics produced while recursively enumerating file trees.
///
/// A file is considered scanned only after it has been opened for reading and its size has been obtained. Files that
/// cannot reach that point are counted as problematic and are excluded from both the scanned-file and byte totals.
/// The directory count includes every accepted root directory and each directory encountered below it.
class FileTreeEnumerationMetrics
{
public:
    /// Records one directory visited by the enumerator.
    void incrementScannedDirectoriesCount() { ++scannedDirectoriesCount_; }
    /// Records one readable file delivered to the file visitor.
    void incrementScannedFilesCount() { ++scannedFilesCount_; }
    /// Records one file skipped because it could not be opened or its size could not be read.
    void incrementProblematicFilesCount() { ++problematicFilesCount_; }

    /// Adds every counter from another enumeration to this instance.
    /// @param[in] otherMetrics Metrics to accumulate, typically from another root directory.
    void merge(const FileTreeEnumerationMetrics& otherMetrics)
    {
        scannedDirectoriesCount_ += otherMetrics.scannedDirectoriesCount_;
        scannedFilesCount_ += otherMetrics.scannedFilesCount_;
        problematicFilesCount_ += otherMetrics.problematicFilesCount_;
        totalScannedBytes_ += otherMetrics.totalScannedBytes_;
    }

    /// Adds a successfully scanned file's size to the total byte count. Non-positive values leave the total unchanged.
    /// @param[in] bytes File size in bytes.
    void addToTotalScannedBytes(const qint64 bytes)
    {
        if (bytes > 0)
        {
            totalScannedBytes_ += static_cast<quint64>(bytes);
        }
    }

    /// Returns the number of accepted roots and descendant directories visited.
    [[nodiscard]] quint64 getScannedDirectoriesCount() const { return scannedDirectoriesCount_; }
    /// Returns the number of readable files delivered to the file visitor.
    [[nodiscard]] quint64 getScannedFilesCount() const { return scannedFilesCount_; }
    /// Returns the number of files skipped because their contents or size could not be accessed.
    [[nodiscard]] quint64 getProblematicFilesCount() const { return problematicFilesCount_; }
    /// Returns the combined size of all files delivered to the file visitor.
    [[nodiscard]] quint64 getTotalScannedBytes() const { return totalScannedBytes_; }

private:
    quint64 scannedDirectoriesCount_{};
    quint64 scannedFilesCount_{};
    quint64 problematicFilesCount_{};
    quint64 totalScannedBytes_{};
};
