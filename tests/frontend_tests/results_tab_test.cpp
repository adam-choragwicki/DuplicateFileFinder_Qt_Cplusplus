#include "frontend/results_tab.h"

#include <QHeaderView>
#include <QTableWidget>

#include <gtest/gtest.h>

namespace
{
    /// @brief Zero-based index of the results-table column containing file names.
    constexpr int fileNameColumn = 0;

    /// @brief Zero-based index of the results-table column containing directory paths.
    constexpr int directoriesColumn = 1;

    /// @brief Zero-based index of the results-table column containing file sizes.
    constexpr int sizesColumn = 2;

    /// @brief Creates a duplicate group from file records supplied in their intended display order.
    ///
    /// @param files File records to append to the group. The first record becomes the reference file.
    /// @return A duplicate group containing copies of all supplied records in the same order.
    DuplicateGroup createDuplicateGroup(const std::initializer_list<FileRecord> files)
    {
        DuplicateGroup duplicateGroup;

        for (const FileRecord& file: files)
        {
            duplicateGroup.addFile(file);
        }

        return duplicateGroup;
    }

    /// @brief Verifies the complete top-to-bottom filename sequence displayed by a results table.
    ///
    /// @param resultsTable Results table containing the filename column to inspect.
    /// @param expectedFileNames Expected filename for every row, in display order.
    void verifyExpectedFileNameOrder(const QTableWidget& resultsTable, const QStringList& expectedFileNames)
    {
        ASSERT_EQ(resultsTable.rowCount(), expectedFileNames.size());

        for (int row = 0; row < resultsTable.rowCount(); ++row)
        {
            // On failure, GoogleTest adds "Result row <index>" to identify which row has an unexpected filename.
            SCOPED_TRACE(testing::Message() << "Result row " << row);
            const QTableWidgetItem* fileNameItem = resultsTable.item(row, fileNameColumn);
            ASSERT_NE(fileNameItem, nullptr);
            EXPECT_EQ(fileNameItem->text().toStdString(), expectedFileNames.at(row).toStdString());
        }
    }
}

/// @brief Verifies that duplicate-group data is represented completely and correctly in the results table.
///
/// @par Test setup
/// Create one duplicate group containing two files and another containing three files. Supply the groups
/// in reverse filename order so the test also observes the table's default presentation order.
///
/// @par Procedure
/// Pass both groups to `ResultsTab::showDuplicateGroups()`, locate the generated table rows, and compare
/// every displayed field and associated item data with the corresponding `FileRecord`.
///
/// @par Expected results
/// - The table contains five rows and the three expected columns.
/// - Groups are displayed in ascending reference-filename order, with files remaining in their groups.
/// - Every row contains the expected filename, directory, and byte size divided by 1024 and formatted with two decimals.
/// - Each filename item stores the file's absolute path in `Qt::UserRole`.
/// - Only reference-file rows display their one-based duplicate-group number.
TEST(ResultsTabTest, PopulateTable_WhenDuplicateGroupsAreShown)
{
    ResultsTab resultsTab;

    const FileRecord twoFileGroupReference{QStringLiteral("first.txt"), QStringLiteral("C:/references/first"), 1536};
    const FileRecord twoFileGroupDuplicate{QStringLiteral("first-copy.txt"), QStringLiteral("C:/duplicates/first"), 2048};
    const FileRecord threeFileGroupReference{QStringLiteral("second.txt"), QStringLiteral("C:/references/second"), 3072};
    const FileRecord threeFileGroupDuplicateOne{QStringLiteral("second-copy-1.txt"), QStringLiteral("C:/duplicates/second/one"), 512};
    const FileRecord threeFileGroupDuplicateTwo{QStringLiteral("second-copy-2.txt"), QStringLiteral("C:/duplicates/second/two"), 1024};

    const DuplicateGroup twoFileGroup = createDuplicateGroup({twoFileGroupReference, twoFileGroupDuplicate});
    const DuplicateGroup threeFileGroup = createDuplicateGroup({threeFileGroupReference, threeFileGroupDuplicateOne, threeFileGroupDuplicateTwo});
    resultsTab.showDuplicateGroups(QList<DuplicateGroup>{threeFileGroup, twoFileGroup});

    const auto* resultsTable = resultsTab.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));
    ASSERT_NE(resultsTable, nullptr);

    const QList<FileRecord> expectedFiles{twoFileGroupReference,
                                          twoFileGroupDuplicate,
                                          threeFileGroupReference,
                                          threeFileGroupDuplicateOne,
                                          threeFileGroupDuplicateTwo
    };
    const QStringList expectedGroupNumbers{QStringLiteral("1"),
                                           QString{},
                                           QStringLiteral("2"),
                                           QString{},
                                           QString{}
    };

    ASSERT_EQ(resultsTable->rowCount(), expectedFiles.size());
    ASSERT_EQ(resultsTable->columnCount(), 3);

    for (int row = 0; row < resultsTable->rowCount(); ++row)
    {
        // On failure, GoogleTest adds "Result row <index>" to identify which row contains incorrect file data.
        SCOPED_TRACE(testing::Message() << "Result row " << row);
        const FileRecord& expectedFile = expectedFiles.at(row);
        const QTableWidgetItem* fileNameItem = resultsTable->item(row, fileNameColumn);
        const QTableWidgetItem* directoryItem = resultsTable->item(row, directoriesColumn);
        const QTableWidgetItem* sizeItem = resultsTable->item(row, sizesColumn);
        const QTableWidgetItem* groupNumberItem = resultsTable->verticalHeaderItem(row);

        ASSERT_NE(fileNameItem, nullptr);
        ASSERT_NE(directoryItem, nullptr);
        ASSERT_NE(sizeItem, nullptr);
        ASSERT_NE(groupNumberItem, nullptr);

        EXPECT_EQ(fileNameItem->text().toStdString(), expectedFile.getFileName().toStdString());
        EXPECT_EQ(directoryItem->text().toStdString(), expectedFile.getDirectoryPath().toStdString());
        EXPECT_EQ(sizeItem->text().toStdString(), QString::number(expectedFile.getSizeBytes() / 1024.0, 'f', 2).toStdString());
        EXPECT_EQ(fileNameItem->data(Qt::UserRole).toString().toStdString(), expectedFile.getAbsoluteFilePath().toStdString());
        EXPECT_EQ(groupNumberItem->text().toStdString(), expectedGroupNumbers.at(row).toStdString());
    }
}

