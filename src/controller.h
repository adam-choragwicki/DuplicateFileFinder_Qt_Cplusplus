#pragma once

#include "model.h"
#include "view/main_window.h"
#include "scanner.h"

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
    void onScanOperationComplete(const ScanResult& scanResult);
    void onScanOperationCancelled();

private:
    void closeScanProgressDialog();
    void revealFileInSystemFileManager(const QString& absoluteFilePath);

    /// Returns an absolute, cleaned directory path using Qt's '/' separator format without resolving symbolic links.
    [[nodiscard]] static QString normalizeDirectoryPath(const QString& directoryPath);

    /// Returns true when directoryPath is the same as, or is located anywhere below, possibleParentDirectoryPath.
    [[nodiscard]] static bool isSameDirectoryOrSubdirectoryOf(const QString& directoryPath, const QString& possibleParentDirectoryPath);

    Model& model_;
    MainWindow& view_;

    Scanner scanner_;
    QProgressDialog* scanProgressDialog_{};
};
