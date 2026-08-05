#include "main_window.h"
#include "scanner.h"
#include "ui_main_window.h"

#include <QComboBox>
#include <QCloseEvent>
#include <QColor>
#include <QDir>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QTimer>
#include <QCollator>

// TODO move result handling specific code to separate class

namespace
{
    constexpr int resultRowVerticalPadding = 4;
    constexpr int fileNameColumn = 0;
    constexpr int directoriesColumn = 1;
    constexpr int sizesColumn = 2;

    const QColor referenceRowBackgroundColor{0xC0, 0xC0, 0xC0};
    const QColor referenceRowTextColor{0x20, 0x32, 0xE8};
    const QColor duplicateRowBackgroundColor{0xFF, 0xFF, 0xFF};
    const QColor duplicateRowTextColor{0x00, 0x00, 0x00};

    QTableWidgetItem* createResultItem(const QString& text, const QFont& tableFont, const bool isReferenceFile)
    {
        auto* item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item->setBackground(isReferenceFile ? referenceRowBackgroundColor : duplicateRowBackgroundColor);
        item->setForeground(isReferenceFile ? referenceRowTextColor : duplicateRowTextColor);

        QFont itemFont = tableFont;
        itemFont.setBold(isReferenceFile);
        item->setFont(itemFont);

        return item;
    }

    int compareReferenceFiles(const FileRecord& leftFile, const FileRecord& rightFile, const int column, const QCollator& collator)
    {
        switch (column)
        {
            case fileNameColumn:
                return collator.compare(leftFile.getFileName(), rightFile.getFileName());

            case directoriesColumn:
                return collator.compare(leftFile.getDirectoryPath(), rightFile.getDirectoryPath());

            case sizesColumn:
                if (leftFile.getSizeBytes() < rightFile.getSizeBytes())
                {
                    return -1;
                }
                if (leftFile.getSizeBytes() > rightFile.getSizeBytes())
                {
                    return 1;
                }
                return 0;

            default:
                qFatal("Cannot sort results by unknown column %d", column);
        }
    }
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

    // Disable QTableWidget's built-in sorting because it moves rows independently, which would separate duplicate rows from their reference.
    ui->results_TableWidget->setSortingEnabled(false);

    // All sections must be Interactive so both separators expose a drag handle; a Stretch section
    // cannot be resized by the user. Initial proportional widths are assigned once the Results tab
    // is visible and its viewport has its final size.
    QHeaderView* horizontalHeader = ui->results_TableWidget->horizontalHeader();
    horizontalHeader->setSectionResizeMode(0, QHeaderView::Interactive);
    horizontalHeader->setSectionResizeMode(1, QHeaderView::Interactive);
    horizontalHeader->setSectionResizeMode(2, QHeaderView::Interactive);
    horizontalHeader->setSectionsClickable(true);
    horizontalHeader->setSortIndicatorShown(true);
    horizontalHeader->setSortIndicatorClearable(false);
    horizontalHeader->setSortIndicator(fileNameColumn, Qt::AscendingOrder);

    // group-aware sorting when column header is clicked
    connect(horizontalHeader, &QHeaderView::sectionClicked, this, [this, horizontalHeader](const int column)
    {
        const Qt::SortOrder sortOrder = column == resultSortColumn_
                                            ? (resultSortOrder_ == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder)
                                            : Qt::AscendingOrder;

        horizontalHeader->setSortIndicator(column, sortOrder);
        sortResultGroups(column, sortOrder);
    });

    // Each file occupies one row, so all rows can use one compact fixed height.
    QHeaderView* verticalHeader = ui->results_TableWidget->verticalHeader();
    const int resultRowHeight = QFontMetrics(ui->results_TableWidget->font()).height() + resultRowVerticalPadding;
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setMinimumSectionSize(resultRowHeight);
    verticalHeader->setDefaultSectionSize(resultRowHeight);

