#include "controller.h"
#include "html_result_exporter.h"
#include "backend/scan_request.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QDesktopServices> // linux only
#include <QUrl> // linux only

#include <limits>

Controller::Controller(Model& model, MainWindow& view) : model_(model), view_(view), scanner_(this)
{
    qInfo() << "Initializing controller";

    connect(&view_, &MainWindow::startScanButtonClicked, this, &Controller::onStartScanButtonClicked);
    connect(&view_, &MainWindow::addDirectoryButtonClicked, this, &Controller::onAddDirectoryButtonClicked);
    connect(&view_, &MainWindow::removeDirectoryButtonClicked, this, &Controller::onRemoveDirectoryButtonClicked);
    connect(&view_, &MainWindow::scanTypeSelectionChanged, &model_, &Model::setScanType);
    connect(&view_, &MainWindow::scanResultCloseRequested, &model_, &Model::clearScanResult);
    connect(&view_, &MainWindow::exportToHtmlRequested, this, &Controller::onExportToHtmlRequested);
    connect(&view_, &MainWindow::revealFileInSystemFileManagerRequested, this, &Controller::revealFileInSystemFileManager);

    connect(&model_, &Model::scanDirectoryPathsChanged, &view_, &MainWindow::setScanDirectoryPaths);
    connect(&model_, &Model::scanTypeChanged, &view_, &MainWindow::setScanTypeInComboBox);
    connect(&model_, &Model::scanResultChanged, this, &Controller::onScanResultChanged);

    connect(&scanner_, &Scanner::progressChanged, this, &Controller::onScanProgressChanged);
    connect(&scanner_, &Scanner::scanComplete, this, &Controller::onScanOperationComplete);
    connect(&scanner_, &Scanner::scanCancelled, this, &Controller::onScanOperationCancelled);

    view_.setScanDirectoryPaths(model_.getScanDirectoryPaths());
    view_.setScanTypeInComboBox(model_.getScanType());

    onScanResultChanged();
}

void Controller::onExportToHtmlRequested()
{
    // This is the table model's sorted presentation copy, so its order matches the rows visible to the user.
    const QList<DuplicateGroup>& duplicateGroups = view_.getDisplayedDuplicateGroups();

    if (duplicateGroups.isEmpty())
    {
        QMessageBox::warning(&view_, "Export failed", "There are no scan results to export.");
        return;
    }

    const QString outputFilePath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("duplicate_file_finder_results.html"));
    QString errorMessage;

    if (!HtmlResultExporter::exportToFile(duplicateGroups, outputFilePath, errorMessage))
    {
        qCritical() << "Failed to export scan results to" << outputFilePath << ':' << errorMessage;

        QMessageBox::critical(
            &view_,
            "Export failed",
            QStringLiteral("The HTML report could not be saved.\n\n%1").arg(errorMessage));

        return;
    }

    qInfo() << "Exported scan results to" << outputFilePath;

    QMessageBox::information(
        &view_,
        "Export complete",
        QStringLiteral("The HTML report was saved to:\n\n%1").arg(QDir::toNativeSeparators(outputFilePath)));
}

void Controller::onStartScanButtonClicked()
{
    switch (model_.beginScan())
    {
        case Model::ScanStartOutcome::Started:
            showScanProgressDialog();
            scanner_.scan(ScanRequest(model_.getScanDirectoryPaths(), model_.getScanType()));
            return;

        case Model::ScanStartOutcome::AlreadyScanning:
            return;

        case Model::ScanStartOutcome::NoDirectoriesSelected:
            QMessageBox::information(
                &view_,
                "Nothing to scan",
                "Nothing to scan. No directories selected.");
    }
}