/// @brief Verifies that sorting reorders complete duplicate groups rather than individual table rows.
///
/// @par Test setup
/// Create three groups of three files whose reference filenames, directories, and sizes produce different
/// group orders. Give every file a distinct name so movement within or between groups is observable.
///
/// @par Procedure
/// Display the groups, activate one sortable column header, and compare the resulting filename sequence
/// with the expected flattened group order. Redisplay the original input before testing each column.
///
/// @par Expected results
/// - Each column orders groups according to that column's value in the group's reference-file row.
/// - Clicking the initially ascending filename column produces descending filename order.
/// - Clicking the directory or size column after redisplay produces ascending order for that column.
/// - Every duplicate remains adjacent to its reference file and retains its position within the group.
TEST(ResultsTabTest, KeepDuplicateGroupsTogether_WhenResultsAreSorted)
{
    ResultsTab resultsTab;
    const DuplicateGroup fileTwoGroup = createDuplicateGroup({FileRecord{QStringLiteral("file2.txt"), QStringLiteral("C:/second"), 2048},
                                                              FileRecord{QStringLiteral("file2-copy-a.txt"), QStringLiteral("C:/second/a"), 2048},
                                                              FileRecord{QStringLiteral("file2-copy-b.txt"), QStringLiteral("C:/second/b"), 2048}});
    const DuplicateGroup fileTenGroup = createDuplicateGroup({FileRecord{QStringLiteral("file10.txt"), QStringLiteral("C:/first"), 3072},
                                                              FileRecord{QStringLiteral("file10-copy-a.txt"), QStringLiteral("C:/first/a"), 3072},
                                                              FileRecord{QStringLiteral("file10-copy-b.txt"), QStringLiteral("C:/first/b"), 3072}});
    const DuplicateGroup thirdGroup = createDuplicateGroup({FileRecord{QStringLiteral("other.txt"), QStringLiteral("C:/third"), 1024},
                                                            FileRecord{QStringLiteral("other-copy-a.txt"), QStringLiteral("C:/third/a"), 1024},
                                                            FileRecord{QStringLiteral("other-copy-b.txt"), QStringLiteral("C:/third/b"), 1024}});
    const QList<DuplicateGroup> duplicateGroups{fileTenGroup, thirdGroup, fileTwoGroup};

    auto* resultsTable = resultsTab.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));
    ASSERT_NE(resultsTable, nullptr);
    QHeaderView* horizontalHeader = resultsTable->horizontalHeader();
    ASSERT_NE(horizontalHeader, nullptr);

    resultsTab.showDuplicateGroups(duplicateGroups);
    horizontalHeader->sectionClicked(fileNameColumn);
    verifyExpectedFileNameOrder(*resultsTable, {QStringLiteral("other.txt"), QStringLiteral("other-copy-a.txt"), QStringLiteral("other-copy-b.txt"),
                                                QStringLiteral("file10.txt"), QStringLiteral("file10-copy-a.txt"), QStringLiteral("file10-copy-b.txt"),
                                                QStringLiteral("file2.txt"), QStringLiteral("file2-copy-a.txt"), QStringLiteral("file2-copy-b.txt")});

    resultsTab.showDuplicateGroups(duplicateGroups);
    horizontalHeader->sectionClicked(directoriesColumn);
    verifyExpectedFileNameOrder(*resultsTable, {QStringLiteral("file10.txt"), QStringLiteral("file10-copy-a.txt"), QStringLiteral("file10-copy-b.txt"),
                                                QStringLiteral("file2.txt"), QStringLiteral("file2-copy-a.txt"), QStringLiteral("file2-copy-b.txt"),
                                                QStringLiteral("other.txt"), QStringLiteral("other-copy-a.txt"), QStringLiteral("other-copy-b.txt")});

    resultsTab.showDuplicateGroups(duplicateGroups);
    horizontalHeader->sectionClicked(sizesColumn);
    verifyExpectedFileNameOrder(*resultsTable, {QStringLiteral("other.txt"), QStringLiteral("other-copy-a.txt"), QStringLiteral("other-copy-b.txt"),
                                                QStringLiteral("file2.txt"), QStringLiteral("file2-copy-a.txt"), QStringLiteral("file2-copy-b.txt"),
                                                QStringLiteral("file10.txt"), QStringLiteral("file10-copy-a.txt"), QStringLiteral("file10-copy-b.txt")});
}

