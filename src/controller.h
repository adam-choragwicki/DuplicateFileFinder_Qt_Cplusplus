#pragma once

#include "backend/model.h"
#include "backend/scanner.h"
#include "frontend/main_window.h"

class QProgressDialog;

/// Coordinates the model and main window while managing scans, progress feedback, result presentation, export, and file-reveal actions.
class Controller : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Connects the model, view, and scanner, then synchronizes the view with current model state.
     *
     * @param[in] model Authoritative application model.
     * @param[in] view Main application window.
     */
    Controller(Model& model, MainWindow& view);

private slots:
    /// Exports the currently displayed duplicate groups to HTML and reports the outcome to the user.
    void onExportToHtmlRequested();
    /// Starts a scan from the current configuration, reports missing roots, and ignores duplicate requests.
    void onStartScanButtonClicked();
    /// Prompts for a scan root, adds it to the model, and reports when it is already covered.
    void onAddDirectoryButtonClicked();
    /// Removes the scan root currently selected in the view from the model.
    void onRemoveDirectoryButtonClicked();
    /// Updates the active progress dialog with the current phase and processed-file counts.
    void onScanProgressChanged(const ScanProgress& scanProgress);
    /// Shows the model's latest duplicate result or clears the result presentation when none is available.
    void onScanResultChanged();
    /// Closes progress feedback, stores the completed result, and displays any applicable status message.
    void onScanOperationComplete(const ScanResult& scanResult);
    /// Closes progress feedback and returns the model to its idle state after cancellation.
    void onScanOperationCancelled();

private:
    /// Creates and immediately displays the modal scan progress dialog.
    void showScanProgressDialog();
    /// Closes the active progress dialog, schedules its deletion, and clears the tracked pointer.
    void closeScanProgressDialog();
    /// Reveals an existing file in the platform file manager when the operating system supports it.
    /// @param[in] absoluteFilePath Absolute path of the file to reveal.
    void revealFileInSystemFileManager(const QString& absoluteFilePath);

    /// Authoritative application state.
    Model& model_;
    /// main application window.
    MainWindow& view_;

    /// Executes scan requests asynchronously and publishes progress and completion signals.
    Scanner scanner_;
    /// Non-owning pointer to the view-owned scan progress dialog, or nullptr when no dialog is active.
    QProgressDialog* scanProgressDialog_{};
};
