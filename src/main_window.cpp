#include "main_window.h"
#include "scanner.h"
#include "ui_main_window.h"

#include <QComboBox>
#include <QCloseEvent>
#include <QDir>
#include <QFontMetrics>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QTimer>

namespace
{
    constexpr int cellVerticalPadding = 20;
}

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

void MainWindow::initializeResultTabColumns()
{
    ui->results_TableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->results_TableWidget->setWordWrap(false);

    // All sections must be Interactive so both separators expose a drag handle; a Stretch section
    // cannot be resized by the user. Initial proportional widths are assigned once the Results tab
    // is visible and its viewport has its final size.
    QHeaderView* horizontalHeader = ui->results_TableWidget->horizontalHeader();
    horizontalHeader->setSectionResizeMode(0, QHeaderView::Interactive);
    horizontalHeader->setSectionResizeMode(1, QHeaderView::Interactive);
    horizontalHeader->setSectionResizeMode(2, QHeaderView::Interactive);

    // Sorting moves groups to different row indexes. Reapply their stored line-count heights after
    // the table has finished sorting so every group remains fully visible without auto-sizing.
    connect(horizontalHeader, &QHeaderView::sortIndicatorChanged, this, [this]
    {
        QTimer::singleShot(0, this, &MainWindow::updateResultRowHeights);
    });
}

void MainWindow::initializeUI()
{
    // Clear placeholder values from combo boxes
    ui->scanType_ComboBox->clear();

    ui->directoryPath_Label->setText(getInitialDirectoryScanPath());

    initializeResultTabColumns();
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

        auto* fileNameItem = new QTableWidgetItem(files.constFirst().getFileName());
        auto* directoriesItem = new QTableWidgetItem(directories.join('\n'));
        auto* sizesItem = new QTableWidgetItem(sizes.join('\n'));

        fileNameItem->setTextAlignment(Qt::AlignLeft);
        directoriesItem->setTextAlignment(Qt::AlignLeft);
        sizesItem->setTextAlignment(Qt::AlignLeft);
        directoriesItem->setData(Qt::UserRole, files.size());

        ui->results_TableWidget->setItem(static_cast<int>(row), 0, fileNameItem);
        ui->results_TableWidget->setItem(static_cast<int>(row), 1, directoriesItem);
        ui->results_TableWidget->setItem(static_cast<int>(row), 2, sizesItem);
    }

    ui->results_TableWidget->setSortingEnabled(true);
    updateResultRowHeights();
    ui->results_TableWidget->setUpdatesEnabled(true);
    ui->main_TabWidget->setCurrentWidget(ui->resultsTab);

    if (!resultColumnWidthsInitialized_)
    {
        // Wait until the tab switch and scrollbar layout have established the usable viewport
        // width. This runs only once, so later scans do not overwrite user-adjusted widths.
        QTimer::singleShot(0, this, &MainWindow::initializeResultColumnWidths);
    }
}

void MainWindow::initializeResultColumnWidths()
{
    if (resultColumnWidthsInitialized_)
    {
        return;
    }

    const int widthAvailableForColumns = ui->results_TableWidget->viewport()->width();

    if (widthAvailableForColumns <= 0)
    {
        qWarning() << "Cannot initialize result column widths; results table viewport width is" << widthAvailableForColumns;
        return;
    }

    constexpr int totalColumnWidthWeight = 10;

    constexpr int fileNameColumnWidthWeight = 3; // take 30% of horizontal width
    constexpr int sizesColumnWidthWeight = 1; // take 10% of horizontal width
    constexpr int directoriesColumnWidthWeight = 6; // take 60% of horizontal width

    static_assert(fileNameColumnWidthWeight + sizesColumnWidthWeight + directoriesColumnWidthWeight == totalColumnWidthWeight,
                  "Column width weights must sum to the total column width weight");

    const int fileNameColumnWidth = widthAvailableForColumns * fileNameColumnWidthWeight / totalColumnWidthWeight;
    const int sizesColumnWidth = widthAvailableForColumns * sizesColumnWidthWeight / totalColumnWidthWeight;
    // Give the directories column any pixels left by integer division so the columns exactly fill the viewport.
    const int directoriesColumnWidth = widthAvailableForColumns - fileNameColumnWidth - sizesColumnWidth;

    ui->results_TableWidget->setColumnWidth(0, fileNameColumnWidth);
    ui->results_TableWidget->setColumnWidth(1, directoriesColumnWidth);
    ui->results_TableWidget->setColumnWidth(2, sizesColumnWidth);
    resultColumnWidthsInitialized_ = true;
}

void MainWindow::updateResultRowHeights()
{
    const int lineHeight = QFontMetrics(ui->results_TableWidget->font()).lineSpacing();

    for (int row = 0; row < ui->results_TableWidget->rowCount(); ++row)
    {
        const QTableWidgetItem* directoriesItem = ui->results_TableWidget->item(row, 1);
        const int lineCount = directoriesItem ? qMax(directoriesItem->data(Qt::UserRole).toInt(), 1) : 1;
        ui->results_TableWidget->setRowHeight(row, lineCount * lineHeight + cellVerticalPadding);
    }
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