/// @brief Verifies sort-direction toggling and natural numeric ordering of filenames.
///
/// @par Test setup
/// Create two groups whose reference files are named `file2.txt` and `file10.txt`. Activate the directory
/// column first so the filename column is not the current sort column.
///
/// @par Procedure
/// Activate the filename header once, inspect the row and sort-indicator order, then activate the same
/// header again and repeat the checks.
///
/// @par Expected results
/// - The first filename-header activation selects ascending order and places `file2.txt` before `file10.txt`.
/// - The second activation selects descending order and reverses the two complete groups.
/// - The header indicator reports the filename column and the active direction after each activation.
TEST(ResultsTabTest, ToggleSortOrder_WhenSameHeaderIsClicked)
{
    ResultsTab resultsTab;
    const DuplicateGroup fileTenGroup = createDuplicateGroup({FileRecord{QStringLiteral("file10.txt"), QStringLiteral("C:/second"), 1024},
                                                              FileRecord{QStringLiteral("file10-copy.txt"), QStringLiteral("C:/second/copy"), 1024}});
    const DuplicateGroup fileTwoGroup = createDuplicateGroup({FileRecord{QStringLiteral("file2.txt"), QStringLiteral("C:/first"), 2048},
                                                              FileRecord{QStringLiteral("file2-copy.txt"), QStringLiteral("C:/first/copy"), 2048}});

    resultsTab.showDuplicateGroups(QList<DuplicateGroup>{fileTenGroup, fileTwoGroup});

    auto* resultsTable = resultsTab.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));
    ASSERT_NE(resultsTable, nullptr);
    QHeaderView* horizontalHeader = resultsTable->horizontalHeader();
    ASSERT_NE(horizontalHeader, nullptr);

    horizontalHeader->sectionClicked(directoriesColumn);
    horizontalHeader->sectionClicked(fileNameColumn);
    verifyExpectedFileNameOrder(*resultsTable, {QStringLiteral("file2.txt"), QStringLiteral("file2-copy.txt"),
                                                QStringLiteral("file10.txt"), QStringLiteral("file10-copy.txt")});

    EXPECT_EQ(horizontalHeader->sortIndicatorSection(), fileNameColumn);
    EXPECT_EQ(horizontalHeader->sortIndicatorOrder(), Qt::AscendingOrder);

    horizontalHeader->sectionClicked(fileNameColumn);
    verifyExpectedFileNameOrder(*resultsTable, {QStringLiteral("file10.txt"), QStringLiteral("file10-copy.txt"),
                                                QStringLiteral("file2.txt"), QStringLiteral("file2-copy.txt")});

    EXPECT_EQ(horizontalHeader->sortIndicatorOrder(), Qt::DescendingOrder);
}

