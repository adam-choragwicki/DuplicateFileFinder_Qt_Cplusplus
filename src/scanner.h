#pragma once

#include <QTimer>

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
    void progressChanged(int elapsedMilliseconds);
    void operationComplete();
    void operationCancelled();

private:
    static constexpr int fakeScanDurationMilliseconds_ = 3000;

    QTimer progressTimer_;
    QElapsedTimer elapsedTimer_;
    bool isScanning_{};
};
