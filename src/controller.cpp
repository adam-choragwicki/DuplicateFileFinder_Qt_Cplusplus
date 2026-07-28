#include "controller.h"
#include "scan_request.h"

#include <QFileDialog>
#include <QProgressDialog>

Controller::Controller(Model& model, MainWindow& view) : model_(model), view_(view), scanner_(this)
{
    qInfo() << "Initializing controller";

    connect(&view_, &MainWindow::startScanButtonClicked, this, &Controller::onStartScanButtonClicked);
    connect(&view_, &MainWindow::chooseDirectoryButtonClicked, this, &Controller::onChooseDirectoryButtonClicked);
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
        "Collecting files...",
        "Cancel",
        0,
        Scanner::scanDurationMilliseconds(),
        &view_);
    scanProgressDialog_->setWindowTitle("Scanning");
    scanProgressDialog_->setWindowModality(Qt::WindowModal);
    scanProgressDialog_->setMinimumDuration(0);
    scanProgressDialog_->setAutoClose(false);
    scanProgressDialog_->setAutoReset(false);

    connect(scanProgressDialog_, &QProgressDialog::canceled, &scanner_, &Scanner::cancelScan);
    connect(&scanner_, &Scanner::progressChanged, scanProgressDialog_, &QProgressDialog::setValue);

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

void Controller::onScanOperationComplete(const ScanResult& scanResult)
{
    closeScanProgressDialog();
    view_.showScanResult(scanResult);
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
