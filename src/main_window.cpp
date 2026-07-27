#include "main_window.h"
#include "ui_main_window.h"
#include <QComboBox>
#include <QCloseEvent>
#include <QDir>

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

ScanType MainWindow::getScanType() const
{
    const QVariant data = ui->scanType_ComboBox->currentData();

    if (!data.canConvert<ScanType>())
    {
        qFatal("Invalid or missing ScanType in scanType_ComboBox");
    }

    return data.value<ScanType>();
}
