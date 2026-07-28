#include "main_window.h"
#include "scanner.h"
#include "ui_main_window.h"

#include <QComboBox>
#include <QCloseEvent>
#include <QDir>
#include <QHeaderView>
#include <QTableWidgetItem>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("Duplicate file finder");

    initializeUI();

    connect(ui->chooseDirectory_PushButton, &QPushButton::clicked, this, &MainWindow::chooseDirectoryButtonClicked);
    connect(ui->startScan_PushButton, &QPushButton::clicked, this, &MainWindow::startScanButtonClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initializeUI()
{
    // Clear placeholder values from combo boxes
    ui->scanType_ComboBox->clear();

    ui->directoryPath_Label->setText(QDir::currentPath());

    populateScanTypeComboBox();
}

void MainWindow::populateScanTypeComboBox()
{
    ui->scanType_ComboBox->addItem("By file name", QVariant::fromValue(ScanType::ByFileName));
    ui->scanType_ComboBox->addItem("By file content", QVariant::fromValue(ScanType::ByFileContent));

    // Center align text
    for (int i = 0; i < ui->scanType_ComboBox->count(); ++i)
    {
        ui->scanType_ComboBox->setItemData(i, Qt::AlignCenter, Qt::TextAlignmentRole);
    }
}

void MainWindow::setDirectoryPathLabel(const QString& directoryPath)
{
    ui->directoryPath_Label->setText(directoryPath);
}

void MainWindow::showScanResult(const ScanResult& scanResult)
{
    ui->results_TableWidget->setSortingEnabled(false);
    ui->results_TableWidget->clearContents();
    ui->results_TableWidget->setRowCount(scanResult.files.size());

    for (qsizetype row = 0; row < scanResult.files.size(); ++row)
    {
        const FileRecord& file = scanResult.files.at(row);

        ui->results_TableWidget->setItem(static_cast<int>(row), 0, new QTableWidgetItem(file.fileName_));
        ui->results_TableWidget->setItem(static_cast<int>(row), 1, new QTableWidgetItem(file.directoryPath_));
        ui->results_TableWidget->setItem(static_cast<int>(row), 2, new QTableWidgetItem(QString::number(file.sizeBytes_ / 1024.0, 'f', 2)));
    }

    ui->results_TableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->results_TableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->results_TableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->results_TableWidget->setSortingEnabled(true);
    ui->main_TabWidget->setCurrentWidget(ui->resultsTab);
}

ScanType MainWindow::getScanType() const
{
    const QVariant data = ui->scanType_ComboBox->currentData();

    if (!data.canConvert<ScanType>())
    {
        qFatal("Invalid or missing ScanType in scanType_ComboBox");
    }

    return data.value<ScanType>();
}
