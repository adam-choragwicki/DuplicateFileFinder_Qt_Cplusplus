#pragma once

#include "scan_progress.h"
#include "types/scan_result.h"

#include <QFutureWatcher>
#include <QString>
#include <QTimer>
#include <memory>
#include <stop_token>

class ScanRequest;

/// @brief Executes scan workflows asynchronously and publishes throttled progress on its owning thread.
///
/// At most one scan is active at a time. Work runs on Qt's global thread pool, while a timer periodically snapshots
/// synchronized worker progress and emits it to UI consumers. Cancellation is cooperative through std::stop_token.
class Scanner : public QObject
{
    Q_OBJECT

signals:
    /// Emitted on the scanner's thread with the latest available progress snapshot.
    void progressChanged(const ScanProgress& progress);
    /// Emitted after a successful or failed scan finishes; cancelled scans use scanCancelled() instead.
    void scanComplete(const ScanResult& scanResult);
    /// Emitted after an active scan observes cancellation and finishes unwinding.
    void scanCancelled();

public:
    /// Creates an idle scanner and its progress-polling infrastructure.
    /// @param[in] parent Optional QObject owner.
    explicit Scanner(QObject* parent = nullptr);

    /// Schedules the requested workflow unless another scan is already active.
    /// @param[in] scanRequest Immutable scan configuration copied before asynchronous execution.
    void scan(const ScanRequest& scanRequest);
    /// Returns whether a workflow has been scheduled and has not yet delivered a terminal signal.
    [[nodiscard]] bool isScanning() const;

public slots:
    /// Requests cooperative cancellation of the active scan; does nothing while idle.
    void cancelScan();

private:
    /// Shared, synchronized progress storage defined in the implementation file.
    struct ProgressState;

    /// Copies and emits the latest progress snapshot without holding its synchronization lock during signal delivery.
    void emitCurrentProgress();

    /// Polls worker progress at a UI-friendly interval.
    QTimer progressTimer_;
    /// Observes asynchronous workflow completion on the scanner's owning thread.
    QFutureWatcher<ScanResult> scanWatcher_;
    /// Lifetime-managed progress state shared with the worker callback.
    std::shared_ptr<ProgressState> progressState_;
    /// Supplies the cancellation token captured by the active worker.
    std::stop_source stopSource_;
    /// Guards against scheduling overlapping scans.
    bool isScanning_{};
};
