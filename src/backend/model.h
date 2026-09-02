#pragma once

#include "types/scan_result.h"
#include "types/scan_type.h"

#include <QObject>

#include <optional>

/// Owns the scan configuration, lifecycle state, and latest result, emitting signals whenever observable state changes.
class Model : public QObject
{
    Q_OBJECT

signals:
    /// Emitted after the selected scan roots change, with the complete updated list.
    void scanDirectoryPathsChanged(const QStringList& scanDirectoryPaths);
    /// Emitted after the scan type changes, with the newly selected type.
    void scanTypeChanged(ScanType scanType);
    /// Emitted after the latest scan result is stored or an existing result is cleared.
    void scanResultChanged();

public:
    /// Describes the effect of an addScanDirectory() request.
    enum class AddScanDirectoryOutcome
    {
        /// The requested path was added as a scan root.
        Added,
        /// An existing scan root already covers the requested path.
        AlreadyIncluded,
        /// The supplied path was empty or contained only whitespace.
        InvalidPath
    };

    /// Describes whether a requested scan root was added and identifies any existing root that already includes it.
    class AddScanDirectoryResult final
    {
        friend class Model;

    public:
        /// Returns the result of attempting to add the requested directory.
        [[nodiscard]] AddScanDirectoryOutcome getAddScanDirectoryOutcome() const { return addScanDirectoryOutcome_; }

        /// Returns the absolute, cleaned requested path, or an empty string when the path is invalid.
        [[nodiscard]] const QString& getNormalizedDirectoryPath() const { return normalizedDirectoryPath_; }

        /// Returns the existing scan root that covers the requested path for AlreadyIncluded; otherwise an empty string.
        [[nodiscard]] const QString& getCoveringDirectoryPath() const { return coveringDirectoryPath_; }

    private:
        /// Creates the immutable result returned by Model::addScanDirectory().
        AddScanDirectoryResult(AddScanDirectoryOutcome addScanDirectoryOutcome, QString normalizedDirectoryPath, QString coveringDirectoryPath);

        const AddScanDirectoryOutcome addScanDirectoryOutcome_;
        const QString normalizedDirectoryPath_;
        const QString coveringDirectoryPath_;
    };

    /// Describes whether beginScan() started a new scan or why it could not do so.
    enum class ScanStartOutcome
    {
        /// The model entered the scanning state.
        Started,
        /// A scan is already in progress.
        AlreadyScanning,
        /// No scan roots have been selected.
        NoDirectoriesSelected
    };

    /// Creates an idle model with no scan roots or result and file-content scanning selected.
    /// @param[in] parent Optional QObject that owns this model.
    explicit Model(QObject* parent = nullptr);

    /// Returns the normalized scan roots in selection order.
    [[nodiscard]] const QStringList& getScanDirectoryPaths() const { return scanDirectoryPaths_; }
    /// Returns the currently selected duplicate-detection strategy.
    [[nodiscard]] ScanType getScanType() const { return scanType_; }
    /// Returns the latest completed scan result, or std::nullopt when no result is available.
    [[nodiscard]] const std::optional<ScanResult>& getLatestScanResult() const { return latestScanResult_; }
    /// Returns whether the latest scan completed with at least one non-empty duplicate group.
    [[nodiscard]] bool hasDuplicateResults() const;

    /**
     * @brief Adds a scan root to the list of existing scan roots.
     *
     * The supplied path is normalized before comparison. If the requested scan root is equal to or
     * below an existing scan root, the list remains unchanged and the covering scan root is reported.
     * Otherwise, existing scan roots below the requested scan root are removed and
     * the requested scan root is added. scanDirectoryPathsChanged() is emitted only when the list changes.
     *
     * @param[in] directoryPath Absolute or relative path of the requested scan root.
     * @return Result describing whether the requested scan root was added, was already covered, or had an invalid path.
     *
     * @note Only empty or whitespace-only input is considered invalid here; path existence is
     * checked later when a scan runs.
     */
    AddScanDirectoryResult addScanDirectory(const QString& directoryPath);

    /**
     * @brief Removes a scan root from the list of existing scan roots.
     *
     * The supplied path is normalized before comparison. If the requested scan root exactly matches
     * an existing scan root, that scan root is removed. A path below an existing scan root does not
     * remove its parent. scanDirectoryPathsChanged() is emitted only when the list changes.
     *
     * @param[in] directoryPath Absolute or relative path of the requested scan root.
     * @return true if the requested scan root matched an existing scan root and was removed; otherwise false.
     */
    bool removeScanDirectory(const QString& directoryPath);

    /// Selects the duplicate-detection strategy and emits scanTypeChanged() when it changes.
    void setScanType(ScanType scanType);

    /**
     * @brief Attempts to transition the model into the scanning state.
     *
     * Any existing scan result is cleared before the current state and selected scan roots are checked.
     * The model enters the scanning state only when Started is returned.
     *
     * @return Started when a scan may begin, AlreadyScanning when one is already in progress, or
     * NoDirectoriesSelected when no scan roots are configured.
     */
    ScanStartOutcome beginScan();

    /// Stores a completed result, returns the model to the idle state, and emits scanResultChanged().
    /// @param[in] scanResult Result to copy into the model as the latest scan result.
    void completeScan(const ScanResult& scanResult);

    /// Returns the model to the idle state after cancellation without creating or changing a scan result.
    void markScanCancelled();

    /// Removes the latest scan result and emits scanResultChanged() if a result was present.
    void clearScanResult();

private:
    /// Internal lifecycle state used to prevent simultaneous overlapping scans.
    enum class ScanState
    {
        /// No scan is currently in progress.
        Idle,
        /// A scan has begun and has not yet completed or been cancelled.
        Scanning
    };

    /// Absolute, cleaned scan roots; no entry is equal to or nested beneath another entry.
    QStringList scanDirectoryPaths_;
    /// Duplicate-detection strategy to use for the next scan.
    ScanType scanType_{ScanType::ByFileContent};
    /// Current scan lifecycle state.
    ScanState scanState_{ScanState::Idle};
    /// Most recently completed scan result, if one has not been cleared.
    std::optional<ScanResult> latestScanResult_;
};
