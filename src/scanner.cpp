#include "scanner.h"
#include "scan_request.h"
#include "file_name_scan_workflow.h"
#include "file_content_scan_workflow.h"
#include "scan_summary/scan_summary_logger.h"

#include <QDebug>
#include <QtConcurrentRun>
#include <mutex>

struct Scanner::ProgressState
{
    // ScanProgress is written by the background scan thread and polled by the UI thread.
    // Protect the whole value so readers see one coherent phase/count snapshot instead of fields from two
    // different updates; accessing it concurrently without synchronization would be a data race.
    std::mutex mutex;
    ScanProgress progress;
};

Scanner::Scanner(QObject* parent) : QObject(parent), progressState_(std::make_shared<ProgressState>())
{
    progressTimer_.setInterval(50);

    connect(&progressTimer_, &QTimer::timeout, this, [this]
    {
        emitCurrentProgress();
    });

    connect(&scanWatcher_, &QFutureWatcher<ScanResult>::finished, this, [this]
    {
        emitCurrentProgress();
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
    qDebug().noquote() << QString("Scan type: %1").arg(scanTypeToString(scanRequest.getScanType()));

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

    {
        // UI-side write: use the same lock as later worker updates and timer snapshots.
        const std::lock_guard lock(progressState_->mutex);
        progressState_->progress = {.phase = ScanPhase::EnumeratingFiles, .processedFilesCount = 0, .totalFilesCount = std::nullopt};
    }

    emitCurrentProgress();
    progressTimer_.start();

    const QString rootDirectoryPath = scanRequest.getRootDirectoryPath();
    const std::stop_token stopToken = stopSource_.get_token();
    const std::shared_ptr<ProgressState> progressState = progressState_;

    scanWatcher_.setFuture(QtConcurrent::run([rootDirectoryPath, scanWorkflow, stopToken, progressState]
    {
        const ScanProgressCallback scanProgressCallback = [progressState](const ScanProgress& progress)
        {
            // Worker-side write: the UI timer may take a snapshot at the same time.
            const std::lock_guard lock(progressState->mutex);
            progressState->progress = progress;
        };

        ScanResult scanResult = scanWorkflow->execute(rootDirectoryPath, stopToken, scanProgressCallback);

        if (stopToken.stop_requested())
        {
            scanResult.setOutcome(ScanOutcome::Cancelled);
        }

        return scanResult;
    }));
}

void Scanner::emitCurrentProgress()
{
    ScanProgress progress;

    {
        // UI-side read: copy all fields while locked, then release the mutex before emitting the
        // signal so connected UI code never runs while the shared state is locked.
        const std::lock_guard lock(progressState_->mutex);
        progress = progressState_->progress;
    }

    emit progressChanged(progress);
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
