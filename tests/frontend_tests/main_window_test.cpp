#include "frontend/main_window.h"

#include <QAction>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>

#include <gtest/gtest.h>

namespace
{
    /// @brief Creates a scan result suitable for testing result-dependent `MainWindow` behavior.
    ///
    /// @return A completed file-name scan result containing one duplicate group whose two files have
    /// the same name and reside in different directories.
    ScanResult createScanResultWithDuplicates()
    {
        DuplicateGroup duplicateGroup;
        duplicateGroup.addFile(FileRecord{QStringLiteral("duplicate.txt"), QStringLiteral("C:/first"), 8});
        duplicateGroup.addFile(FileRecord{QStringLiteral("duplicate.txt"), QStringLiteral("C:/second"), 8});

        return ScanResult{QList<DuplicateGroup>{duplicateGroup},
                          ScanOutcome::CompletedWithDuplicates,
                          FileNameScanSummary{}
        };
    }
}

/// @brief Verifies the result-related state of a newly constructed main window.
///
/// @par Test setup
/// Construct `MainWindow` without displaying any scan result and locate the main tab widget, its Directories
/// and Results pages, and the export action.
///
/// @par Procedure
/// Inspect the active and visible tabs, the export action state, and the duplicate groups exposed by the window.
///
/// @par Expected results
/// - The Results tab is hidden and the Directories tab is active.
/// - Export is disabled because no result is available.
/// - The displayed duplicate-group collection is empty.
TEST(MainWindowTest, CheckResultsUnavailable_WhenApplicationStarts)
{
    MainWindow mainWindow;

    const auto* tabWidget = mainWindow.findChild<QTabWidget*>(QStringLiteral("main_TabWidget"));
    const auto* resultsTab = mainWindow.findChild<QWidget*>(QStringLiteral("resultsTab"));
    const auto* directoriesTab = mainWindow.findChild<QWidget*>(QStringLiteral("directoriesTab"));
    const auto* exportAction = mainWindow.findChild<QAction*>(QStringLiteral("exportToHtml_Action"));

    ASSERT_NE(tabWidget, nullptr);
    ASSERT_NE(resultsTab, nullptr);
    ASSERT_NE(directoriesTab, nullptr);
    ASSERT_NE(exportAction, nullptr);

    EXPECT_FALSE(tabWidget->isTabVisible(tabWidget->indexOf(resultsTab))); // results tab is not visible
    EXPECT_EQ(tabWidget->currentWidget(), directoriesTab); // directories tab is the active tab
    EXPECT_FALSE(exportAction->isEnabled()); // export action is disabled
    EXPECT_TRUE(mainWindow.getDisplayedDuplicateGroups().isEmpty()); // no duplicates group available
}

/// @brief Verifies the complete UI transition from a displayed result to the cleared state.
///
/// @par Test setup
/// Construct `MainWindow` and prepare a scan result containing one duplicate group.
///
/// @par Procedure
/// Display the scan result and verify that result-related controls become available. Then call
/// `MainWindow::clearScanResult()` and inspect the same controls and stored result data again.
///
/// @par Expected results
/// - Displaying the scan result reveals and activates the Results tab, enables export, and stores one group.
/// - Clearing the scan result hides the Results tab, activates Directories, disables export, and removes all displayed groups.
TEST(MainWindowTest, CheckResultsUnavailable_AfterDisplayedResultIsCleared)
{
    MainWindow mainWindow;
    const ScanResult scanResult = createScanResultWithDuplicates();

    auto* tabWidget = mainWindow.findChild<QTabWidget*>(QStringLiteral("main_TabWidget"));
    auto* resultsTab = mainWindow.findChild<QWidget*>(QStringLiteral("resultsTab"));
    auto* directoriesTab = mainWindow.findChild<QWidget*>(QStringLiteral("directoriesTab"));
    auto* exportAction = mainWindow.findChild<QAction*>(QStringLiteral("exportToHtml_Action"));

    ASSERT_NE(tabWidget, nullptr);
    ASSERT_NE(resultsTab, nullptr);
    ASSERT_NE(directoriesTab, nullptr);
    ASSERT_NE(exportAction, nullptr);

    mainWindow.showScanResult(scanResult);

    EXPECT_TRUE(tabWidget->isTabVisible(tabWidget->indexOf(resultsTab))); // results tab is visible
    EXPECT_EQ(tabWidget->currentWidget(), resultsTab); // results tab is the active tab
    EXPECT_TRUE(exportAction->isEnabled()); // export action is enabled
    EXPECT_EQ(mainWindow.getDisplayedDuplicateGroups().size(), 1); // one duplicate group is available

    mainWindow.clearScanResult();

    EXPECT_FALSE(tabWidget->isTabVisible(tabWidget->indexOf(resultsTab))); // results tab is not visible
    EXPECT_EQ(tabWidget->currentWidget(), directoriesTab); // directories tab is the active tab
    EXPECT_FALSE(exportAction->isEnabled()); // export action is disabled
    EXPECT_TRUE(mainWindow.getDisplayedDuplicateGroups().isEmpty()); // no duplicates group available
}

