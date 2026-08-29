#pragma once

#include "types/duplicate_group.h"

#include <QAbstractTableModel>
#include <QVector>

class QColor;
class QCollator;

/// @brief Adapts duplicate groups to the rows, columns, and roles consumed by the results table view.
///
/// This is a Qt presentation model, distinct from the application's authoritative `Model`. It owns a
/// display copy so that sorting can change the presentation order without changing application state.
/// Each file occupies one table row, while the files belonging to a duplicate group remain consecutive.
/// The first file in every group is treated as its reference file and receives the group's vertical-header
/// number and reference-row styling.
///
/// Sorting rearranges whole groups by their reference files; it never rearranges files within a group.
class DuplicateResultsTableModel final : public QAbstractTableModel
{
public:
    /// Logical columns exposed to the view. `ColumnCount` is a sentinel rather than a visible column.
    enum Column
    {
        FileNameColumn,
        DirectoryColumn,
        SizeColumn,
        ColumnCount
    };

    /// Additional data that consumers can request without parsing display text.
    enum DataRole
    {
        /// Absolute path of the file represented by the row, used by context-menu and double-click actions.
        AbsoluteFilePathRole = Qt::UserRole
    };

    explicit DuplicateResultsTableModel(QObject* parent = nullptr);

    /// Returns the number of flattened file rows. Valid parents have no children because this is a table.
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    /// Returns the number of visible columns for the root index only.
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    /// Supplies display text, row styling, alignment, and the custom absolute-path role.
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    /// Returns display text for column headers and group numbers for vertical row headers.
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    /// Returns enabled and selectable flags for valid file indexes, and no flags for invalid indexes.
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    /// Sorts duplicate groups as indivisible units using the requested reference-file column.
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    /// Replaces the display copy, restores the default file-name sort, and rebuilds the flattened rows.
    /// Every group in `duplicateGroups` must contain at least one file.
    void setDuplicateGroups(const QList<DuplicateGroup>& duplicateGroups);
    /// Removes all displayed groups and rows.
    void clearDuplicateGroups();
    /// Returns the model-owned groups in their current presentation order.
    [[nodiscard]] const QList<DuplicateGroup>& getDuplicateGroups() const;
    /// Returns the column used for the most recent accepted sort request.
    [[nodiscard]] int getSortColumn() const;
    /// Returns the direction used for the most recent accepted sort request.
    [[nodiscard]] Qt::SortOrder getSortOrder() const;

private:
    /// Maps one flat table row back to its owning group and its position within that group.
    /// Entries contain indices rather than pointers so rebuilding the list after a group sort is safe.
    struct RowEntry
    {
        qsizetype groupIndex;
        qsizetype fileIndex;
    };

    // Reference rows introduce a duplicate group; contrasting colors make group boundaries visible without
    // inserting non-file separator rows into the table model.
    static const QColor referenceRowBackgroundColor_;
    static const QColor referenceRowTextColor_;
    static const QColor duplicateRowBackgroundColor_;
    static const QColor duplicateRowTextColor_;

    /// Compares two group-reference files by the requested table column.
    [[nodiscard]] static int compareReferenceFiles(const FileRecord& leftFile,
                                                   const FileRecord& rightFile,
                                                   int column,
                                                   const QCollator& collator);
    /// Reorders `duplicateGroups_` according to the saved sort settings.
    void sortGroups();
    /// Recreates the row-to-file lookup after the groups or their order change.
    void rebuildRowEntries();
    /// Resolves a valid table index to its file, or returns `nullptr` for an invalid/out-of-range index.
    [[nodiscard]] const FileRecord* fileForIndex(const QModelIndex& index) const;

    /// Presentation-owned copy; mutations here never change the scan result stored by the application model.
    QList<DuplicateGroup> duplicateGroups_;
    /// Cached flattening of `duplicateGroups_`, in exactly the order visible to the table.
    QVector<RowEntry> rowEntries_;
    /// Sort state is retained so the view can keep its header indicator synchronized with the model.
    int sortColumn_{FileNameColumn};
    Qt::SortOrder sortOrder_{Qt::AscendingOrder};
};
