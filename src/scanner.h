#pragma once

#include "types/file_record.h"
#include "types/scan_result.h"

#include <QFutureWatcher>
#include <QList>
#include <QMap>
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
    class FileCollectionMetrics
    {
    public:
        void incrementScannedDirectoriesCount() { ++scannedDirectoriesCount_; }
        void incrementScannedFilesCount() { ++scannedFilesCount_; }

        void addToTotalScannedBytes(const qint64 bytes)
        {
            if (bytes > 0)
            {
                totalScannedBytes_ += static_cast<quint64>(bytes);
            }
        }

        [[nodiscard]] quint64 getScannedDirectoriesCount() const { return scannedDirectoriesCount_; }
        [[nodiscard]] quint64 getScannedFilesCount() const { return scannedFilesCount_; }
        [[nodiscard]] quint64 getTotalScannedBytes() const { return totalScannedBytes_; }

    private:
        quint64 scannedDirectoriesCount_{};
        quint64 scannedFilesCount_{};
        quint64 totalScannedBytes_{};
    };

    [[nodiscard]] static QMap<QString, QList<FileRecord>> collectFilesByNameRecursively(const QString& rootDirectoryPath, const std::stop_token& stopToken, FileCollectionMetrics& fileCollectionMetrics);
    [[nodiscard]] static ScanResult findDuplicateGroupsByFileName(const QMap<QString, QList<FileRecord>>& filesByName, const std::stop_token& stopToken);
    [[nodiscard]] static ScanSummary createScanSummary(const FileCollectionMetrics& collectionMetrics, const ScanResult& scanResult, std::chrono::milliseconds duration);
    static void logScanSummary(const ScanSummary& summary);

    static constexpr int scanDurationMilliseconds_ = 750;

    QTimer progressTimer_;
    QElapsedTimer elapsedTimer_;
    QFutureWatcher<ScanResult> scanWatcher_;
    /// Used to stop scan asynchronously
    std::stop_source stopSource_;
    bool isScanning_{};
};
