#pragma once

#include <QDir>

class FileRecord
{
public:
    FileRecord(const QString& fileName, const QString& directoryPath, const qint64 sizeBytes)
        : fileName_(fileName), directoryPath_(directoryPath), sizeBytes_(sizeBytes)
    {}

    [[nodiscard]] const QString& getFileName() const { return fileName_; }
    [[nodiscard]] const QString& getDirectoryPath() const { return directoryPath_; }
    [[nodiscard]] QString getAbsoluteFilePath() const { return QDir(directoryPath_).filePath(fileName_); }
    [[nodiscard]] qint64 getSizeBytes() const { return sizeBytes_; }

private:
    QString fileName_;
    QString directoryPath_;
    qint64 sizeBytes_;
};
