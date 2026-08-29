#include "duplicate_results_table_model.h"
#include "file_size_formatter.h"

#include <QBrush>
#include <QColor>
#include <QCollator>
#include <QFont>
#include <QLocale>

#include <algorithm>

const QColor DuplicateResultsTableModel::referenceRowBackgroundColor_{0xC0, 0xC0, 0xC0};
const QColor DuplicateResultsTableModel::referenceRowTextColor_{0x20, 0x32, 0xE8};
const QColor DuplicateResultsTableModel::duplicateRowBackgroundColor_{0xFF, 0xFF, 0xFF};
const QColor DuplicateResultsTableModel::duplicateRowTextColor_{0x00, 0x00, 0x00};

DuplicateResultsTableModel::DuplicateResultsTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int DuplicateResultsTableModel::compareReferenceFiles(const FileRecord& leftFile, const FileRecord& rightFile, const int column, const QCollator& collator)
{
    // Return a three-way comparison so one helper can support both sort directions. Text comparison is
    // delegated to QCollator for locale-aware, case-insensitive natural ordering (for example, file2 before
    // file10); size remains a numeric comparison and must not use its formatted display text.
    switch (column)
    {
        case FileNameColumn:
            return collator.compare(leftFile.getFileName(), rightFile.getFileName());

        case DirectoryColumn:
            return collator.compare(leftFile.getDirectoryPath(), rightFile.getDirectoryPath());

        case SizeColumn:
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
            // Callers validate the column before sorting, so reaching this branch indicates a programming error rather than malformed user input.
            qFatal("Cannot sort results by unknown column %d", column);
    }
}

int DuplicateResultsTableModel::rowCount(const QModelIndex& parent) const
{
    // QAbstractTableModel is flat: only the invalid root index owns rows.
    return parent.isValid() ? 0 : rowEntries_.size();
}

int DuplicateResultsTableModel::columnCount(const QModelIndex& parent) const
{
    // Returning zero for valid parents reinforces that no cell can act as a parent of more columns.
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant DuplicateResultsTableModel::data(const QModelIndex& index, const int role) const
{
    const FileRecord* file = fileForIndex(index);

    if (!file)
    {
        return {};
    }

    const RowEntry& rowEntry = rowEntries_.at(index.row());
    // The scan result preserves the first file as the representative/reference for its duplicate group.
    const bool isReferenceFile = rowEntry.fileIndex == 0;

    // A single QModelIndex can be queried repeatedly for different roles. Keep the exact path and visual
    // presentation separate from the user-facing strings returned for DisplayRole.
    switch (role)
    {
        case Qt::DisplayRole:
            switch (index.column())
            {
                case FileNameColumn:
                    return file->getFileName();
                case DirectoryColumn:
                    return file->getDirectoryPath();
                case SizeColumn:
                    return FileSizeFormatter::format(file->getSizeBytes());
                default:
                    return {};
            }

        case Qt::TextAlignmentRole:
            // Right-aligned sizes are easier to compare vertically; textual columns retain normal reading order.
            return QVariant::fromValue(index.column() == SizeColumn
                                           ? Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter)
                                           : Qt::Alignment(Qt::AlignLeft | Qt::AlignVCenter));

        case Qt::BackgroundRole:
            return QBrush(isReferenceFile ? referenceRowBackgroundColor_ : duplicateRowBackgroundColor_);

        case Qt::ForegroundRole:
            return QBrush(isReferenceFile ? referenceRowTextColor_ : duplicateRowTextColor_);

        case Qt::FontRole:
            if (isReferenceFile)
            {
                // Set only the weight so the delegate can resolve the remaining properties from the view's font.
                QFont font;
                font.setBold(true);
                return font;
            }
            return {};

        case AbsoluteFilePathRole:
            // Consumers use this value for file actions, avoiding reconstruction from two formatted columns.
            return file->getAbsoluteFilePath();

        default:
            return {};
    }
}

QVariant DuplicateResultsTableModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
            case FileNameColumn:
                return QStringLiteral("File name");
            case DirectoryColumn:
                return QStringLiteral("Directories");
            case SizeColumn:
                return QStringLiteral("Size");
            default:
                return {};
        }
    }

    if (orientation == Qt::Vertical && section >= 0 && section < rowEntries_.size())
    {
        const RowEntry& rowEntry = rowEntries_.at(section);

        if (role == Qt::DisplayRole)
        {
            // Label a group only once, next to its reference row. The one-based value is intended for users; groupIndex itself remains zero-based for container access.
            return rowEntry.fileIndex == 0 ? QString::number(rowEntry.groupIndex + 1) : QString{};
        }

        if (role == Qt::TextAlignmentRole)
        {
            return QVariant::fromValue(Qt::Alignment(Qt::AlignCenter));
        }
    }

    return {};
}