/// @brief Verifies the placement and behavior of the custom Results-tab close button.
///
/// @par Test setup
/// Construct `MainWindow`, prepare a scan result containing duplicates, and locate both tab pages, the custom
/// close button, and the export action.
///
/// @par Procedure
/// Inspect the close button's icon and tab-bar placement, display the result, and click the close button.
///
/// @par Expected results
/// - The close button has an icon and belongs only to the Results tab.
/// - Clicking it clears the result, hides Results, activates Directories, and disables export.
/// - No duplicate groups remain available after the click.
TEST(MainWindowTest, ClearDisplayedResult_WhenResultsTabCloseButtonIsClicked)
{
    MainWindow mainWindow;
    const ScanResult scanResult = createScanResultWithDuplicates();

    auto* tabWidget = mainWindow.findChild<QTabWidget*>(QStringLiteral("main_TabWidget"));
    auto* resultsTab = mainWindow.findChild<QWidget*>(QStringLiteral("resultsTab"));
    auto* directoriesTab = mainWindow.findChild<QWidget*>(QStringLiteral("directoriesTab"));
    auto* closeButton = mainWindow.findChild<QToolButton*>(QStringLiteral("closeResultsTab_ToolButton"));
    auto* exportAction = mainWindow.findChild<QAction*>(QStringLiteral("exportToHtml_Action"));

    ASSERT_NE(tabWidget, nullptr);
    ASSERT_NE(resultsTab, nullptr);
    ASSERT_NE(directoriesTab, nullptr);
    ASSERT_NE(closeButton, nullptr);
    ASSERT_NE(exportAction, nullptr);
    EXPECT_FALSE(closeButton->icon().isNull()); // close button has an icon

    const int directoriesTabIndex = tabWidget->indexOf(directoriesTab);
    const int resultsTabIndex = tabWidget->indexOf(resultsTab);
    EXPECT_EQ(tabWidget->tabBar()->tabButton(directoriesTabIndex, QTabBar::RightSide), nullptr); // directories tab has no close button
    EXPECT_EQ(tabWidget->tabBar()->tabButton(resultsTabIndex, QTabBar::RightSide), closeButton); // results tab owns the close button

    mainWindow.showScanResult(scanResult);
    closeButton->click();

    EXPECT_FALSE(tabWidget->isTabVisible(resultsTabIndex)); // results tab is not visible
    EXPECT_EQ(tabWidget->currentWidget(), directoriesTab); // directories tab is the active tab
    EXPECT_FALSE(exportAction->isEnabled()); // export action is disabled
    EXPECT_TRUE(mainWindow.getDisplayedDuplicateGroups().isEmpty()); // no duplicates group available
}

