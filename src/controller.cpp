#include "controller.h"
#include "scan_request.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>

#include <limits>

Controller::Controller(Model& model, MainWindow& view) : model_(model), view_(view), scanner_(this)
{
    qInfo() << "Initializing controller";

    connect(&view_, &MainWindow::startScanButtonClicked, this, &Controller::onStartScanButtonClicked);
    connect(&view_, &MainWindow::chooseDirectoryButtonClicked, this, &Controller::onChooseDirectoryButtonClicked);
    connect(&view_, &MainWindow::revealFileInSystemFileManagerRequested, this, &Controller::revealFileInSystemFileManager);
    connect(&scanner_, &Scanner::progressChanged, this, &Controller::onScanProgressChanged);
    connect(&scanner_, &Scanner::scanComplete, this, &Controller::onScanOperationComplete);
    connect(&scanner_, &Scanner::scanCancelled, this, &Controller::onScanOperationCancelled);
}

void Controller::onStartScanButtonClicked()
{
    if (scanner_.isScanning())
    {
        return;
    }

    const ScanRequest scanRequest(view_.getDirectoryPath(), view_.getScanType());

    scanProgressDialog_ = new QProgressDialog(
        "Preparing scan...",
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

    scanner_.scan(scanRequest);
}

void Controller::onChooseDirectoryButtonClicked()
{
    qDebug() << "Choose directory clicked";

    const QString directoryPath = QFileDialog::getExistingDirectory(&view_, "Choose directory to scan", QDir::currentPath(), QFileDialog::DontResolveSymlinks);

    if (!directoryPath.isEmpty())
    {
        view_.setDirectoryPathLabel(directoryPath);
    }
    else
    {
        qWarning() << "Directory path is empty";
    }
}

void Controller::onScanProgressChanged(const ScanProgress& progress)
{
    if (!scanProgressDialog_)
    {
        return;
    }

    QString progressText = scanPhaseDescription(progress.phase);

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

void Controller::onScanOperationComplete(const ScanResult& scanResult)
{
    closeScanProgressDialog();

    switch (scanResult.getOutcome())
    {
        case ScanOutcome::CompletedWithDuplicates:
            view_.showScanResult(scanResult);
            break;

        case ScanOutcome::CompletedWithoutDuplicates:
            QMessageBox::information(
                &view_,
                "Scan complete",
                "The scan completed successfully. No duplicate files were found.");
            break;

        case ScanOutcome::NoFilesFound:
            QMessageBox::information(
                &view_,
                "No files found",
                "The selected directory and its subdirectories contain no files.");
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
}

void Controller::onScanOperationCancelled()
{
    closeScanProgressDialog();
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
