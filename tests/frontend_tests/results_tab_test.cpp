#include "frontend/results_tab.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLocale>
#include <QMenu>
#include <QTableWidget>
#include <QTimer>

#include <gtest/gtest.h>

namespace
{
    /// @brief Zero-based index of the results-table column containing file names.
    constexpr int fileNameColumn = 0;

    /// @brief Zero-based index of the results-table column containing directory paths.
    constexpr int directoriesColumn = 1;

    /// @brief Zero-based index of the results-table column containing file sizes.
    constexpr int sizesColumn = 2;

    /// @brief Temporarily replaces Qt's default locale and restores it when the current scope ends.
    ///
    /// File-size text is intentionally localized by the application. Tests that verify exact strings use this
    /// guard to make decimal separators and unit formatting deterministic on every development and CI machine.
    class ScopedDefaultLocale
    {
    public:
        /// @brief Stores the current default locale and installs the requested locale.
        ///
        /// @param locale Locale to use until this object is destroyed.
        explicit ScopedDefaultLocale(const QLocale& locale) : previousDefaultLocale_(QLocale())
        {
            QLocale::setDefault(locale);
        }

        /// @brief Restores the default locale that was active before construction.
        ~ScopedDefaultLocale()
        {
            QLocale::setDefault(previousDefaultLocale_);
        }

        ScopedDefaultLocale(const ScopedDefaultLocale&) = delete;
        ScopedDefaultLocale& operator=(const ScopedDefaultLocale&) = delete;

    private:
        /// @brief Default locale to restore when this guard leaves scope.
        QLocale previousDefaultLocale_;
    };

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

