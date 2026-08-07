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
    [[nodiscard]] const QList<DuplicateGroup>& getDisplayedDuplicateGroups() const;

private:
    void initializeTable();
    void initializeColumnWidths();
    void sortResultGroups(int column, Qt::SortOrder sortOrder);
    void populateTable();

    Ui::ResultsTab* ui;
    QList<DuplicateGroup> duplicateGroups_;
    int sortColumn_{};
    Qt::SortOrder sortOrder_{Qt::AscendingOrder};
    bool columnWidthsInitialized_{};
};
