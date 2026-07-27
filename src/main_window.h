#pragma once

#include "types/scan_type.h"
#include <QMainWindow>

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

private:
    void initializeUI();
    void populateScanTypeComboBox();

    Ui::MainWindow* ui;

    bool isShutdownRequested_{};
};
