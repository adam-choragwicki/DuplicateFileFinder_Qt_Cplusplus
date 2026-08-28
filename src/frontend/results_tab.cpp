#include "results_tab.h"
#include "duplicate_results_table_model.h"
#include "ui_results_tab.h"

#include <QClipboard>
#include <QFontMetrics>
#include <QHeaderView>
#include <QMenu>
#include <QTimer>

namespace
{
    constexpr int resultRowVerticalPadding = 4;
}

ResultsTab::ResultsTab(QWidget* parent) : QWidget(parent), ui(new Ui::ResultsTab)
{
    ui->setupUi(this);
    resultsTableModel_ = new DuplicateResultsTableModel(this);
    initializeTable();
}

ResultsTab::~ResultsTab()
{
    delete ui;
}

void ResultsTab::initializeTable()
{
    ui->results_TableView->setModel(resultsTableModel_);
    ui->results_TableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->results_TableView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->results_TableView->setWordWrap(false);

    QHeaderView* horizontalHeader = ui->results_TableView->horizontalHeader();
    horizontalHeader->setSectionResizeMode(DuplicateResultsTableModel::FileNameColumn, QHeaderView::Interactive);
    horizontalHeader->setSectionResizeMode(DuplicateResultsTableModel::DirectoryColumn, QHeaderView::Interactive);
    horizontalHeader->setSectionResizeMode(DuplicateResultsTableModel::SizeColumn, QHeaderView::Interactive);
    horizontalHeader->setSectionsClickable(true);
    horizontalHeader->setSortIndicatorShown(true);
    horizontalHeader->setSortIndicatorClearable(false);
    horizontalHeader->setSortIndicator(DuplicateResultsTableModel::FileNameColumn, Qt::AscendingOrder);

    connect(horizontalHeader, &QHeaderView::sectionClicked, this, [this, horizontalHeader](const int column)
    {
        const Qt::SortOrder sortOrder = column == resultsTableModel_->getSortColumn()
                                            ? (resultsTableModel_->getSortOrder() == Qt::AscendingOrder
                                                   ? Qt::DescendingOrder
                                                   : Qt::AscendingOrder)
                                            : Qt::AscendingOrder;

        horizontalHeader->setSortIndicator(column, sortOrder);
        resultsTableModel_->sort(column, sortOrder);
    });

    QHeaderView* verticalHeader = ui->results_TableView->verticalHeader();
    const int resultRowHeight = QFontMetrics(ui->results_TableView->font()).height() + resultRowVerticalPadding;
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setMinimumSectionSize(resultRowHeight);
    verticalHeader->setDefaultSectionSize(resultRowHeight);

    connect(ui->results_TableView, &QTableView::doubleClicked, this,
            [this](const QModelIndex& index)
            {
                revealFileInSystemFileManager(index.row());
            });

    connect(ui->results_TableView, &QTableView::customContextMenuRequested,
            this, &ResultsTab::showResultContextMenu);
}

/// Get absolute file path of an item in a given row. The item represents a file.
QString ResultsTab::getAbsoluteFilePathForRow(const int row) const
{
    const QModelIndex fileNameIndex = resultsTableModel_->index(row, DuplicateResultsTableModel::FileNameColumn);

    if (!fileNameIndex.isValid())
    {
        qWarning() << "Cannot access a result file; row" << row << "is invalid";
        return {};
    }

    const QString absoluteFilePath = fileNameIndex.data(DuplicateResultsTableModel::AbsoluteFilePathRole).toString();

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
    const QModelIndex clickedIndex = ui->results_TableView->indexAt(position);

    if (!clickedIndex.isValid())
    {
        return;
    }

    const QString absoluteFilePath = getAbsoluteFilePathForRow(clickedIndex.row());

    if (absoluteFilePath.isEmpty())
    {
        return;
    }

    ui->results_TableView->setCurrentIndex(clickedIndex);

    QMenu contextMenu(ui->results_TableView);
    QAction* revealFileInSystemFileManagerAction = contextMenu.addAction(tr("Reveal in file manager"));
    QAction* copyFilePathAction = contextMenu.addAction(tr("Copy path"));
    const QAction* selectedAction = contextMenu.exec(ui->results_TableView->viewport()->mapToGlobal(position));

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
    resultsTableModel_->setDuplicateGroups(duplicateGroups);
    ui->results_TableView->horizontalHeader()->setSortIndicator(DuplicateResultsTableModel::FileNameColumn,
                                                                Qt::AscendingOrder);

    if (!columnWidthsInitialized_)
    {
        // The main window selects this tab immediately after providing the result. Defer until its
        // viewport has the final visible size and any scrollbar has been laid out.
        QTimer::singleShot(0, this, &ResultsTab::initializeColumnWidths);
    }
}

void ResultsTab::clearDuplicateGroups()
{
    resultsTableModel_->clearDuplicateGroups();
}

const QList<DuplicateGroup>& ResultsTab::getDisplayedDuplicateGroups() const
{
    return resultsTableModel_->getDuplicateGroups();
}

void ResultsTab::initializeColumnWidths()
{
    if (columnWidthsInitialized_)
    {
        return;
    }

    const int widthAvailableForColumns = ui->results_TableView->viewport()->width();

    if (widthAvailableForColumns <= 0)
    {
        qWarning() << "Cannot initialize result column widths; results table viewport width is" << widthAvailableForColumns;
        return;
    }

    constexpr int totalColumnWidthWeight = 10;
    constexpr int fileNameColumnWidthWeight = 3;
    constexpr int sizesColumnWidthWeight = 1;
    constexpr int directoriesColumnWidthWeight = 6;

    static_assert(fileNameColumnWidthWeight + sizesColumnWidthWeight + directoriesColumnWidthWeight
                  == totalColumnWidthWeight,
                  "Column width weights must sum to the total column width weight");

    const int fileNameColumnWidth = widthAvailableForColumns * fileNameColumnWidthWeight / totalColumnWidthWeight;
    const int sizesColumnWidth = widthAvailableForColumns * sizesColumnWidthWeight / totalColumnWidthWeight;
    const int directoriesColumnWidth = widthAvailableForColumns - fileNameColumnWidth - sizesColumnWidth;

    ui->results_TableView->setColumnWidth(DuplicateResultsTableModel::FileNameColumn, fileNameColumnWidth);
    ui->results_TableView->setColumnWidth(DuplicateResultsTableModel::DirectoryColumn, directoriesColumnWidth);
    ui->results_TableView->setColumnWidth(DuplicateResultsTableModel::SizeColumn, sizesColumnWidth);
    columnWidthsInitialized_ = true;
}