Qt::ItemFlags DuplicateResultsTableModel::flags(const QModelIndex& index) const
{
    // Results are read-only but can be selected for file actions.
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

void DuplicateResultsTableModel::sort(const int column, const Qt::SortOrder order)
{
    // Header views may request a sort using an invalid section while being initialized or reconfigured.
    if (column < FileNameColumn || column >= ColumnCount)
    {
        return;
    }

    // Reordering groups changes the file represented by most row numbers. A model reset invalidates stale
    // indexes and tells every attached view to fetch both cell data and vertical headers again.
    beginResetModel();
    sortColumn_ = column;
    sortOrder_ = order;
    sortGroups();
    rebuildRowEntries();
    endResetModel();
}

void DuplicateResultsTableModel::setDuplicateGroups(const QList<DuplicateGroup>& duplicateGroups)
{
    // Copying creates a presentation snapshot that can be sorted independently of the authoritative result.
    beginResetModel();
    duplicateGroups_ = duplicateGroups;
    // Each newly supplied result starts from a predictable order regardless of the previous table state.
    sortColumn_ = FileNameColumn;
    sortOrder_ = Qt::AscendingOrder;
    sortGroups();
    rebuildRowEntries();
    endResetModel();
}

void DuplicateResultsTableModel::clearDuplicateGroups()
{
    // Clearing both containers together preserves the invariant that every RowEntry resolves into a group.
    beginResetModel();
    duplicateGroups_.clear();
    rowEntries_.clear();
    endResetModel();
}

const QList<DuplicateGroup>& DuplicateResultsTableModel::getDuplicateGroups() const
{
    return duplicateGroups_;
}

int DuplicateResultsTableModel::getSortColumn() const
{
    return sortColumn_;
}

Qt::SortOrder DuplicateResultsTableModel::getSortOrder() const
{
    return sortOrder_;
}

void DuplicateResultsTableModel::sortGroups()
{
    // The C locale uses lexical collation on some platforms even when numeric mode is requested. Fall back to
    // English collation there so natural filename ordering remains available in C-locale environments, while
    // retaining the user's collation rules for every fully featured locale.
    const QLocale defaultCollationLocale = QLocale().collation();
    const QLocale collationLocale = defaultCollationLocale.language() == QLocale::C
                                        ? QLocale(QLocale::English, QLocale::UnitedStates)
                                        : defaultCollationLocale;
    QCollator collator(collationLocale);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);

    // Compare only each group's first file so the group remains an indivisible display unit. stable_sort also
    // preserves the incoming relative order of groups whose reference values compare equal.


    std::ranges::stable_sort(duplicateGroups_, [this, &collator](const DuplicateGroup& leftGroup, const DuplicateGroup& rightGroup)
    {
        // This lambda is the ordering predicate used by stable_sort. For each pair of groups, it compares their
        // reference files and returns true only when leftGroup should appear before rightGroup according to the
        // model's current sort column and direction.
        const int comparison = compareReferenceFiles(leftGroup.getFiles().constFirst(),
                                                     rightGroup.getFiles().constFirst(),
                                                     sortColumn_,
                                                     collator);

        return sortOrder_ == Qt::AscendingOrder ? comparison < 0 : comparison > 0;
    });
}

void DuplicateResultsTableModel::rebuildRowEntries()
{
    rowEntries_.clear();

    // Flatten groups in presentation order without copying FileRecord objects a second time. Appending every
    // group's files consecutively is what guarantees that duplicates stay adjacent to their reference row.
    for (qsizetype groupIndex = 0; groupIndex < duplicateGroups_.size(); ++groupIndex)
    {
        const QList<FileRecord>& files = duplicateGroups_.at(groupIndex).getFiles();

        for (qsizetype fileIndex = 0; fileIndex < files.size(); ++fileIndex)
        {
            rowEntries_.append(RowEntry{groupIndex, fileIndex});
        }
    }
}

const FileRecord* DuplicateResultsTableModel::fileForIndex(const QModelIndex& index) const
{
    // Validate both dimensions before touching rowEntries_; Qt can query models with stale or synthetic indexes.
    if (!index.isValid() || index.row() < 0 || index.row() >= rowEntries_.size()
        || index.column() < FileNameColumn || index.column() >= ColumnCount)
    {
        return nullptr;
    }

    const RowEntry& rowEntry = rowEntries_.at(index.row());
    // The pointer remains valid until the next reset operation mutates or reorders duplicateGroups_.
    return &duplicateGroups_.at(rowEntry.groupIndex).getFiles().at(rowEntry.fileIndex);
}
