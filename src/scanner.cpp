#include "scanner.h"
#include "scan_request.h"
#include "file_name_scan_workflow.h"
#include "file_content_scan_workflow.h"
#include "scan_summary/scan_summary_logger.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
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

        ScanResult scanResult = scanWatcher_.result();

        if (stopSource_.stop_requested())
        {
            scanResult.setOutcome(ScanOutcome::Cancelled);
        }

        isScanning_ = false;

        if (scanResult.getOutcome() == ScanOutcome::Cancelled)
        {
            qInfo() << "Scan operation cancelled";
            ScanSummaryLogger::log(scanResult);
            emit scanCancelled();
            return;
        }

        emit progressChanged(scanDurationMilliseconds_);

        if (scanResult.getOutcome() == ScanOutcome::Failed)
        {
            qCritical() << "Scan operation failed";
        }
        else
        {
            qInfo() << "Scan operation complete:" << scanResult.getDuplicateGroups().size() << "duplicate groups found";
        }

        ScanSummaryLogger::log(scanResult);
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

    // Select the complete control-flow path before any scan work is scheduled.
    std::shared_ptr<const ScanWorkflow> scanWorkflow;

    switch (scanRequest.getScanType())
    {
        case ScanType::ByFileName:
            scanWorkflow = std::make_shared<FileNameScanWorkflow>();
            break;

        case ScanType::ByFileContent:
            scanWorkflow = std::make_shared<FileContentScanWorkflow>();
            break;
    }

    if (!scanWorkflow)
    {
        qFatal("Unknown ScanType value: %d", scanRequest.getScanType());
    }

    isScanning_ = true;
    stopSource_ = std::stop_source();
    elapsedTimer_.start();
    emit progressChanged(0);
    progressTimer_.start();

    const QString rootDirectoryPath = scanRequest.getRootDirectoryPath();
    const std::stop_token stopToken = stopSource_.get_token();

    scanWatcher_.setFuture(QtConcurrent::run([rootDirectoryPath, scanWorkflow, stopToken]
    {
        // TODO remove minimum duration
        QElapsedTimer minimumDurationTimer;
        minimumDurationTimer.start();

        ScanResult scanResult = scanWorkflow->execute(rootDirectoryPath, stopToken);

        // Keep the progress dialog visible long enough to provide useful feedback
        while (!scanResult.isScanCancelled() && minimumDurationTimer.elapsed() < scanDurationMilliseconds_)
        {
            if (stopToken.stop_requested())
            {
                scanResult.setOutcome(ScanOutcome::Cancelled);
                break;
            }

            QThread::msleep(20);
        }

        if (stopToken.stop_requested())
        {
            scanResult.setOutcome(ScanOutcome::Cancelled);
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

    stopSource_.request_stop();
}