void Controller::showScanProgressDialog()
{
    scanProgressDialog_ = new QProgressDialog("Preparing scan...",
                                              "Cancel",
                                              0,
                                              0,
                                              &view_);
    scanProgressDialog_->setWindowTitle("Scanning");
    scanProgressDialog_->setWindowModality(Qt::WindowModal);
    scanProgressDialog_->setMinimumDuration(0);
    scanProgressDialog_->setAutoClose(false);
    scanProgressDialog_->setAutoReset(false);

    connect(scanProgressDialog_, &QProgressDialog::canceled, &scanner_, &Scanner::cancelScan);

    // QProgressDialog normally waits for control to return to the event loop before painting.
    // Show and paint the initial state first so thread-pool startup or other scan preparation can
    // never leave the user looking at an unchanged main window after clicking Start scan.
    scanProgressDialog_->show();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void Controller::onAddDirectoryButtonClicked()
{
    qDebug() << "Add directory clicked";

    const QString directoryPath = QFileDialog::getExistingDirectory(&view_, "Choose directory to scan", QDir::currentPath(), QFileDialog::DontResolveSymlinks);

    if (directoryPath.isEmpty())
    {
        return;
    }

    const Model::AddScanDirectoryResult addScanDirectoryResult = model_.addScanDirectory(directoryPath);

    switch (addScanDirectoryResult.outcome)
    {
        case Model::AddScanDirectoryOutcome::Added:
        case Model::AddScanDirectoryOutcome::InvalidPath:
            return;

        case Model::AddScanDirectoryOutcome::AlreadyIncluded:
            QMessageBox::information(&view_,
                                     "Directory already included",
                                     QStringLiteral("Directory \"%1\" is already in the list because it is covered by \"%2\".")
                                     .arg(
                                         QDir::toNativeSeparators(addScanDirectoryResult.normalizedDirectoryPath),
                                         QDir::toNativeSeparators(addScanDirectoryResult.coveringDirectoryPath)));
    }
}

void Controller::onRemoveDirectoryButtonClicked()
{
    qDebug() << "Remove directory clicked";

    model_.removeScanDirectory(view_.getSelectedScanDirectoryPath());
}

void Controller::onScanProgressChanged(const ScanProgress& progress)
{
    model_.updateScanProgress(progress);

    if (!scanProgressDialog_)
    {
        return;
    }

    QString progressText = scanPhaseDescription(progress.scanPhase);

    if (!progress.totalFilesCount.has_value())
    {
        // A directory must be enumerated before its total number of files is known. Keep the
        // progress bar indeterminate during that single pass, but still expose the live count.
        scanProgressDialog_->setRange(0, 0);
        progressText += QStringLiteral("\nProcessed %1/? files").arg(progress.processedFilesCount);
    }
    else
    {
        const quint64 totalFiles = *progress.totalFilesCount;
        const quint64 processedFiles = qMin(progress.processedFilesCount, totalFiles);
        progressText += QStringLiteral("\nProcessed %1/%2 files").arg(processedFiles).arg(totalFiles);

        if (totalFiles == 0)
        {
            scanProgressDialog_->setRange(0, 1);
            scanProgressDialog_->setValue(1);
        }
        else if (totalFiles <= static_cast<quint64>(std::numeric_limits<int>::max()))
        {
            scanProgressDialog_->setRange(0, static_cast<int>(totalFiles));
            scanProgressDialog_->setValue(static_cast<int>(processedFiles));
        }
        else
        {
            // QProgressDialog uses int ranges. Scale exceptionally large file counts while keeping the exact 64-bit values in the text shown to the user.
            constexpr int scaledMaximum = 1'000'000;
            const auto scaledValue = static_cast<int>(static_cast<long double>(processedFiles) / static_cast<long double>(totalFiles) * scaledMaximum);
            scanProgressDialog_->setRange(0, scaledMaximum);
            scanProgressDialog_->setValue(scaledValue);
        }
    }

    scanProgressDialog_->setLabelText(progressText);
}

void Controller::onScanResultChanged()
{
    if (model_.hasDuplicateResults())
    {
        view_.showScanResult(*model_.getLatestScanResult());
    }
    else
    {
        view_.clearScanResult();
    }
}

void Controller::onScanOperationComplete(const ScanResult& scanResult)
{
    closeScanProgressDialog();
    model_.completeScan(scanResult);

    const quint64 problematicFilesCount = scanResult.getProblematicFilesCount();
    const bool completedWithProblems = problematicFilesCount > 0
                                       && scanResult.getOutcome() != ScanOutcome::Failed
                                       && scanResult.getOutcome() != ScanOutcome::Cancelled;
    switch (scanResult.getOutcome())
    {
        case ScanOutcome::CompletedWithDuplicates:
            break;

        case ScanOutcome::CompletedWithoutDuplicates:
            if (!completedWithProblems)
            {
                QMessageBox::information(
                    &view_,
                    "Scan complete",
                    "The scan completed successfully. No duplicate files were found.");
            }
            break;

        case ScanOutcome::NoFilesFound:
            if (!completedWithProblems)
            {
                QMessageBox::information(
                    &view_,
                    "No files found",
                    "The selected directory and its subdirectories contain no files.");
            }
            break;

        case ScanOutcome::Failed:
            QMessageBox::critical(
                &view_,
                "Scan failed",
                "The scan could not be completed. Check the application log for details.");
            break;

        case ScanOutcome::Cancelled:
            break;
    }

    if (completedWithProblems)
    {
        const QString warningText = problematicFilesCount == 1
                                        ? QStringLiteral("The scan completed, but 1 file could not be read and was skipped. "
                                            "Results may be incomplete.\n\n"
                                            "Check the application log for details.")
                                        : QStringLiteral("The scan completed, but %1 files could not be read and were skipped. "
                                            "Results may be incomplete.\n\n"
                                            "Check the application log for details.")
                                        .arg(problematicFilesCount);

        QMessageBox::warning(&view_, "Scan completed with warnings", warningText);
    }
}

void Controller::onScanOperationCancelled()
{
    closeScanProgressDialog();
    model_.markScanCancelled();
}

void Controller::closeScanProgressDialog()
{
    if (!scanProgressDialog_)
    {
        return;
    }

    scanProgressDialog_->close();
    scanProgressDialog_->deleteLater();
    scanProgressDialog_ = nullptr;
}

void Controller::revealFileInSystemFileManager(const QString& absoluteFilePath)
{
    const QFileInfo fileInfo(absoluteFilePath);
    if (!fileInfo.isFile())
    {
        qWarning() << "Cannot reveal file because it no longer exists:" << absoluteFilePath;
        return;
    }

#if defined(Q_OS_WIN)
    const bool fileManagerStarted = QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(fileInfo.absoluteFilePath())});

    if (!fileManagerStarted)
    {
        qWarning() << "Failed to open Windows File Explorer for:" << fileInfo.absoluteFilePath();
    }
#elif defined(Q_OS_LINUX) // TODO test on Linux
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath())))
    {
        qWarning() << "Failed to open the containing directory for:" << fileInfo.absoluteFilePath();
    }
#else
    qWarning() << "Revealing files in the system file manager is unsupported on this operating system";
#endif
}
