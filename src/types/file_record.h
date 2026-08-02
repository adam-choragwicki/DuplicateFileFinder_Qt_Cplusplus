#pragma once

#include <QDir>
#include <QDateTime>

class FileRecord
{
public:
    FileRecord(QString fileName, QString directoryPath, const qint64 sizeBytes, QDateTime lastModified = {})
        : fileName_(std::move(fileName)),
          directoryPath_(std::move(directoryPath)),
          sizeBytes_(sizeBytes),
          lastModified_(std::move(lastModified))
    {}

    [[nodiscard]] const QString& getFileName() const { return fileName_; }
    [[nodiscard]] const QString& getDirectoryPath() const { return directoryPath_; }
    [[nodiscard]] QString getAbsoluteFilePath() const { return QDir(directoryPath_).filePath(fileName_); }
    [[nodiscard]] qint64 getSizeBytes() const { return sizeBytes_; }
    [[nodiscard]] const QDateTime& getLastModified() const { return lastModified_; }

private:
    QString fileName_;
    QString directoryPath_;
    qint64 sizeBytes_;
    QDateTime lastModified_;
};
