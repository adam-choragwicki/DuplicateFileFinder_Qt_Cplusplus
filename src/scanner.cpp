#include "scanner.h"
#include "scan_request.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
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
        const bool wasCancelled = scanResult.cancelled || cancellationRequested_->load();
        isScanning_ = false;

        if (wasCancelled)
        {
            qInfo() << "Scan operation cancelled";
            emit scanCancelled();
            return;
        }

        emit progressChanged(scanDurationMilliseconds_);
        qInfo() << "Scan operation complete:" << scanResult.files.size() << "files found";
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
    const std::shared_ptr<std::atomic_bool> cancellationRequested = cancellationRequested_;

    scanWatcher_.setFuture(QtConcurrent::run([rootDirectoryPath, cancellationRequested]
    {
        ScanResult scanResult;
        QElapsedTimer minimumDurationTimer;
        minimumDurationTimer.start();

        QDirIterator iterator(rootDirectoryPath, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);

        while (iterator.hasNext())
        {
            if (cancellationRequested->load())
            {
                scanResult.cancelled = true;
                return scanResult;
            }

            iterator.next();
            const QFileInfo fileInfo = iterator.fileInfo();

            scanResult.files.append({
                .fileName_ = fileInfo.fileName(),
                .directoryPath_ = fileInfo.absolutePath(),
                .sizeBytes_ = fileInfo.size()
            });
        }

        // Keep the progress dialog visible long enough to provide useful feedback
        while (minimumDurationTimer.elapsed() < scanDurationMilliseconds_)
        {
            if (cancellationRequested->load())
            {
                scanResult.cancelled = true;
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
