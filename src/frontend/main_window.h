#pragma once

#include "types/scan_type.h"
#include "types/scan_result.h"

#include <QFileInfo>
#include <QMainWindow>

class QTreeWidgetItem;

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
    void quitButtonClicked();

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    [[nodiscard]] ScanType getScanType() const;
    void showScanResult(const ScanResult& scanResult);
    void clearScanResult();
    [[nodiscard]] QStringList getScanDirectoryPaths() const; /// Get all of top level paths in directories tree
    void addScanDirectory(const QString& directoryPath);
    void removeScanDirectory(const QString& directoryPath);
    void removeSelectedScanDirectory();
    [[nodiscard]] const QList<DuplicateGroup>& getDisplayedDuplicateGroups() const;

private:
    void initializeUI();
    void initializeDirectoriesTree(); // TODO move it out of this class
    void populateScanTypeComboBox();
    void populateDirectoryTreeItem(QTreeWidgetItem* directoryItem) const; // TODO move it out of this class
    void showAboutDialog();

    /// Enables directory actions according to whether scan roots exist and whether one top-level scan root is selected.
    void updateDirectoryActionStates();

    [[nodiscard]] QTreeWidgetItem* createDirectoryTreeItem(const QString& directoryPath, const QString& displayedPath) const; // TODO move it out of this class
    [[nodiscard]] static QFileInfoList findChildDirectories(const QString& directoryPath); // TODO move it out of this class
    [[nodiscard]] QString getInitialDirectoryScanPath() const;
    void initializeResultsTabCloseButton();

    static constexpr int directoryPathDataRole_ = Qt::UserRole; // TODO move it out of this class
    static constexpr int directoryChildrenLoadedDataRole_ = Qt::UserRole + 1; // TODO move it out of this class

    Ui::MainWindow* ui;
};
