#pragma once

#include "types/scan_type.h"
#include "types/scan_result.h"

#include <QMainWindow>

class ScanDirectoriesTreeModel;

QT_BEGIN_NAMESPACE

namespace Ui
{
    class MainWindow;
}

QT_END_NAMESPACE

/// @brief Presents scan configuration and duplicate results while exposing user intent through Qt signals.
///
/// The window owns presentation models only; authoritative configuration and results remain in the backend Model and
/// are synchronized by Controller.
class MainWindow : public QMainWindow
{
    Q_OBJECT

signals:
    /// Emitted when the user requests selection of another scan root.
    void addDirectoryButtonClicked();
    /// Emitted when the user requests removal of the selected top-level scan root.
    void removeDirectoryButtonClicked();
    /// Emitted when the user requests HTML export of the displayed result.
    void exportToHtmlRequested();
    /// Forwards a result-row request to reveal a file in the platform file manager.
    void revealFileInSystemFileManagerRequested(const QString& absoluteFilePath);
    /// Emitted when the user requests a scan with the currently presented configuration.
    void startScanButtonClicked();
    /// Emitted after the scan-type combo box selects a different valid ScanType.
    void scanTypeSelectionChanged(ScanType scanType);
    /// Emitted when the user closes the duplicate-results tab.
    void scanResultTabCloseRequested();
    /// Requests application shutdown.
    void quitButtonClicked();

public:
    /// Constructs and initializes the complete main-window UI.
    /// @param[in] parent Optional QWidget owner.
    explicit MainWindow(QWidget* parent = nullptr);
    /// Releases the generated Qt Designer UI object.
    ~MainWindow() override;

    /// Returns the ScanType stored in the current combo-box item.
    /// @warning Terminates the process if the current item does not contain a valid ScanType.
    [[nodiscard]] ScanType getScanTypeFromComboBox() const;
    /// Selects the combo-box item representing the supplied scan type.
    /// @param[in] scanType Scan type to display.
    /// @warning Terminates the process when no item represents scanType.
    void setScanTypeInComboBox(ScanType scanType);
    /// Displays non-empty duplicate groups and selects the results tab; an empty result clears the presentation.
    /// @param[in] scanResult Result to present.
    void showScanResult(const ScanResult& scanResult);
    /// Removes all displayed groups, disables export, and hides the results tab.
    void clearScanResult();
    /// Returns only top-level scan roots from the directory presentation model.
    [[nodiscard]] QStringList getScanDirectoryPaths() const;
    /// Returns the selected top-level scan root, or an empty string for no, multiple, or child selection.
    [[nodiscard]] QString getSelectedScanDirectoryPath() const;
    /// Replaces all presented scan roots and selects the last root when available.
    /// @param[in] directoryPaths Complete root list in presentation order.
    void setScanDirectoryPaths(const QStringList& directoryPaths);
    /// Appends one absolute root path to the presentation model.
    /// @param[in] directoryPath Directory path to append.
    void addScanDirectory(const QString& directoryPath);
    /// Removes every presented root equal to the supplied absolute path.
    /// @param[in] directoryPath Directory path to remove.
    void removeScanDirectory(const QString& directoryPath);
    /// Removes the currently selected item only when it is a top-level scan root.
    void removeSelectedScanDirectory();
    /// Returns duplicate groups in their current results-table presentation order.
    [[nodiscard]] const QList<DuplicateGroup>& getDisplayedDuplicateGroups() const;

private:
    /// Sizes the window to 75 percent of the primary screen and centers it in the available geometry.
    void initializeWindowGeometry();
    /// Initializes child widgets and the initial no-result state.
    void initializeUI();
    /// Installs and configures the scan-directory presentation model.
    void initializeDirectoriesTree();
    /// Adds every supported ScanType to the combo box and initializes its descriptive text.
    void populateScanTypeComboBox();
    /// Updates explanatory text to match the currently selected scan type.
    void updateScanTypeDescription() const;
    /// Displays application version.
    void showAboutDialog();

    /// Enables directory actions according to whether scan roots exist and whether one top-level scan root is selected.
    void updateDirectoryActionStates();

    /// Installs the custom close button that emits scanResultTabCloseRequested().
    void initializeResultsTabCloseButton();

    /// Generated Qt Designer widget hierarchy owned and deleted by this window.
    Ui::MainWindow* ui;
    /// Presentation model owned through QObject parenting by this window.
    ScanDirectoriesTreeModel* directoriesTreeModel_{};
};
