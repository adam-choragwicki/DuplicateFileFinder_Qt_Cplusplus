#pragma once

#include "types/scan_type.h"
#include "types/scan_result.h"
#include "types/file_record.h"
#include <QList>
#include <QMainWindow>
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

    [[nodiscard]] ScanType getScanType() const;
    void setDirectoryPathLabel(const QString& directoryPath);
    void showScanResult(const ScanResult& scanResult);
    [[nodiscard]] QString getDirectoryPath() const { return ui->directoryPath_Label->text(); }

private:
    void initializeUI();
    void populateScanTypeComboBox();
    [[nodiscard]] QString getInitialDirectoryScanPath() const;

    Ui::MainWindow* ui;
};
