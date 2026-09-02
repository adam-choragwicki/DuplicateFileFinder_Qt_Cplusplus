#pragma once

#include "types/duplicate_group.h"

#include <QWidget>

class DuplicateResultsTableModel;

namespace Ui
{
    class ResultsTab;
}

/// @brief Presents duplicate groups in a sortable table with file-reveal and path-copy actions.
class ResultsTab : public QWidget
{
    Q_OBJECT

signals:
    /// Emitted when a valid result row requests that its file be revealed by the controller.
    void revealFileInSystemFileManagerRequested(const QString& absoluteFilePath);

public:
    /// Creates and initializes the results table and its presentation model.
    /// @param[in] parent Optional QWidget owner.
    explicit ResultsTab(QWidget* parent = nullptr);
    /// Releases the generated Qt Designer UI object.
    ~ResultsTab() override;

    /// Replaces displayed groups, resets the sort indicator, and initializes proportional column widths when needed.
    /// @param[in] duplicateGroups Groups to copy into the presentation model.
    void showDuplicateGroups(const QList<DuplicateGroup>& duplicateGroups);
    /// Removes every displayed duplicate group.
    void clearDuplicateGroups();
    /// Returns groups in their current table presentation order.
    [[nodiscard]] const QList<DuplicateGroup>& getDisplayedDuplicateGroups() const;

private:
    /// Configures table behavior, headers, sorting, double-click handling, and the context menu.
    void initializeTable();
    /// Applies initial 30/60/10 percent column widths once a usable viewport size is available.
    void initializeColumnWidths();
    /// Resolves a table row to the model's absolute-path role, returning an empty string for invalid data.
    /// @param[in] row Zero-based result row.
    [[nodiscard]] QString getAbsoluteFilePathForRow(int row) const;
    /// Emits a reveal request when the supplied row resolves to a non-empty path.
    /// @param[in] row Zero-based result row.
    void revealFileInSystemFileManager(int row);
    /// Shows reveal and copy-path actions for the result row under the supplied viewport position.
    /// @param[in] position Position relative to the results-table viewport.
    void showResultContextMenu(const QPoint& position);

    /// Generated Qt Designer widget hierarchy owned and deleted by this tab.
    Ui::ResultsTab* ui;
    /// Presentation model owned through QObject parenting by this tab.
    DuplicateResultsTableModel* resultsTableModel_{};
    /// Prevents user-adjusted widths from being overwritten after their initial setup.
    bool columnWidthsInitialized_{};
};
