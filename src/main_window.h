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

    [[nodiscard]] QString getInitialDirectoryScanPath() const;

    Ui::MainWindow* ui;
    bool resultColumnWidthsInitialized_{};
};
