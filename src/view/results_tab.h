#pragma once

#include "types/duplicate_group.h"

#include <QWidget>

namespace Ui
{
    class ResultsTab;
}

class ResultsTab : public QWidget
{
    Q_OBJECT

signals:
    void revealFileInSystemFileManagerRequested(const QString& absoluteFilePath);

public:
    explicit ResultsTab(QWidget* parent = nullptr);
    ~ResultsTab() override;

    void showDuplicateGroups(const QList<DuplicateGroup>& duplicateGroups);
    void clearDuplicateGroups();
    [[nodiscard]] const QList<DuplicateGroup>& getDisplayedDuplicateGroups() const;

private:
    void initializeTable();
    void initializeColumnWidths();
    [[nodiscard]] QString getAbsoluteFilePathForRow(int row) const;
    void revealFileInSystemFileManager(int row);
    void showResultContextMenu(const QPoint& position);
    void sortResultGroups(int column, Qt::SortOrder sortOrder);
    void rebuildResultsTable();

    Ui::ResultsTab* ui;
    QList<DuplicateGroup> duplicateGroups_;
    int sortColumn_{};
    Qt::SortOrder sortOrder_{Qt::AscendingOrder};
    bool columnWidthsInitialized_{};
};