    /// @brief Opens a result context menu and activates the action with the requested text.
    ///
    /// A fail-safe timer closes the popup if it cannot be found or does not close after activation, preventing
    /// the modal `QMenu::exec()` call from hanging the test process.
    ///
    /// @param resultsTable Table whose custom-context-menu signal is invoked.
    /// @param position Position within the table viewport at which the menu is requested.
    /// @param actionText Text of the context-menu action to activate.
    /// @return `true` if the popup menu and requested action were found and the action was activated.
    bool activateContextMenuAction(QTableWidget& resultsTable, const QPoint& position, const QString& actionText)
    {
        // The zero-delay callback updates this flag while the function is blocked inside the menu's nested
        // event loop. The value lets the caller distinguish successful automation from fail-safe dismissal.
        bool actionWasActivated = false;

        // QMenu::exec() is modal and would block indefinitely if the automation callback failed before closing
        // the menu. Keep a local timer alive for the entire nested event loop as a last-resort escape path.
        QTimer failSafeCloseTimer;

        // Only one dismissal attempt is required. Repeating the timeout could affect a later, unrelated popup.
        failSafeCloseTimer.setSingleShot(true);

        // Close the active popup if lookup or activation fails, preventing QMenu::exec() from hanging the test.
        QObject::connect(&failSafeCloseTimer, &QTimer::timeout, &resultsTable,
                         []
                         {
                             // The production code creates QMenu as a local object, so the test cannot retain a
                             // direct pointer before the menu opens. Query Qt for the popup that is active now.
                             if (QWidget* activePopup = QApplication::activePopupWidget())
                             {
                                 // Closing the popup makes QMenu::exec() return even when no action was selected.
                                 activePopup->close();
                             }
                         });

        // Start the fail-safe before requesting the menu so every failure path remains time-bounded.
        failSafeCloseTimer.start(250);

        // Queue action selection before requesting the menu. A zero-delay callback cannot run until control
        // reaches QMenu::exec() and its nested event loop starts processing events, at which point the popup exists.
        QTimer::singleShot(0, &resultsTable,
                           [&actionWasActivated, actionText]
                           {
                               // The context menu is owned by the production stack frame and is discoverable only
                               // while QMenu::exec() is active. qobject_cast also verifies that the popup is a menu.
                               auto* contextMenu = qobject_cast<QMenu*>(QApplication::activePopupWidget());

                               if (!contextMenu)
                               {
                                   // Leave the flag false. The fail-safe timer will release the modal event loop if
                                   // no suitable popup appeared.
                                   return;
                               }

                               // Match the same user-visible action label that appears in the context menu.
                               for (QAction* action: contextMenu->actions())
                               {
                                   if (action->text() == actionText)
                                   {
                                       // Record successful lookup before sending input because the Enter event can
                                       // synchronously dismiss the menu and allow the production slot to continue.
                                       actionWasActivated = true;

                                       // Enter activates whichever QAction QMenu currently considers selected.
                                       contextMenu->setActiveAction(action);

                                       // Route activation through QMenu rather than calling QAction::trigger(). A
                                       // direct trigger emits QAction::triggered but does not make exec() return that
                                       // QAction, so the production code's selected-action branch would not execute.
                                       QKeyEvent enterKeyPress(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                                       QApplication::sendEvent(contextMenu, &enterKeyPress);

                                       // Complete the synthetic key interaction with the corresponding release event.
                                       QKeyEvent enterKeyRelease(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
                                       QApplication::sendEvent(contextMenu, &enterKeyRelease);
                                       return;
                                   }
                               }

                               // The menu opened but did not contain the requested action. Close it immediately
                               // instead of waiting for the fail-safe timeout; actionWasActivated remains false.
                               contextMenu->close();
                           });

        // Invoke the same signal emitted for a user context-menu request. Its connected production slot opens the
        // modal QMenu and blocks here until the queued callback selects an action or the fail-safe closes the popup.
        resultsTable.customContextMenuRequested(position);

        // Normal menu completion no longer needs the fallback. Stopping it also prevents a pending timeout from
        // closing a popup created later in the same test process.
        failSafeCloseTimer.stop();

        // Report whether the requested action was actually located and activated, not merely whether the menu closed.
        return actionWasActivated;
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
/// - The size column is labelled `Size`.
/// - Groups are displayed in ascending reference-filename order, with files remaining in their groups.
/// - Every row contains the expected filename, directory, and adaptively formatted file size.
/// - Each filename item stores the file's absolute path in `Qt::UserRole`.
/// - Only reference-file rows display their one-based duplicate-group number.
TEST(ResultsTabTest, PopulateTable_WhenDuplicateGroupsAreShown)
{
    const ScopedDefaultLocale defaultLocale{QLocale::c()};
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
                                          threeFileGroupDuplicateTwo};

    const QStringList expectedGroupNumbers{QStringLiteral("1"),
                                           QString{},
                                           QStringLiteral("2"),
                                           QString{},
                                           QString{}};

    const QStringList expectedDisplaySizes{QStringLiteral("1.50 kB"),
                                           QStringLiteral("2.00 kB"),
                                           QStringLiteral("3.00 kB"),
                                           QStringLiteral("512 B"),
                                           QStringLiteral("1.00 kB")};

    ASSERT_EQ(resultsTable->rowCount(), expectedFiles.size());
    ASSERT_EQ(resultsTable->columnCount(), 3);
    ASSERT_NE(resultsTable->horizontalHeaderItem(sizesColumn), nullptr);
    EXPECT_EQ(resultsTable->horizontalHeaderItem(sizesColumn)->text().toStdString(), std::string("Size"));

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
        EXPECT_EQ(sizeItem->text().toStdString(), expectedDisplaySizes.at(row).toStdString());
        EXPECT_EQ(fileNameItem->data(Qt::UserRole).toString().toStdString(), expectedFile.getAbsoluteFilePath().toStdString());
        EXPECT_EQ(groupNumberItem->text().toStdString(), expectedGroupNumbers.at(row).toStdString());
    }
}

/// @brief Verifies that displayed file sizes select an appropriate unit for their magnitude.
///
/// @par Test setup
/// Fix Qt's default locale to the C locale and create one duplicate group containing file sizes represented
/// most naturally as bytes, kilobytes, megabytes, gigabytes, and terabytes.
///
/// @par Procedure
/// Display the duplicate group, read the size item from every generated row, and compare its text with the
/// complete expected value and unit.
///
/// @par Expected results
/// - A sub-kilobyte value is displayed as an integer followed by `B`.
/// - Larger values are scaled to kB, MB, GB, or TB as appropriate.
/// - Scaled values retain two decimal places.
TEST(ResultsTabTest, DisplayAdaptiveFileSizeUnits_WhenFileSizesSpanMultipleUnits)
{
    const ScopedDefaultLocale defaultLocale{QLocale::c()};
    ResultsTab resultsTab;

    constexpr qint64 kibibyte = 1024;
    constexpr qint64 mebibyte = kibibyte * 1024;
    constexpr qint64 gibibyte = mebibyte * 1024;
    constexpr qint64 tebibyte = gibibyte * 1024;

    const DuplicateGroup duplicateGroup = createDuplicateGroup({FileRecord{QStringLiteral("bytes.bin"), QStringLiteral("C:/sizes"), 512},
                                                                FileRecord{QStringLiteral("kilobytes.bin"), QStringLiteral("C:/sizes"), kibibyte + kibibyte / 2},
                                                                FileRecord{QStringLiteral("megabytes.bin"), QStringLiteral("C:/sizes"), 2 * mebibyte},
                                                                FileRecord{QStringLiteral("gigabytes.bin"), QStringLiteral("C:/sizes"), 3 * gibibyte},
                                                                FileRecord{QStringLiteral("terabytes.bin"), QStringLiteral("C:/sizes"), 4 * tebibyte}});

    resultsTab.showDuplicateGroups({duplicateGroup});

    const auto* resultsTable = resultsTab.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));
    ASSERT_NE(resultsTable, nullptr);

