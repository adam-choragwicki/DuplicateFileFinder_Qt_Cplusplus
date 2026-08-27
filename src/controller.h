#pragma once

#include "backend/model.h"
#include "backend/scanner.h"
#include "frontend/main_window.h"

class QProgressDialog;

class Controller : public QObject
{
    Q_OBJECT

public:
    Controller(Model& model, MainWindow& view);

private slots:
    void onExportToHtmlRequested();
    void onStartScanButtonClicked();
    void onAddDirectoryButtonClicked();
    void onRemoveDirectoryButtonClicked();
    void onScanProgressChanged(const ScanProgress& progress);
    void onScanResultChanged();
    void onScanOperationComplete(const ScanResult& scanResult);
    void onScanOperationCancelled();

private:
    void showScanProgressDialog();
    void closeScanProgressDialog();
    void revealFileInSystemFileManager(const QString& absoluteFilePath);

    Model& model_;
    MainWindow& view_;

    Scanner scanner_;
    QProgressDialog* scanProgressDialog_{};
};