    connect(ui->results_TableWidget, &QTableWidget::cellDoubleClicked, this, [this](const int row, const int)
    {
        const QTableWidgetItem* fileNameItem = ui->results_TableWidget->item(row, 0);

        if (!fileNameItem)
        {
            qWarning() << "Cannot reveal a result file; row" << row << "has no file name item";
            return;
        }

        emit revealFileInSystemFileManagerRequested(fileNameItem->data(Qt::UserRole).toString());
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
    resultDuplicateGroups_ = scanResult.getDuplicateGroups();
    resultSortColumn_ = fileNameColumn;
    resultSortOrder_ = Qt::AscendingOrder;
    ui->results_TableWidget->horizontalHeader()->setSortIndicator(resultSortColumn_, resultSortOrder_);

    sortResultGroups(resultSortColumn_, resultSortOrder_);

    ui->main_TabWidget->setCurrentWidget(ui->resultsTab);

    if (!resultColumnWidthsInitialized_)
    {
        // Wait until the tab switch and scrollbar layout have established the usable viewport
        // width. This runs only once, so later scans do not overwrite user-adjusted widths.
        QTimer::singleShot(0, this, &MainWindow::initializeResultColumnWidths);
    }
}

void MainWindow::sortResultGroups(const int column, const Qt::SortOrder sortOrder)
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);

    std::ranges::stable_sort(resultDuplicateGroups_, [column, sortOrder, &collator](const DuplicateGroup& leftGroup, const DuplicateGroup& rightGroup)
    {
        const int comparison = compareReferenceFiles(
            leftGroup.getFiles().constFirst(),
            rightGroup.getFiles().constFirst(),
            column,
            collator);

        return sortOrder == Qt::AscendingOrder ? comparison < 0 : comparison > 0;
    });

    resultSortColumn_ = column;
    resultSortOrder_ = sortOrder;
    populateResultTable();
}

void MainWindow::populateResultTable()
{
    ui->results_TableWidget->setUpdatesEnabled(false);
    ui->results_TableWidget->clearContents();

    qsizetype filesInDuplicateGroupsCount = 0;
    for (const DuplicateGroup& duplicateGroup: resultDuplicateGroups_)
    {
        filesInDuplicateGroupsCount += duplicateGroup.getFiles().size();
    }

    ui->results_TableWidget->setRowCount(static_cast<int>(filesInDuplicateGroupsCount));

    int row = 0;
    const QFont tableFont = ui->results_TableWidget->font();

    for (qsizetype groupIndex = 0; groupIndex < resultDuplicateGroups_.size(); ++groupIndex)
    {
        const DuplicateGroup& duplicateGroup = resultDuplicateGroups_.at(groupIndex);
        const QList<FileRecord>& files = duplicateGroup.getFiles();

        for (qsizetype fileIndex = 0; fileIndex < files.size(); ++fileIndex)
        {
            const FileRecord& file = files.at(fileIndex);
            const bool isReferenceFile = fileIndex == 0;

            QTableWidgetItem* fileNameItem = createResultItem(file.getFileName(), tableFont, isReferenceFile);
            fileNameItem->setData(Qt::UserRole, file.getAbsoluteFilePath());

            auto* rowNumberItem = new QTableWidgetItem(isReferenceFile ? QString::number(groupIndex + 1) : QString{});
            rowNumberItem->setTextAlignment(Qt::AlignCenter);

            ui->results_TableWidget->setVerticalHeaderItem(row, rowNumberItem);
            ui->results_TableWidget->setItem(row, fileNameColumn, fileNameItem);
            ui->results_TableWidget->setItem(row, directoriesColumn, createResultItem(file.getDirectoryPath(), tableFont, isReferenceFile));
            ui->results_TableWidget->setItem(row, sizesColumn, createResultItem(QString::number(file.getSizeBytes() / 1024.0, 'f', 2), tableFont, isReferenceFile));

            ++row;
        }
    }

    ui->results_TableWidget->setUpdatesEnabled(true);
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

    ui->results_TableWidget->setColumnWidth(fileNameColumn, fileNameColumnWidth);
    ui->results_TableWidget->setColumnWidth(directoriesColumn, directoriesColumnWidth);
    ui->results_TableWidget->setColumnWidth(sizesColumn, sizesColumnWidth);
    resultColumnWidthsInitialized_ = true;
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
