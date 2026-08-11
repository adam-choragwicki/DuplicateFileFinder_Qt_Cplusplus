#include "controller.h"
#include "html_result_exporter.h"
#include "scan_request.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>

#include <limits>

Controller::Controller(Model& model, MainWindow& view) : model_(model), view_(view), scanner_(this)
{
    qInfo() << "Initializing controller";

    connect(&view_, &MainWindow::startScanButtonClicked, this, &Controller::onStartScanButtonClicked);
    connect(&view_, &MainWindow::addDirectoryButtonClicked, this, &Controller::onAddDirectoryButtonClicked);
    connect(&view_, &MainWindow::removeDirectoryButtonClicked, this, &Controller::onRemoveDirectoryButtonClicked);
    connect(&view_, &MainWindow::exportToHtmlRequested, this, &Controller::onExportToHtmlRequested);
    connect(&view_, &MainWindow::revealFileInSystemFileManagerRequested, this, &Controller::revealFileInSystemFileManager);
    connect(&scanner_, &Scanner::progressChanged, this, &Controller::onScanProgressChanged);
    connect(&scanner_, &Scanner::scanComplete, this, &Controller::onScanOperationComplete);
    connect(&scanner_, &Scanner::scanCancelled, this, &Controller::onScanOperationCancelled);
}

void Controller::onExportToHtmlRequested()
{
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
    if (scanner_.isScanning())
    {
        return;
    }

    const QStringList scanDirectoryPaths = view_.getScanDirectoryPaths();

    if (scanDirectoryPaths.isEmpty())
    {
        QMessageBox::information(
            &view_,
            "Nothing to scan",
            "Nothing to scan. No directories selected.");
        return;
    }

    const ScanRequest scanRequest(scanDirectoryPaths, view_.getScanType());

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

    scanner_.scan(scanRequest);
}

void Controller::onAddDirectoryButtonClicked()
{
    qDebug() << "Add directory clicked";

    const QString directoryPath = QFileDialog::getExistingDirectory(&view_, "Choose directory to scan", QDir::currentPath(), QFileDialog::DontResolveSymlinks);

    if (directoryPath.isEmpty())
    {
        return;
    }

    const QString normalizedDirectoryPath = normalizeDirectoryPath(directoryPath);
    const QStringList existingDirectoryPaths = view_.getScanDirectoryPaths();

    for (const QString& existingDirectoryPath: existingDirectoryPaths)
    {
        if (isSameDirectoryOrSubdirectoryOf(normalizedDirectoryPath, existingDirectoryPath))
        {
            QMessageBox::information(&view_,
                                     "Directory already included",
                                     QStringLiteral("Directory \"%1\" is already in the list because it is covered by \"%2\".")
                                     .arg(
                                         QDir::toNativeSeparators(normalizedDirectoryPath),
                                         QDir::toNativeSeparators(existingDirectoryPath)));
            return;
        }
    }

    // A newly selected parent replaces every existing scan root contained inside it. Scanning
    // both would enumerate the nested root twice and could report the same file more than once.
    for (const QString& existingDirectoryPath: existingDirectoryPaths)
    {
        if (isSameDirectoryOrSubdirectoryOf(existingDirectoryPath, normalizedDirectoryPath))
        {
            view_.removeScanDirectory(existingDirectoryPath);
        }
    }

    view_.addScanDirectory(normalizedDirectoryPath);
}

void Controller::onRemoveDirectoryButtonClicked()
{
    qDebug() << "Remove directory clicked";

    view_.removeSelectedScanDirectory();
}

QString Controller::normalizeDirectoryPath(const QString& directoryPath)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(QDir(directoryPath).absolutePath()));
}

bool Controller::isSameDirectoryOrSubdirectoryOf(const QString& directoryPath, const QString& possibleParentDirectoryPath)
{
    const QString normalizedDirectoryPath = normalizeDirectoryPath(directoryPath);
    const QString normalizedParentDirectoryPath = normalizeDirectoryPath(possibleParentDirectoryPath);

#if defined(Q_OS_WIN)
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseSensitive;
#endif

    if (normalizedDirectoryPath.compare(normalizedParentDirectoryPath, pathCaseSensitivity) == 0)
    {
        return true;
    }

    QString parentDirectoryPathPrefix = normalizedParentDirectoryPath;

    if (!parentDirectoryPathPrefix.endsWith('/'))
    {
        parentDirectoryPathPrefix.append('/');
    }

    return normalizedDirectoryPath.startsWith(parentDirectoryPathPrefix, pathCaseSensitivity);
}

void Controller::onScanProgressChanged(const ScanProgress& progress)
{
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
