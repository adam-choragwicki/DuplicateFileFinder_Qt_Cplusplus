#pragma once

#include "types/file_record.h"
#include "types/scan_result.h"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QList>
#include <QString>
#include <QTimer>

#include <atomic>
#include <memory>

class ScanRequest;

class Scanner : public QObject
{
    Q_OBJECT

public:
    explicit Scanner(QObject* parent = nullptr);

    void scan(const ScanRequest& scanRequest);
    [[nodiscard]] bool isScanning() const;
    [[nodiscard]] static constexpr int scanDurationMilliseconds() { return scanDurationMilliseconds_; }

public slots:
    void cancelScan();

signals:
    void progressChanged(int elapsedMilliseconds);
    void scanComplete(const ScanResult& scanResult);
    void scanCancelled();

private:
    static constexpr int scanDurationMilliseconds_ = 750;

    QTimer progressTimer_;
    QElapsedTimer elapsedTimer_;
    QFutureWatcher<ScanResult> scanWatcher_;
    std::shared_ptr<std::atomic_bool> cancellationRequested_;
    bool isScanning_{};
};