    const QStringList expectedDisplaySizes{QStringLiteral("512 B"),
                                           QStringLiteral("1.50 kB"),
                                           QStringLiteral("2.00 MB"),
                                           QStringLiteral("3.00 GB"),
                                           QStringLiteral("4.00 TB")};
    ASSERT_EQ(resultsTable->rowCount(), expectedDisplaySizes.size());

    for (int row = 0; row < resultsTable->rowCount(); ++row)
    {
        // On failure, GoogleTest adds "Result row <index>" to identify the incorrectly formatted size.
        SCOPED_TRACE(testing::Message() << "Result row " << row);
        const QTableWidgetItem* sizeItem = resultsTable->item(row, sizesColumn);

        ASSERT_NE(sizeItem, nullptr);
        EXPECT_EQ(sizeItem->text().toStdString(), expectedDisplaySizes.at(row).toStdString());
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

/// @brief Verifies that the context-menu Copy path action copies the selected file's native path.
///
/// @par Test setup
/// Display a duplicate group in a visible `ResultsTab`, locate the second file's table row, and preserve the
/// clipboard's original text so the test can restore it afterward.
///
/// @par Procedure
/// Request the context menu over the second row and activate its Copy path action.
///
/// @par Expected results
/// - The Copy path action is found and activated.
/// - The clipboard contains the selected file's absolute path using native directory separators.
TEST(ResultsTabContextMenuTest, CopyNativePath_WhenCopyContextActionIsSelected)
{
    ResultsTab resultsTab;
    const FileRecord referenceFile{QStringLiteral("reference.txt"), QStringLiteral("C:/first"), 1024};
    const FileRecord duplicateFile{QStringLiteral("duplicate.txt"), QStringLiteral("C:/second"), 1024};
    resultsTab.showDuplicateGroups(QList<DuplicateGroup>{createDuplicateGroup({referenceFile, duplicateFile})});
    resultsTab.resize(600, 300);
    resultsTab.show();

    QApplication::processEvents();

    auto* resultsTable = resultsTab.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));
    ASSERT_NE(resultsTable, nullptr);
    const QTableWidgetItem* duplicateFileNameItem = resultsTable->item(1, fileNameColumn);
    ASSERT_NE(duplicateFileNameItem, nullptr);

    const QPoint duplicateRowPosition = resultsTable->visualItemRect(duplicateFileNameItem).center();
    ASSERT_TRUE(resultsTable->indexAt(duplicateRowPosition).isValid());

    QClipboard* clipboard = QApplication::clipboard();
    ASSERT_NE(clipboard, nullptr);
    const QString originalClipboardText = clipboard->text();
    clipboard->setText(QStringLiteral("unchanged"));

    const bool actionWasActivated = activateContextMenuAction(*resultsTable, duplicateRowPosition,QStringLiteral("Copy path"));

    EXPECT_TRUE(actionWasActivated);
    EXPECT_EQ(clipboard->text().toStdString(), QDir::toNativeSeparators(duplicateFile.getAbsoluteFilePath()).toStdString());

    clipboard->setText(originalClipboardText);
}

