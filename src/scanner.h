#pragma once

#include "types/scan_result.h"

#include <QFutureWatcher>
#include <QString>
#include <QTimer>
#include <stop_token>

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
    /// Used to stop scan asynchronously
    std::stop_source stopSource_;
    bool isScanning_{};
};
