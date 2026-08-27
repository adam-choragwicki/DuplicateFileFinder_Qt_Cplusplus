#pragma once

#include "scan_progress.h"
#include "types/scan_result.h"
#include "types/scan_type.h"

#include <QObject>

#include <optional>

class Model : public QObject
{
    Q_OBJECT

public:
    enum class AddScanDirectoryOutcome
    {
        Added,
        AlreadyIncluded,
        InvalidPath
    };

    struct AddScanDirectoryResult
    {
        AddScanDirectoryOutcome outcome;
        QString normalizedDirectoryPath;
        QString coveringDirectoryPath;
    };

    enum class ScanStartOutcome
    {
        Started,
        AlreadyScanning,
        NoDirectoriesSelected
    };

    enum class ScanState
    {
        Idle,
        Scanning
    };

    explicit Model(QObject* parent = nullptr);

    [[nodiscard]] const QStringList& getScanDirectoryPaths() const;
    [[nodiscard]] ScanType getScanType() const;
    [[nodiscard]] ScanState getScanState() const;
    [[nodiscard]] const std::optional<ScanProgress>& getScanProgress() const;
    [[nodiscard]] const std::optional<ScanResult>& getLatestScanResult() const;
    [[nodiscard]] bool hasDuplicateResults() const;

    AddScanDirectoryResult addScanDirectory(const QString& directoryPath);
    bool removeScanDirectory(const QString& directoryPath);
    void setScanType(ScanType scanType);
    ScanStartOutcome beginScan();
    void updateScanProgress(const ScanProgress& progress);
    void completeScan(const ScanResult& scanResult);
    void markScanCancelled();
    void clearScanResult();

signals:
    void scanDirectoryPathsChanged(const QStringList& scanDirectoryPaths);
    void scanTypeChanged(ScanType scanType);
    void scanStateChanged(ScanState scanState);
    void scanProgressChanged(const ScanProgress& progress);
    void scanResultChanged();

private:
    /// Returns an absolute, cleaned directory path using Qt's '/' separator format without resolving symbolic links.
    [[nodiscard]] static QString normalizeDirectoryPath(const QString& directoryPath);

    /// Returns true when directoryPath is the same as, or is located anywhere below, possibleParentDirectoryPath.
    [[nodiscard]] static bool isSameDirectoryOrSubdirectoryOf(const QString& directoryPath, const QString& possibleParentDirectoryPath);

    QStringList scanDirectoryPaths_;
    ScanType scanType_{ScanType::ByFileName};
    ScanState scanState_{ScanState::Idle};
    std::optional<ScanProgress> scanProgress_;
    std::optional<ScanResult> latestScanResult_;
};
