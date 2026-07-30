#include "scanner.h"
#include "scan_request.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMap>
#include <QThread>
#include <QtConcurrentRun>

Scanner::Scanner(QObject* parent) : QObject(parent)
{
    progressTimer_.setInterval(50);

    connect(&progressTimer_, &QTimer::timeout, this, [this]
    {
        const int elapsedMilliseconds = qMin(static_cast<int>(elapsedTimer_.elapsed()), scanDurationMilliseconds_);

        emit progressChanged(elapsedMilliseconds);
    });

    connect(&scanWatcher_, &QFutureWatcher<ScanResult>::finished, this, [this]
    {
        progressTimer_.stop();

        const ScanResult scanResult = scanWatcher_.result();
        const bool wasCancelled = scanResult.isScanCancelled() || stopSource_.stop_requested();
        isScanning_ = false;

        if (wasCancelled)
        {
            qInfo() << "Scan operation cancelled";
            logScanSummary(scanResult.getScanSummary());
            emit scanCancelled();
            return;
        }

        emit progressChanged(scanDurationMilliseconds_);
        qInfo() << "Scan operation complete:" << scanResult.getDuplicateGroups().size() << "duplicate groups found";
        logScanSummary(scanResult.getScanSummary());
        emit scanComplete(scanResult);
    });
}

void Scanner::scan(const ScanRequest& scanRequest)
{
    if (isScanning_)
    {
        qWarning() << "A scan operation is already in progress";
        return;
    }

    qDebug() << "Scan started";
    qDebug() << "Root directory path" << scanRequest.getRootDirectoryPath();
    qDebug() << "Scan type" << static_cast<int>(scanRequest.getScanType());

    isScanning_ = true;
    stopSource_ = std::stop_source();
    elapsedTimer_.start();
    emit progressChanged(0);
    progressTimer_.start();

    const QString rootDirectoryPath = scanRequest.getRootDirectoryPath();
    const ScanType scanType = scanRequest.getScanType();
    const std::stop_token stopToken = stopSource_.get_token();

    scanWatcher_.setFuture(QtConcurrent::run([rootDirectoryPath, scanType, stopToken]
    {
        ScanResult scanResult;
        FileCollectionMetrics fileCollectionMetrics;
        QElapsedTimer minimumDurationTimer;
        minimumDurationTimer.start();

        if (scanType != ScanType::ByFileName)
        {
            qWarning() << "Scan by file content is not implemented yet";
        }
        else
        {
            // Stage 1 - collect files by name recursively
            qInfo() << "Scan stage 1 started: collecting files recursively";
            const QMap<QString, QList<FileRecord>> filesByName = collectFilesByNameRecursively(rootDirectoryPath, stopToken, fileCollectionMetrics);

            if (stopToken.stop_requested())
            {
                scanResult.setScanCancelled();
            }
            else
            {
                // Stage 2 - find which files have non-unique names and group them
                qInfo() << "Scan stage 2 started: grouping files with duplicate names";
                scanResult = findDuplicateGroupsByFileName(filesByName, stopToken);
            }
        }

        // Keep the progress dialog visible long enough to provide useful feedback
        while (!scanResult.isScanCancelled() && minimumDurationTimer.elapsed() < scanDurationMilliseconds_)
        {
            if (stopToken.stop_requested())
            {
                scanResult.setScanCancelled();
                break;
            }

            QThread::msleep(20);
        }

        if (stopToken.stop_requested())
        {
            scanResult.setScanCancelled();
        }

        scanResult.setScanSummary(createScanSummary(fileCollectionMetrics, scanResult, std::chrono::milliseconds(minimumDurationTimer.elapsed())));

        return scanResult;
    }));
}

bool Scanner::isScanning() const
{
    return isScanning_;
}

void Scanner::cancelScan()
{
    if (!isScanning_)
    {
        return;
    }

    stopSource_.request_stop();
}