/// @brief Verifies that an empty scan result invalidates a previously displayed non-empty result.
///
/// @par Test setup
/// Construct `MainWindow`, a scan result containing one duplicate group, and a completed scan result containing
/// no duplicate groups.
///
/// @par Procedure
/// Display the non-empty result and confirm that rows and group data are present. Then pass the empty result to
/// `MainWindow::showScanResult()` and inspect the result-related UI and table contents.
///
/// @par Expected results
/// - The Results tab is hidden and Directories becomes active.
/// - Export is disabled and the displayed duplicate-group collection is empty.
/// - The results table contains no stale rows from the preceding result.
TEST(MainWindowTest, ClearExistingResult_WhenEmptyScanResultIsShown)
{
    MainWindow mainWindow;
    const ScanResult scanResultWithDuplicates = createScanResultWithDuplicates();
    const ScanResult scanResultWithoutDuplicates{QList<DuplicateGroup>{},
                                                 ScanOutcome::CompletedWithoutDuplicates,
                                                 FileNameScanSummary{}
    };

    auto* tabWidget = mainWindow.findChild<QTabWidget*>(QStringLiteral("main_TabWidget"));
    auto* resultsTab = mainWindow.findChild<QWidget*>(QStringLiteral("resultsTab"));
    auto* directoriesTab = mainWindow.findChild<QWidget*>(QStringLiteral("directoriesTab"));
    auto* resultsTable = mainWindow.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));
    auto* exportAction = mainWindow.findChild<QAction*>(QStringLiteral("exportToHtml_Action"));

    ASSERT_NE(tabWidget, nullptr);
    ASSERT_NE(resultsTab, nullptr);
    ASSERT_NE(directoriesTab, nullptr);
    ASSERT_NE(resultsTable, nullptr);
    ASSERT_NE(exportAction, nullptr);

    mainWindow.showScanResult(scanResultWithDuplicates);
    ASSERT_FALSE(mainWindow.getDisplayedDuplicateGroups().isEmpty());
    ASSERT_GT(resultsTable->rowCount(), 0);

    mainWindow.showScanResult(scanResultWithoutDuplicates);

    EXPECT_FALSE(tabWidget->isTabVisible(tabWidget->indexOf(resultsTab))); // results tab is not visible
    EXPECT_EQ(tabWidget->currentWidget(), directoriesTab); // directories tab is the active tab
    EXPECT_FALSE(exportAction->isEnabled()); // export action is disabled
    EXPECT_TRUE(mainWindow.getDisplayedDuplicateGroups().isEmpty()); // no duplicate groups are available
    EXPECT_EQ(resultsTable->rowCount(), 0); // no stale result rows are displayed
}

/// @brief Verifies that a result-row double-click is forwarded as a file-reveal request by `MainWindow`.
///
/// @par Test setup
/// Construct `MainWindow`, prepare a two-file duplicate group, and attach an observer to
/// `MainWindow::revealFileInSystemFileManagerRequested` that records the invocation count and path.
///
/// @par Procedure
/// Display the result and invoke the table's `cellDoubleClicked` signal for the second file's row.
///
/// @par Expected results
/// - `revealFileInSystemFileManagerRequested` is emitted exactly once.
/// - The emitted path is the absolute path of the file represented by the double-clicked row.
TEST(MainWindowTest, RequestFileReveal_WhenResultRowIsDoubleClicked)
{
    MainWindow mainWindow;
    const ScanResult scanResult = createScanResultWithDuplicates();
    auto* resultsTable = mainWindow.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));

    ASSERT_NE(resultsTable, nullptr);

    int revealRequestCount = 0;
    QString requestedAbsoluteFilePath;
    QObject::connect(&mainWindow, &MainWindow::revealFileInSystemFileManagerRequested, &mainWindow,
                     [&](const QString& absoluteFilePath)
                     {
                         ++revealRequestCount;
                         requestedAbsoluteFilePath = absoluteFilePath;
                     });

    mainWindow.showScanResult(scanResult);
    ASSERT_EQ(resultsTable->rowCount(), 2);

    resultsTable->cellDoubleClicked(1, 2);

    const QString expectedAbsoluteFilePath = FileRecord{QStringLiteral("duplicate.txt"),QStringLiteral("C:/second"), 8}.getAbsoluteFilePath();

    EXPECT_EQ(revealRequestCount, 1);
    EXPECT_EQ(requestedAbsoluteFilePath.toStdString(), expectedAbsoluteFilePath.toStdString());
}