/// @brief Verifies that displaying a new result restores the default sorting configuration.
///
/// @par Test setup
/// Display one small-file group and one large-file group, then activate size sorting. Prepare a second set
/// of groups supplied in reverse filename order.
///
/// @par Procedure
/// Confirm that size is the active sort column, replace the displayed data with the second group set, and
/// inspect both its row order and the header's sort indicator.
///
/// @par Expected results
/// - The new groups are ordered by reference filename rather than by the previously selected size column.
/// - `new-first.txt` precedes `new-second.txt`, with each duplicate following its reference file.
/// - The sort indicator returns to the filename column in ascending order.
TEST(ResultsTabTest, ResetSorting_WhenNewResultsAreShown)
{
    ResultsTab resultsTab;
    const DuplicateGroup smallFileGroup = createDuplicateGroup({FileRecord{QStringLiteral("second.txt"), QStringLiteral("C:/small"), 1024},
                                                                FileRecord{QStringLiteral("second-copy.txt"), QStringLiteral("C:/small/copy"), 1024}});
    const DuplicateGroup largeFileGroup = createDuplicateGroup({FileRecord{QStringLiteral("first.txt"), QStringLiteral("C:/large"), 4096},
                                                                FileRecord{QStringLiteral("first-copy.txt"), QStringLiteral("C:/large/copy"), 4096}});
    resultsTab.showDuplicateGroups(QList<DuplicateGroup>{smallFileGroup, largeFileGroup});

    auto* resultsTable = resultsTab.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));
    ASSERT_NE(resultsTable, nullptr);

    QHeaderView* horizontalHeader = resultsTable->horizontalHeader();
    ASSERT_NE(horizontalHeader, nullptr);

    horizontalHeader->sectionClicked(sizesColumn);
    ASSERT_EQ(horizontalHeader->sortIndicatorSection(), sizesColumn);

    const DuplicateGroup secondGroup = createDuplicateGroup({FileRecord{QStringLiteral("new-second.txt"), QStringLiteral("C:/new/second"), 512},
                                                             FileRecord{QStringLiteral("new-second-copy.txt"), QStringLiteral("C:/new/second/copy"), 512}});
    const DuplicateGroup firstGroup = createDuplicateGroup({FileRecord{QStringLiteral("new-first.txt"), QStringLiteral("C:/new/first"), 8192},
                                                            FileRecord{QStringLiteral("new-first-copy.txt"), QStringLiteral("C:/new/first/copy"), 8192}});

    resultsTab.showDuplicateGroups(QList<DuplicateGroup>{secondGroup, firstGroup});

    verifyExpectedFileNameOrder(*resultsTable, {QStringLiteral("new-first.txt"), QStringLiteral("new-first-copy.txt"),
                                                QStringLiteral("new-second.txt"), QStringLiteral("new-second-copy.txt")});

    EXPECT_EQ(horizontalHeader->sortIndicatorSection(), fileNameColumn);
    EXPECT_EQ(horizontalHeader->sortIndicatorOrder(), Qt::AscendingOrder);
}

/// @brief Verifies that new results replace, rather than append to, previously displayed results.
///
/// @par Test setup
/// Create an old group containing three files and a new group containing two files, using distinct filenames
/// and paths so stale data can be identified.
///
/// @par Procedure
/// Display the old group and confirm that it creates three rows. Then display the new group and inspect the
/// table rows and the duplicate groups retained by `ResultsTab`.
///
/// @par Expected results
/// - The table contains only the two rows belonging to the new group.
/// - No filename from the old group remains visible.
/// - `ResultsTab` retains exactly one group, whose reference file is `new.txt`.
TEST(ResultsTabTest, ReplacePreviousRows_WhenNewResultsAreShown)
{
    ResultsTab resultsTab;
    const DuplicateGroup oldGroup = createDuplicateGroup({FileRecord{QStringLiteral("old.txt"), QStringLiteral("C:/old"), 1024},
                                                          FileRecord{QStringLiteral("old-copy-a.txt"), QStringLiteral("C:/old/a"), 1024},
                                                          FileRecord{QStringLiteral("old-copy-b.txt"), QStringLiteral("C:/old/b"), 1024}});
    const DuplicateGroup newGroup = createDuplicateGroup({FileRecord{QStringLiteral("new.txt"), QStringLiteral("C:/new"), 2048},
                                                          FileRecord{QStringLiteral("new-copy.txt"), QStringLiteral("C:/new/copy"), 2048}});

    resultsTab.showDuplicateGroups(QList<DuplicateGroup>{oldGroup});

    auto* resultsTable = resultsTab.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));
    ASSERT_NE(resultsTable, nullptr);
    ASSERT_EQ(resultsTable->rowCount(), 3);

    resultsTab.showDuplicateGroups(QList<DuplicateGroup>{newGroup});

    verifyExpectedFileNameOrder(*resultsTable, {QStringLiteral("new.txt"), QStringLiteral("new-copy.txt")});
    ASSERT_EQ(resultsTab.getDisplayedDuplicateGroups().size(), 1);
    EXPECT_EQ(resultsTab.getDisplayedDuplicateGroups().constFirst().getFiles().constFirst().getFileName().toStdString(), std::string("new.txt"));
}