QMap<QString, QList<FileRecord>> Scanner::collectFilesByNameRecursively(const QString& rootDirectoryPath, const std::stop_token& stopToken, FileCollectionMetrics& fileCollectionMetrics)
{
    QMap<QString, QList<FileRecord>> filesByName;
    const QDir rootDirectory(rootDirectoryPath);
    fileCollectionMetrics.scannedDirectoriesCount = rootDirectory.exists() ? 1 : 0;

    QDirIterator iterator(rootDirectoryPath,
                          QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);

    while (iterator.hasNext())
    {
        if (stopToken.stop_requested())
        {
            break;
        }

        iterator.next();
        const QFileInfo fileInfo = iterator.fileInfo();

        if (fileInfo.isDir())
        {
            ++fileCollectionMetrics.scannedDirectoriesCount;
            continue;
        }

        if (!fileInfo.isFile())
        {
            continue;
        }

        ++fileCollectionMetrics.scannedFilesCount;
        fileCollectionMetrics.totalScannedBytes += static_cast<quint64>(fileInfo.size());
        filesByName[fileInfo.fileName()].append(FileRecord(fileInfo.fileName(), fileInfo.absolutePath(), fileInfo.size()));
    }

    return filesByName;
}

ScanResult Scanner::findDuplicateGroupsByFileName(const QMap<QString, QList<FileRecord>>& filesByName, const std::stop_token& stopToken)
{
    ScanResult scanResult;

    for (auto iterator = filesByName.cbegin(); iterator != filesByName.cend(); ++iterator)
    {
        if (stopToken.stop_requested())
        {
            scanResult.setScanCancelled();
            return scanResult;
        }

        if (iterator.value().size() < 2)
        {
            continue;
        }

        DuplicateGroup duplicateGroup;

        for (const FileRecord& file: iterator.value())
        {
            duplicateGroup.addFile(file);
        }

        scanResult.appendDuplicateGroup(duplicateGroup);
    }

    return scanResult;
}

ScanSummary Scanner::createScanSummary(const FileCollectionMetrics& collectionMetrics, const ScanResult& scanResult, const std::chrono::milliseconds duration)
{
    quint64 totalFilesInDuplicateGroupsCount = 0;
    quint64 totalBytesOccupiedByFilesInDuplicateGroups = 0;

    for (const DuplicateGroup& duplicateGroup: scanResult.getDuplicateGroups())
    {
        totalFilesInDuplicateGroupsCount += static_cast<quint64>(duplicateGroup.getFiles().size());

        for (const FileRecord& file: duplicateGroup.getFiles())
        {
            totalBytesOccupiedByFilesInDuplicateGroups += static_cast<quint64>(file.getSizeBytes());
        }
    }

    // Duplicate filenames do not prove identical content, so no bytes are considered safely recoverable in this scan mode.
    constexpr quint64 totalAmountOfPotentiallyRecoverableBytes = 0;

    return ScanSummary{
        QDateTime::currentDateTimeUtc(),
        duration,
        collectionMetrics.scannedDirectoriesCount,
        collectionMetrics.scannedFilesCount,
        collectionMetrics.totalScannedBytes,
        static_cast<quint64>(scanResult.getDuplicateGroups().size()),
        totalFilesInDuplicateGroupsCount,
        totalBytesOccupiedByFilesInDuplicateGroups,
        totalAmountOfPotentiallyRecoverableBytes
    };
}

void Scanner::logScanSummary(const ScanSummary& summary)
{
    qInfo() << "Scan summary:";
    qInfo().noquote() << "  Completed at:"
            << summary.getCompletedAt().toLocalTime().toString("yyyy-MM-dd hh:mm:ss");
    qInfo() << "  Duration (ms):" << summary.getDuration().count();
    qInfo() << "  Scanned directories:" << summary.getScannedDirectoriesCount();
    qInfo() << "  Scanned files:" << summary.getScannedFilesCount();
    qInfo() << "  Total scanned bytes:" << summary.getTotalScannedBytes();
    qInfo() << "  Duplicate groups:" << summary.getDuplicateGroupsCount();
    qInfo() << "  Files in duplicate groups:" << summary.getTotalFilesInDuplicateGroupsCount();
    qInfo() << "  Bytes occupied by files in duplicate groups:" << summary.getTotalBytesOccupiedByFilesInDuplicateGroups();

    qInfo() << "  Potentially recoverable bytes:" << summary.getTotalAmountOfPotentiallyRecoverableBytes(); // TODO this is irrelevant for scanning by file name
}
