#include "controller.h"
#include "scan_request.h"
#include <QFileDialog>

Controller::Controller(Model& model, MainWindow& view) : model_(model), view_(view)
{
    qInfo() << "Initializing controller";

    connect(&view_, &MainWindow::startScanButtonClicked, this, &Controller::onStartScanButtonClicked);
    connect(&view_, &MainWindow::chooseDirectoryButtonClicked, this, &Controller::onChooseDirectoryButtonClicked);
}

void Controller::onStartScanButtonClicked()
{
    const ScanRequest scanRequest(view_.getDirectoryPath(), view_.getScanType());

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
