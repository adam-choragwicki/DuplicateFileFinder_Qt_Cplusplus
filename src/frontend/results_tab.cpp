#include "results_tab.h"
#include "ui_results_tab.h"

#include <QCollator>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QTimer>
#include <QClipboard>
#include <QMenu>

#include <algorithm>

namespace
{
    constexpr int resultRowVerticalPadding = 4;
    constexpr int fileNameColumn = 0;
    constexpr int directoriesColumn = 1;
    constexpr int sizesColumn = 2;

    constexpr QColor referenceRowBackgroundColor{0xC0, 0xC0, 0xC0};
    constexpr QColor referenceRowTextColor{0x20, 0x32, 0xE8};
    constexpr QColor duplicateRowBackgroundColor{0xFF, 0xFF, 0xFF};
    constexpr QColor duplicateRowTextColor{0x00, 0x00, 0x00};

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

    [[nodiscard]] QString formatFileSize(const qint64 sizeBytes)
    {
        const QLocale locale;

        if (sizeBytes < 1024)
        {
            return QStringLiteral("%1 B").arg(locale.toString(sizeBytes));
        }

        return locale.formattedDataSize(sizeBytes, 2, QLocale::DataSizeTraditionalFormat);
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

ResultsTab::ResultsTab(QWidget* parent) : QWidget(parent), ui(new Ui::ResultsTab)
{
    ui->setupUi(this);
    initializeTable();
}

ResultsTab::~ResultsTab()
{
    delete ui;
}

void ResultsTab::initializeTable()
{
    ui->results_TableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->results_TableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->results_TableWidget->setWordWrap(false);

    // Disable sorting because QTableWidget sorts individual rows, which would separate duplicates from their reference file.
    ui->results_TableWidget->setSortingEnabled(false);

    QHeaderView* horizontalHeader = ui->results_TableWidget->horizontalHeader();
    horizontalHeader->setSectionResizeMode(fileNameColumn, QHeaderView::Interactive);
    horizontalHeader->setSectionResizeMode(directoriesColumn, QHeaderView::Interactive);
    horizontalHeader->setSectionResizeMode(sizesColumn, QHeaderView::Interactive);
    horizontalHeader->setSectionsClickable(true);
    horizontalHeader->setSortIndicatorShown(true);
    horizontalHeader->setSortIndicatorClearable(false);
    horizontalHeader->setSortIndicator(fileNameColumn, Qt::AscendingOrder);

    connect(horizontalHeader, &QHeaderView::sectionClicked, this, [this, horizontalHeader](const int column)
    {
        const Qt::SortOrder sortOrder = column == sortColumn_
                                            ? (sortOrder_ == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder)
                                            : Qt::AscendingOrder;

        horizontalHeader->setSortIndicator(column, sortOrder);
        sortResultGroups(column, sortOrder);
    });

    QHeaderView* verticalHeader = ui->results_TableWidget->verticalHeader();
    const int resultRowHeight = QFontMetrics(ui->results_TableWidget->font()).height() + resultRowVerticalPadding;
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setMinimumSectionSize(resultRowHeight);
    verticalHeader->setDefaultSectionSize(resultRowHeight);

    connect(ui->results_TableWidget, &QTableWidget::cellDoubleClicked, this, [this](const int row, const int)
    {
        revealFileInSystemFileManager(row);
    });

    connect(ui->results_TableWidget, &QTableWidget::customContextMenuRequested, this, &ResultsTab::showResultContextMenu);
}

/// Get absolute file path of an item in a given row. The item represents a file.
QString ResultsTab::getAbsoluteFilePathForRow(const int row) const
{
    const QTableWidgetItem* fileNameItem = ui->results_TableWidget->item(row, fileNameColumn);

    if (!fileNameItem)
    {
        qWarning() << "Cannot access a result file; row" << row << "has no file name item";
        return {};
    }

    const QString absoluteFilePath = fileNameItem->data(Qt::UserRole).toString();

    if (absoluteFilePath.isEmpty())
    {
        qWarning() << "Cannot access a result file; row" << row << "has no absolute file path";
    }

    return absoluteFilePath;
}

void ResultsTab::revealFileInSystemFileManager(const int row)
{
    const QString absoluteFilePath = getAbsoluteFilePathForRow(row);

    if (!absoluteFilePath.isEmpty())
    {
        emit revealFileInSystemFileManagerRequested(absoluteFilePath);
    }
}

void ResultsTab::showResultContextMenu(const QPoint& position)
{
    const QModelIndex clickedIndex = ui->results_TableWidget->indexAt(position);

    if (!clickedIndex.isValid())
    {
        return;
    }

    const QString absoluteFilePath = getAbsoluteFilePathForRow(clickedIndex.row());

    if (absoluteFilePath.isEmpty())
    {
        return;
    }

    ui->results_TableWidget->setCurrentIndex(clickedIndex);

    QMenu contextMenu(ui->results_TableWidget);
    QAction* revealFileInSystemFileManagerAction = contextMenu.addAction(tr("Reveal in file manager"));
    QAction* copyFilePathAction = contextMenu.addAction(tr("Copy path"));
    const QAction* selectedAction = contextMenu.exec(ui->results_TableWidget->viewport()->mapToGlobal(position));

    if (selectedAction == revealFileInSystemFileManagerAction)
    {
        revealFileInSystemFileManager(clickedIndex.row());
    }
    else if (selectedAction == copyFilePathAction)
    {
        QApplication::clipboard()->setText(QDir::toNativeSeparators(absoluteFilePath));
    }
}

void ResultsTab::showDuplicateGroups(const QList<DuplicateGroup>& duplicateGroups)
{
    duplicateGroups_ = duplicateGroups;
    sortColumn_ = fileNameColumn;
    sortOrder_ = Qt::AscendingOrder;
    ui->results_TableWidget->horizontalHeader()->setSortIndicator(sortColumn_, sortOrder_);

    sortResultGroups(sortColumn_, sortOrder_);

    if (!columnWidthsInitialized_)
    {
        // The main window selects this tab immediately after providing the result. Defer until its
        // viewport has the final visible size and any scrollbar has been laid out.
        QTimer::singleShot(0, this, &ResultsTab::initializeColumnWidths);
    }
}

void ResultsTab::clearDuplicateGroups()
{
    duplicateGroups_.clear();
    rebuildResultsTable();
}

const QList<DuplicateGroup>& ResultsTab::getDisplayedDuplicateGroups() const
{
    return duplicateGroups_;
}

void ResultsTab::sortResultGroups(const int column, const Qt::SortOrder sortOrder)
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);

