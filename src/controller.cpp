#include "controller.h"

Controller::Controller(Model& model, MainWindow& view) : model_(model), view_(view)
{
    qInfo() << "Initializing controller";

    connect(&view_, &MainWindow::startScanButtonClicked, this, &Controller::onStartScanButtonClicked);
    connect(&view_, &MainWindow::chooseDirectoryButtonClicked, this, &Controller::onChooseDirectoryButtonClicked);
}

void Controller::onStartScanButtonClicked()
{
    qDebug() << "Start scan clicked";
}

void Controller::onChooseDirectoryButtonClicked()
{
    qDebug() << "Choose directory clicked";
}
