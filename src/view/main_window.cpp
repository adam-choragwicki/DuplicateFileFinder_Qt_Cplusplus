#include "main_window.h"
#include "results_tab.h"
#include "ui_main_window.h"

#include <QComboBox>
#include <QDir>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("Duplicate file finder");

    initializeUI();

    connect(ui->chooseDirectory_PushButton, &QPushButton::clicked, this, &MainWindow::chooseDirectoryButtonClicked);
    connect(ui->resultsTab, &ResultsTab::revealFileInSystemFileManagerRequested, this, &MainWindow::revealFileInSystemFileManagerRequested);
    connect(ui->startScan_PushButton, &QPushButton::clicked, this, &MainWindow::startScanButtonClicked);

    connect(ui->exportToHtml_Action, &QAction::triggered, this, &MainWindow::exportToHtmlRequested);
    connect(ui->quit_Action, &QAction::triggered, this, &QWidget::close);
    connect(ui->aboutDuplicateFileFinder_Action, &QAction::triggered, this, &MainWindow::showAboutDialog);
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

    ui->scanType_ComboBox->setCurrentIndex(0); // choose "By file name"
}

void MainWindow::showAboutDialog()
{
    QMessageBox aboutDialog(
        QMessageBox::Information,
        QStringLiteral("About Duplicate file finder"),
        QStringLiteral("Duplicate file finder\nVersion %1")
        .arg(QString::fromUtf8(APP_VERSION)),
        QMessageBox::Ok,
        this);
    aboutDialog.exec();
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
    enum class TestType
    {
        SmokeTest,
        ResultPresentationTest,
        BigDirectoryTest
    };

    constexpr TestType testType = TestType::ResultPresentationTest; // adjust test type here

    const QDir fileSystemScenariosDirectory(QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../tests/file_system_scenarios"))));

    switch (testType)
    {
        case TestType::SmokeTest:
            return fileSystemScenariosDirectory.filePath(QStringLiteral("smoke_test"));
        case TestType::ResultPresentationTest:
            return fileSystemScenariosDirectory.filePath(QStringLiteral("result_presentation_test"));
        case TestType::BigDirectoryTest:
            return QStringLiteral("C:/FULL_EXTERNAL_DISK");
    }

    throw std::runtime_error("Invalid TestType");
}