    std::ranges::stable_sort(duplicateGroups_, [column, sortOrder, &collator](const DuplicateGroup& leftGroup, const DuplicateGroup& rightGroup)
    {
        const int comparison = compareReferenceFiles(
            leftGroup.getFiles().constFirst(),
            rightGroup.getFiles().constFirst(),
            column,
            collator);

        return sortOrder == Qt::AscendingOrder ? comparison < 0 : comparison > 0;
    });

    sortColumn_ = column;
    sortOrder_ = sortOrder;
    rebuildResultsTable();
}

void ResultsTab::rebuildResultsTable()
{
    ui->results_TableWidget->setUpdatesEnabled(false);
    ui->results_TableWidget->clearContents();

    qsizetype filesInDuplicateGroupsCount = 0;
    for (const DuplicateGroup& duplicateGroup: duplicateGroups_)
    {
        filesInDuplicateGroupsCount += duplicateGroup.getFiles().size();
    }

    ui->results_TableWidget->setRowCount(static_cast<int>(filesInDuplicateGroupsCount));

    int row = 0;
    const QFont tableFont = ui->results_TableWidget->font();

    for (qsizetype groupIndex = 0; groupIndex < duplicateGroups_.size(); ++groupIndex)
    {
        const DuplicateGroup& duplicateGroup = duplicateGroups_.at(groupIndex);
        const QList<FileRecord>& files = duplicateGroup.getFiles();

        for (qsizetype fileIndex = 0; fileIndex < files.size(); ++fileIndex)
        {
            const FileRecord& file = files.at(fileIndex);
            const bool isReferenceFile = fileIndex == 0;

            QTableWidgetItem* fileNameItem = createResultItem(file.getFileName(), tableFont, isReferenceFile);
            fileNameItem->setData(Qt::UserRole, file.getAbsoluteFilePath());

            QTableWidgetItem* sizeItem = createResultItem(formatFileSize(file.getSizeBytes()),
                                                          tableFont,
                                                          isReferenceFile);

            sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto* rowNumberItem = new QTableWidgetItem(isReferenceFile ? QString::number(groupIndex + 1) : QString{});
            rowNumberItem->setTextAlignment(Qt::AlignCenter);

            ui->results_TableWidget->setVerticalHeaderItem(row, rowNumberItem);
            ui->results_TableWidget->setItem(row, fileNameColumn, fileNameItem);
            ui->results_TableWidget->setItem(row, directoriesColumn, createResultItem(file.getDirectoryPath(), tableFont, isReferenceFile));
            ui->results_TableWidget->setItem(row, sizesColumn, sizeItem);

            ++row;
        }
    }

    ui->results_TableWidget->setUpdatesEnabled(true);
}

void ResultsTab::initializeColumnWidths()
{
    if (columnWidthsInitialized_)
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
    constexpr int fileNameColumnWidthWeight = 3;
    constexpr int sizesColumnWidthWeight = 1;
    constexpr int directoriesColumnWidthWeight = 6;

    static_assert(fileNameColumnWidthWeight + sizesColumnWidthWeight + directoriesColumnWidthWeight == totalColumnWidthWeight,
                  "Column width weights must sum to the total column width weight");

    const int fileNameColumnWidth = widthAvailableForColumns * fileNameColumnWidthWeight / totalColumnWidthWeight;
    const int sizesColumnWidth = widthAvailableForColumns * sizesColumnWidthWeight / totalColumnWidthWeight;
    const int directoriesColumnWidth = widthAvailableForColumns - fileNameColumnWidth - sizesColumnWidth;

    ui->results_TableWidget->setColumnWidth(fileNameColumn, fileNameColumnWidth);
    ui->results_TableWidget->setColumnWidth(directoriesColumn, directoriesColumnWidth);
    ui->results_TableWidget->setColumnWidth(sizesColumn, sizesColumnWidth);
    columnWidthsInitialized_ = true;
}
