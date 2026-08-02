#include "file_content_scan_workflow.h"
#include <QElapsedTimer>
#include <QCryptographicHash>

FileContentScanWorkflow::FileContentScanWorkflow() : fileHasher_(calculateFileHash)
{}

ScanResult FileContentScanWorkflow::execute(const QString& rootDirectoryPath, const std::stop_token& stopToken) const
{
    QElapsedTimer durationTimer;
    durationTimer.start();

    QList<DuplicateGroup> duplicateGroups;
    ScanOutcome outcome = ScanOutcome::Failed;
    QHash<qint64, QList<FileRecord>> filesBySize;

    qInfo() << "File content scan stage 1 started: collecting files and grouping them by size";
    const FileCollectionResult collectionResult = FileCollector::collectRecursively(
        rootDirectoryPath,
        stopToken,
        [&filesBySize](FileRecord file)
        {
            // collect files and group them by size
            filesBySize[file.getSizeBytes()].append(std::move(file));
        });

    if (collectionResult.getStatus() == FileCollectionStatus::InvalidRootDirectory)
    {
        outcome = ScanOutcome::Failed;
    }
    else if (collectionResult.getStatus() == FileCollectionStatus::Cancelled || stopToken.stop_requested())
    {
        outcome = ScanOutcome::Cancelled;
    }
    else
    {
        bool fileAccessFailed = false;

        for (auto sizeIterator = filesBySize.cbegin(); sizeIterator != filesBySize.cend() && !fileAccessFailed; ++sizeIterator)
        {
            if (stopToken.stop_requested())
            {
                break;
            }

            const QList<FileRecord>& equalSizeFiles = sizeIterator.value();

            qInfo() << "File content scan stage 2: discard unique-size groups";
            // Discard unique-size groups. A unique file size cannot have a duplicate, so do not perform any file I/O for it.
            if (equalSizeFiles.size() < 2)
            {
                continue;
            }

            QHash<QByteArray, QList<FileRecord>> filesByHash;

            for (const FileRecord& file: equalSizeFiles)
            {
                if (stopToken.stop_requested())
                {
                    break;
                }

                qInfo() << "File content scan stage 3: hash candidate files and group the files by hash";
                // hash candidate files and group the files by hash
                const std::optional<QByteArray> hash = fileHasher_(file, stopToken);

                if (!hash.has_value())
                {
                    if (!stopToken.stop_requested())
                    {
                        qWarning() << "Content scan cannot produce a complete result; hashing failed for:" << file.getAbsoluteFilePath();
                        fileAccessFailed = true;
                    }

                    break;
                }

                filesByHash[*hash].append(file);
            }

            if (stopToken.stop_requested() || fileAccessFailed)
            {
                break;
            }

            // TODO add verification
        }

        if (stopToken.stop_requested())
        {
            outcome = ScanOutcome::Cancelled;
        }
        else if (fileAccessFailed)
        {
            outcome = ScanOutcome::Failed;
        }
        else
        {
            outcome = classifySuccessfulScan(collectionResult.getMetrics(), duplicateGroups);
        }
    }

    const DuplicateGroupMetrics duplicateMetrics = calculateDuplicateGroupMetrics(duplicateGroups);
    ScanSummary summary{
        QDateTime::currentDateTimeUtc(),
        std::chrono::milliseconds(durationTimer.elapsed()),
        collectionResult.getMetrics().getScannedDirectoriesCount(),
        collectionResult.getMetrics().getScannedFilesCount(),
        collectionResult.getMetrics().getTotalScannedBytes(),
        static_cast<quint64>(duplicateGroups.size()),
        duplicateMetrics.getFilesCount(),
        duplicateMetrics.getTotalBytes()
    };

    return ScanResult(std::move(duplicateGroups), outcome, std::move(summary));
}

std::optional<QByteArray> FileContentScanWorkflow::calculateFileHash(const FileRecord& file, const std::stop_token& stopToken)
{
    constexpr qint64 fileReadBufferSize = 1024 * 1024;

    if (stopToken.stop_requested())
    {
        return std::nullopt;
    }

    QFile inputFile(file.getAbsoluteFilePath());

    if (!inputFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Cannot hash file:" << inputFile.fileName() << '-' << inputFile.errorString();
        return std::nullopt;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    const qint64 bufferSize = std::min(fileReadBufferSize, std::max<qint64>(file.getSizeBytes(), 1));
    QByteArray buffer(bufferSize, '\0');
    qint64 totalBytesRead = 0;

    while (!inputFile.atEnd())
    {
        if (stopToken.stop_requested())
        {
            return std::nullopt;
        }

        const qint64 bytesRead = inputFile.read(buffer.data(), buffer.size());

        if (bytesRead < 0)
        {
            qWarning() << "Cannot read file while hashing:" << inputFile.fileName() << '-' << inputFile.errorString();
            return std::nullopt;
        }

        if (bytesRead == 0)
        {
            break;
        }

        hash.addData(QByteArrayView(buffer.constData(), bytesRead));
        totalBytesRead += bytesRead;
    }

    return hash.result();
}
