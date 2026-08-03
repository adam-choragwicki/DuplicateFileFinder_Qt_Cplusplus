#pragma once

#include "model.h"
#include "main_window.h"
#include "scanner.h"

class QProgressDialog;

class Controller : public QObject
{
    Q_OBJECT

public:
    Controller(Model& model, MainWindow& view);

private slots:
    void onStartScanButtonClicked();
    void onChooseDirectoryButtonClicked();
    void onScanProgressChanged(const ScanProgress& progress);
    void onScanOperationComplete(const ScanResult& scanResult);
    void onScanOperationCancelled();

private:
    void closeScanProgressDialog();

    Model& model_;
    MainWindow& view_;

    Scanner scanner_;
    QProgressDialog* scanProgressDialog_{};
};
