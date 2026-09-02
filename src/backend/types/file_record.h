#pragma once

#include <QDir>

/// @brief Stores the name, containing directory, and observed size of one enumerated file.
///
/// The record is a snapshot created during file-tree enumeration. It does not keep the file open or re-read filesystem
/// metadata, so the represented file may subsequently change or cease to exist.
class FileRecord
{
public:
    /// Creates a file record from its path components and observed size.
    /// @param[in] fileName File name without its containing directory.
    /// @param[in] directoryPath Path of the containing directory, expected to be absolute.
    /// @param[in] sizeBytes Non-negative file size observed during enumeration.
    FileRecord(QString fileName, QString directoryPath, const qint64 sizeBytes)
        : fileName_(std::move(fileName)),
          directoryPath_(std::move(directoryPath)),
          sizeBytes_(sizeBytes)
    {}

    /// Returns the file name without its containing directory.
    [[nodiscard]] const QString& getFileName() const { return fileName_; }
    /// Returns the path of the directory containing the file.
    [[nodiscard]] const QString& getDirectoryPath() const { return directoryPath_; }
    /// Returns the complete file path formed by joining the containing directory and file name.
    [[nodiscard]] QString getAbsoluteFilePath() const { return QDir(directoryPath_).filePath(fileName_); }
    /// Returns the file size observed when this record was created.
    [[nodiscard]] qint64 getSizeBytes() const { return sizeBytes_; }

private:
    QString fileName_;
    QString directoryPath_;
    qint64 sizeBytes_;
};
