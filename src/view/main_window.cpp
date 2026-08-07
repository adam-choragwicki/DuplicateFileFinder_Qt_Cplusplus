#include "main_window.h"
#include "results_tab.h"
#include "ui_main_window.h"

#include <QComboBox>
#include <QDir>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("Duplicate file finder");

    initializeUI();

    connect(ui->chooseDirectory_PushButton, &QPushButton::clicked, this, &MainWindow::chooseDirectoryButtonClicked);
    connect(ui->exportToHtml_Action, &QAction::triggered, this, &MainWindow::exportToHtmlRequested);
    connect(ui->quit_Action, &QAction::triggered, this, &QWidget::close);
    connect(ui->resultsTab, &ResultsTab::revealFileInSystemFileManagerRequested, this, &MainWindow::revealFileInSystemFileManagerRequested);
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

    ui->directoryPath_Label->setText(getInitialDirectoryScanPath());

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

    ui->scanType_ComboBox->setCurrentIndex(1); // choose "By file content"
}

void MainWindow::setDirectoryPathLabel(const QString& directoryPath)
{
    ui->directoryPath_Label->setText(directoryPath);
}

void MainWindow::showScanResult(const ScanResult& scanResult)
{
    ui->resultsTab->showDuplicateGroups(scanResult.getDuplicateGroups());

    ui->exportToHtml_Action->setEnabled(true);
    ui->main_TabWidget->setCurrentWidget(ui->resultsTab);
}

QString MainWindow::getDirectoryPath() const
{
    return ui->directoryPath_Label->text();
}

const QList<DuplicateGroup>& MainWindow::getDisplayedDuplicateGroups() const
{
    return ui->resultsTab->getDisplayedDuplicateGroups();
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

QString MainWindow::getInitialDirectoryScanPath() const
{
    constexpr enum class TestType
    {
        SMOKE_TEST,
        RESULT_PRESENTATION_TEST,
        BIG_DIRECTORY_TEST
    } testType = TestType::RESULT_PRESENTATION_TEST; // adjust test type here

    if (testType == TestType::SMOKE_TEST)
    {
        QDir smokeTestFilesDirectory{QDir::currentPath()};
        smokeTestFilesDirectory.cdUp();
        smokeTestFilesDirectory.cd("test_files");
        smokeTestFilesDirectory.cd("smoke_test");
        return smokeTestFilesDirectory.path();
    }

    if (testType == TestType::RESULT_PRESENTATION_TEST)
    {
        QDir resultPresentationTestDirectoryPath{QDir::currentPath()};
        resultPresentationTestDirectoryPath.cdUp();
        resultPresentationTestDirectoryPath.cd("test_files");
        resultPresentationTestDirectoryPath.cd("result_presentation_test");
        return resultPresentationTestDirectoryPath.path();
    }

    if (testType == TestType::BIG_DIRECTORY_TEST)
    {
        const QDir bigDirectoryPath = QString("C:") + QDir::separator() + QString("FULL_EXTERNAL_DISK");
        return bigDirectoryPath.path();
    }

    throw std::runtime_error("Invalid TestType");
}