/// @brief Verifies that the context-menu Reveal action emits a request for the selected file.
///
/// @par Test setup
/// Display a duplicate group in a visible `ResultsTab`, locate its reference-file row, and attach an observer
/// to `ResultsTab::revealFileInSystemFileManagerRequested`.
///
/// @par Procedure
/// Request the context menu over the reference row and activate its Reveal in file manager action.
///
/// @par Expected results
/// - The Reveal action is found and activated.
/// - `revealFileInSystemFileManagerRequested` is emitted exactly once with the reference file's absolute path.
TEST(ResultsTabContextMenuTest, RequestFileReveal_WhenRevealContextActionIsSelected)
{
    ResultsTab resultsTab;
    const FileRecord referenceFile{QStringLiteral("reference.txt"), QStringLiteral("C:/first"), 1024};
    const FileRecord duplicateFile{QStringLiteral("duplicate.txt"), QStringLiteral("C:/second"), 1024};
    resultsTab.showDuplicateGroups(QList<DuplicateGroup>{createDuplicateGroup({referenceFile, duplicateFile})});
    resultsTab.resize(600, 300);
    resultsTab.show();

    QApplication::processEvents();

    auto* resultsTable = resultsTab.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));
    ASSERT_NE(resultsTable, nullptr);
    const QTableWidgetItem* referenceFileNameItem = resultsTable->item(0, fileNameColumn);
    ASSERT_NE(referenceFileNameItem, nullptr);

    const QPoint referenceRowPosition = resultsTable->visualItemRect(referenceFileNameItem).center();
    ASSERT_TRUE(resultsTable->indexAt(referenceRowPosition).isValid());

    int revealRequestCount = 0;
    QString requestedAbsoluteFilePath;
    QObject::connect(&resultsTab, &ResultsTab::revealFileInSystemFileManagerRequested, &resultsTab,
                     [&](const QString& absoluteFilePath)
                     {
                         ++revealRequestCount;
                         requestedAbsoluteFilePath = absoluteFilePath;
                     });

    const bool actionWasActivated = activateContextMenuAction(*resultsTable,
                                                              referenceRowPosition,
                                                              QStringLiteral("Reveal in file manager"));

    EXPECT_TRUE(actionWasActivated);
    EXPECT_EQ(revealRequestCount, 1);
    EXPECT_EQ(requestedAbsoluteFilePath.toStdString(), referenceFile.getAbsoluteFilePath().toStdString());
}

/// @brief Verifies that requesting a context menu outside a result row has no effect.
///
/// @par Test setup
/// Display a duplicate group in a visible `ResultsTab`, select its first cell, preserve the clipboard text, and
/// attach an observer to the file-reveal request signal. Choose an empty point below all populated rows.
///
/// @par Procedure
/// Invoke the table's custom-context-menu signal at the empty viewport position.
///
/// @par Expected results
/// - No context menu is opened and no file-reveal request is emitted.
/// - The clipboard and current table selection remain unchanged.
TEST(ResultsTabContextMenuTest, DoNothing_WhenContextMenuIsRequestedOutsideAResultRow)
{
    ResultsTab resultsTab;
    const FileRecord referenceFile{QStringLiteral("reference.txt"), QStringLiteral("C:/first"), 1024};
    const FileRecord duplicateFile{QStringLiteral("duplicate.txt"), QStringLiteral("C:/second"), 1024};
    resultsTab.showDuplicateGroups(QList<DuplicateGroup>{createDuplicateGroup({referenceFile, duplicateFile})});
    resultsTab.resize(600, 300);
    resultsTab.show();

    QApplication::processEvents();

    auto* resultsTable = resultsTab.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));
    ASSERT_NE(resultsTable, nullptr);
    resultsTable->setCurrentCell(0, fileNameColumn);
    const QModelIndex originalCurrentIndex = resultsTable->currentIndex();
    ASSERT_TRUE(originalCurrentIndex.isValid());

    const QPoint emptyPosition(resultsTable->viewport()->width() / 2,
                               resultsTable->viewport()->height() - 1);
    ASSERT_FALSE(resultsTable->indexAt(emptyPosition).isValid());

    int revealRequestCount = 0;
    QObject::connect(&resultsTab, &ResultsTab::revealFileInSystemFileManagerRequested, &resultsTab,
                     [&revealRequestCount](const QString&) { ++revealRequestCount; });

    QClipboard* clipboard = QApplication::clipboard();
    ASSERT_NE(clipboard, nullptr);
    const QString originalClipboardText = clipboard->text();
    const QString sentinelClipboardText = QStringLiteral("clipboard must remain unchanged");
    clipboard->setText(sentinelClipboardText);

    resultsTable->customContextMenuRequested(emptyPosition);

    EXPECT_EQ(QApplication::activePopupWidget(), nullptr);
    EXPECT_EQ(revealRequestCount, 0);
    EXPECT_EQ(clipboard->text().toStdString(), sentinelClipboardText.toStdString());
    EXPECT_TRUE(resultsTable->currentIndex() == originalCurrentIndex);

    clipboard->setText(originalClipboardText);
}
