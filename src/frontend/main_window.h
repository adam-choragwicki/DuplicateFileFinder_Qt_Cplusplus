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

class MainWindow : public QMainWindow
{
    Q_OBJECT

signals:
    void addDirectoryButtonClicked();
    void removeDirectoryButtonClicked();
    void exportToHtmlRequested();
    void revealFileInSystemFileManagerRequested(const QString& absoluteFilePath);
    void startScanButtonClicked();
    void scanTypeSelectionChanged(ScanType scanType);
    void scanResultCloseRequested();
    void quitButtonClicked();

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    [[nodiscard]] ScanType getScanTypeFromComboBox() const;
    void setScanTypeInComboBox(ScanType scanType);
    void showScanResult(const ScanResult& scanResult);
    void clearScanResult();
    [[nodiscard]] QStringList getScanDirectoryPaths() const; /// Get all of top level paths in directories tree
    [[nodiscard]] QString getSelectedScanDirectoryPath() const;
    void setScanDirectoryPaths(const QStringList& directoryPaths);
    void addScanDirectory(const QString& directoryPath);
    void removeScanDirectory(const QString& directoryPath);
    void removeSelectedScanDirectory();
    [[nodiscard]] const QList<DuplicateGroup>& getDisplayedDuplicateGroups() const;

private:
    void initializeWindowGeometry();
    void initializeUI();
    void initializeDirectoriesTree();
    void populateScanTypeComboBox();
    void updateScanTypeDescription() const;
    void showAboutDialog();

    /// Enables directory actions according to whether scan roots exist and whether one top-level scan root is selected.
    void updateDirectoryActionStates();

    void initializeResultsTabCloseButton();

    Ui::MainWindow* ui;
    ScanDirectoriesTreeModel* directoriesTreeModel_{};
};
