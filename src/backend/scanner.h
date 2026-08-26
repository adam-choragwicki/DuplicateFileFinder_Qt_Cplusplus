#pragma once

#include "scan_progress.h"
#include "types/scan_result.h"

#include <QFutureWatcher>
#include <QString>
#include <QTimer>
#include <memory>
#include <stop_token>

class ScanRequest;

class Scanner : public QObject
{
    Q_OBJECT

public:
    explicit Scanner(QObject* parent = nullptr);

    void scan(const ScanRequest& scanRequest);
    [[nodiscard]] bool isScanning() const;

public slots:
    void cancelScan();

signals:
    void progressChanged(const ScanProgress& progress);
    void scanComplete(const ScanResult& scanResult);
    void scanCancelled();

private:
    struct ProgressState;

    void emitCurrentProgress();

    QTimer progressTimer_;
    QFutureWatcher<ScanResult> scanWatcher_;
    std::shared_ptr<ProgressState> progressState_;
    std::stop_source stopSource_; /// Used to stop scan asynchronously
    bool isScanning_{};
};
