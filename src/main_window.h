#pragma once

#include "types/scan_type.h"
#include "types/scan_result.h"
#include <ui_main_window.h>

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
    void chooseDirectoryButtonClicked();
    void revealFileInSystemFileManagerRequested(const QString& absoluteFilePath);
    void startScanButtonClicked();
    void quitButtonClicked();

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void initializeResultTabColumns();

    [[nodiscard]] ScanType getScanType() const;
    void setDirectoryPathLabel(const QString& directoryPath);
    void showScanResult(const ScanResult& scanResult);
    [[nodiscard]] QString getDirectoryPath() const { return ui->directoryPath_Label->text(); }

private:
    void initializeUI();
    void populateScanTypeComboBox();
    void initializeResultColumnWidths();
    void sortResultGroups(int column, Qt::SortOrder sortOrder);
    void populateResultTable();

    [[nodiscard]] QString getInitialDirectoryScanPath() const;

    Ui::MainWindow* ui;
    QList<DuplicateGroup> resultDuplicateGroups_;
    int resultSortColumn_{};
    Qt::SortOrder resultSortOrder_{Qt::AscendingOrder};
    bool resultColumnWidthsInitialized_{};
};
