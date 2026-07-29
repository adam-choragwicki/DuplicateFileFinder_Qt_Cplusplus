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

    QDir testFilesDirectory{QDir::currentPath()};
    testFilesDirectory.cdUp();
    testFilesDirectory.cd("test_files");

    ui->directoryPath_Label->setText(testFilesDirectory.path());

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
    ui->results_TableWidget->setRowCount(scanResult.getDuplicateGroups().size());

    for (qsizetype row = 0; row < scanResult.getDuplicateGroups().size(); ++row)
    {
        const DuplicateGroup& duplicateGroup = scanResult.getDuplicateGroups().at(row);
        const QList<FileRecord>& files = duplicateGroup.getFiles();
        QStringList directories;
        QStringList sizes;

        for (const FileRecord& file: files)
        {
            directories.append(file.getDirectoryPath());
            sizes.append(QString::number(file.getSizeBytes() / 1024.0, 'f', 2));
        }

        ui->results_TableWidget->setItem(static_cast<int>(row), 0, new QTableWidgetItem(files.constFirst().getFileName()));
        ui->results_TableWidget->setItem(static_cast<int>(row), 1, new QTableWidgetItem(directories.join('\n')));
        ui->results_TableWidget->setItem(static_cast<int>(row), 2, new QTableWidgetItem(sizes.join('\n')));
    }

    ui->results_TableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->results_TableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->results_TableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->results_TableWidget->resizeRowsToContents();
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
