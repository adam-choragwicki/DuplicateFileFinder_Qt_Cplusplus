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
        const bool wasCancelled = scanResult.isScanCancelled() || cancellationRequested_->load();
        isScanning_ = false;

        if (wasCancelled)
        {
            qInfo() << "Scan operation cancelled";
            emit scanCancelled();
            return;
        }

        emit progressChanged(scanDurationMilliseconds_);
        qInfo() << "Scan operation complete:" << scanResult.getDuplicateGroups().size() << "duplicate groups found";
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
    cancellationRequested_ = std::make_shared<std::atomic_bool>(false);
    elapsedTimer_.start();
    emit progressChanged(0);
    progressTimer_.start();

    const QString rootDirectoryPath = scanRequest.getRootDirectoryPath();
    const ScanType scanType = scanRequest.getScanType();
    const std::shared_ptr<std::atomic_bool> cancellationRequested = cancellationRequested_;

    scanWatcher_.setFuture(QtConcurrent::run([rootDirectoryPath, scanType, cancellationRequested]
    {
        ScanResult scanResult;
        QElapsedTimer minimumDurationTimer;
        minimumDurationTimer.start();

        if (scanType != ScanType::ByFileName)
        {
            qWarning() << "Scan by file content is not implemented yet";
        }
        else
        {
            // Stage 1 - collect files by name recursively
            const QMap<QString, QList<FileRecord>> filesByName = collectFilesByNameRecursively(rootDirectoryPath, cancellationRequested);

            if (cancellationRequested->load())
            {
                scanResult.setScanCancelled();
                return scanResult;
            }

            // Stage 2 - find which files have non-unique names and group them
            scanResult = findDuplicateGroupsByFileName(filesByName, cancellationRequested);

            if (scanResult.isScanCancelled())
            {
                return scanResult;
            }
        }

        // Keep the progress dialog visible long enough to provide useful feedback
        while (minimumDurationTimer.elapsed() < scanDurationMilliseconds_)
        {
            if (cancellationRequested->load())
            {
                scanResult.setScanCancelled();
                return scanResult;
            }

            QThread::msleep(20);
        }

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

    cancellationRequested_->store(true);
}

QMap<QString, QList<FileRecord>> Scanner::collectFilesByNameRecursively(const QString& rootDirectoryPath, const std::shared_ptr<std::atomic_bool>& cancellationRequested)
{
    QMap<QString, QList<FileRecord>> filesByName;
    QDirIterator iterator(rootDirectoryPath,
                          QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);

    while (iterator.hasNext())
    {
        if (cancellationRequested->load())
        {
            break;
        }

        iterator.next();
        const QFileInfo fileInfo = iterator.fileInfo();

        filesByName[fileInfo.fileName()].append(FileRecord(fileInfo.fileName(), fileInfo.absolutePath(), fileInfo.size()));
    }

    return filesByName;
}

ScanResult Scanner::findDuplicateGroupsByFileName(
    const QMap<QString, QList<FileRecord>>& filesByName,
    const std::shared_ptr<std::atomic_bool>& cancellationRequested)
{
    ScanResult scanResult;

    for (auto iterator = filesByName.cbegin(); iterator != filesByName.cend(); ++iterator)
    {
        if (cancellationRequested->load())
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
