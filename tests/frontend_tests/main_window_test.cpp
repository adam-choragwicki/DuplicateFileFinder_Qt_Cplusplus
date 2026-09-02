#include "frontend/main_window.h"
#include "test_helpers/scan_result_test_helpers.h"

#include <QAction>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolButton>

#include <gtest/gtest.h>

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
    const ScanResult scanResult = test_helpers::createScanResultWithDuplicates();

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

/// @brief Verifies the placement of the custom Results-tab close button and forwarding of its user intent.
///
/// @par Test setup
/// Construct `MainWindow`, prepare a scan result containing duplicates, and locate both tab pages, the custom
/// close button, and the export action. Observe `MainWindow::scanResultCloseRequested`.
///
/// @par Procedure
/// Inspect the close button's icon and tab-bar placement, display the result, and click the close button while
/// recording the requests emitted by the window.
///
/// @par Expected results
/// - The close button has an icon and belongs only to the Results tab.
/// - The prepared result is visible and export is enabled before the interaction.
/// - Clicking the button emits exactly one request to clear the application result.
TEST(MainWindowTest, RequestResultClearing_WhenResultsTabCloseButtonIsClicked)
{
    MainWindow mainWindow;
    const ScanResult scanResult = test_helpers::createScanResultWithDuplicates();

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
    int closeRequestCount = 0;
    QObject::connect(&mainWindow, &MainWindow::scanResultTabCloseRequested, &mainWindow,
                     [&closeRequestCount] { ++closeRequestCount; });
    EXPECT_FALSE(closeButton->icon().isNull()); // close button has an icon

    const int directoriesTabIndex = tabWidget->indexOf(directoriesTab);
    const int resultsTabIndex = tabWidget->indexOf(resultsTab);
    EXPECT_EQ(tabWidget->tabBar()->tabButton(directoriesTabIndex, QTabBar::RightSide), nullptr); // directories tab has no close button
    EXPECT_EQ(tabWidget->tabBar()->tabButton(resultsTabIndex, QTabBar::RightSide), closeButton); // results tab owns the close button

    mainWindow.showScanResult(scanResult);
    ASSERT_TRUE(tabWidget->isTabVisible(resultsTabIndex)); // results tab is visible before requesting its closure
    ASSERT_EQ(tabWidget->currentWidget(), resultsTab); // results tab is active before requesting its closure
    ASSERT_TRUE(exportAction->isEnabled()); // export is enabled while a result is displayed

    closeButton->click();

    EXPECT_EQ(closeRequestCount, 1); // one close request is forwarded for one button activation
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
    const ScanResult scanResultWithDuplicates = test_helpers::createScanResultWithDuplicates();
    const ScanResult scanResultWithoutDuplicates{QList<DuplicateGroup>{},
                                                 ScanOutcome::CompletedWithoutDuplicates,
                                                 FileNameScanSummary{}
    };

    auto* tabWidget = mainWindow.findChild<QTabWidget*>(QStringLiteral("main_TabWidget"));
    auto* resultsTab = mainWindow.findChild<QWidget*>(QStringLiteral("resultsTab"));
    auto* directoriesTab = mainWindow.findChild<QWidget*>(QStringLiteral("directoriesTab"));
    auto* resultsTable = mainWindow.findChild<QTableView*>(QStringLiteral("results_TableView"));
    auto* exportAction = mainWindow.findChild<QAction*>(QStringLiteral("exportToHtml_Action"));

    ASSERT_NE(tabWidget, nullptr);
    ASSERT_NE(resultsTab, nullptr);
    ASSERT_NE(directoriesTab, nullptr);
    ASSERT_NE(resultsTable, nullptr);
    ASSERT_NE(exportAction, nullptr);

    mainWindow.showScanResult(scanResultWithDuplicates);
    ASSERT_FALSE(mainWindow.getDisplayedDuplicateGroups().isEmpty());
    ASSERT_NE(resultsTable->model(), nullptr);
    ASSERT_GT(resultsTable->model()->rowCount(), 0);

    mainWindow.showScanResult(scanResultWithoutDuplicates);

    EXPECT_FALSE(tabWidget->isTabVisible(tabWidget->indexOf(resultsTab))); // results tab is not visible
    EXPECT_EQ(tabWidget->currentWidget(), directoriesTab); // directories tab is the active tab
    EXPECT_FALSE(exportAction->isEnabled()); // export action is disabled
    EXPECT_TRUE(mainWindow.getDisplayedDuplicateGroups().isEmpty()); // no duplicate groups are available
    EXPECT_EQ(resultsTable->model()->rowCount(), 0); // no stale result rows are displayed
}

/// @brief Verifies that a result-row double-click is forwarded as a file-reveal request by `MainWindow`.
///
/// @par Test setup
/// Construct `MainWindow`, prepare a two-file duplicate group, and attach an observer to
/// `MainWindow::revealFileInSystemFileManagerRequested` that records the invocation count and path.
///
/// @par Procedure
/// Display the result and invoke the table view's `doubleClicked` signal for the second file's row.
///
/// @par Expected results
/// - `revealFileInSystemFileManagerRequested` is emitted exactly once.
/// - The emitted path is the absolute path of the file represented by the double-clicked row.
TEST(MainWindowTest, RequestFileReveal_WhenResultRowIsDoubleClicked)
{
    MainWindow mainWindow;
    const ScanResult scanResult = test_helpers::createScanResultWithDuplicates();
    auto* resultsTable = mainWindow.findChild<QTableView*>(QStringLiteral("results_TableView"));

    ASSERT_NE(resultsTable, nullptr);
    ASSERT_NE(resultsTable->model(), nullptr);

    int revealRequestCount = 0;
    QString requestedAbsoluteFilePath;
    QObject::connect(&mainWindow, &MainWindow::revealFileInSystemFileManagerRequested, &mainWindow,
                     [&](const QString& absoluteFilePath)
                     {
                         ++revealRequestCount;
                         requestedAbsoluteFilePath = absoluteFilePath;
                     });

    mainWindow.showScanResult(scanResult);
    ASSERT_EQ(resultsTable->model()->rowCount(), 2);

    resultsTable->doubleClicked(resultsTable->model()->index(1, 2));

    const QString expectedAbsoluteFilePath = FileRecord{QStringLiteral("duplicate.txt"),QStringLiteral("C:/second"), 8}.getAbsoluteFilePath();

    EXPECT_EQ(revealRequestCount, 1);
    EXPECT_EQ(requestedAbsoluteFilePath.toStdString(), expectedAbsoluteFilePath.toStdString());
}

/// @brief Verifies that user-facing controls emit the request signals consumed by the controller.
///
/// @par Test setup
/// Construct `MainWindow`, add a temporary scan root so directory removal is available, locate the Add directory,
/// Remove directory, Start scan, and Export controls, and attach a counter to each corresponding request signal.
///
/// @par Procedure
/// Activate the three directory/scan buttons one at a time, display a result to enable Export, and then activate
/// the export action.
///
/// @par Expected results
/// - Each control emits its corresponding request signal exactly once.
/// - Activating one control does not emit any request signal belonging to another control.
TEST(MainWindowTest, EmitExpectedRequestSignals_WhenControlsAreActivated)
{
    MainWindow mainWindow;
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    mainWindow.addScanDirectory(temporaryDirectory.path());

    auto* addDirectoryButton = mainWindow.findChild<QPushButton*>(QStringLiteral("addDirectory_PushButton"));
    auto* removeDirectoryButton = mainWindow.findChild<QPushButton*>(QStringLiteral("removeDirectory_PushButton"));
    auto* startScanButton = mainWindow.findChild<QPushButton*>(QStringLiteral("startScan_PushButton"));
    auto* exportAction = mainWindow.findChild<QAction*>(QStringLiteral("exportToHtml_Action"));

    ASSERT_NE(addDirectoryButton, nullptr);
    ASSERT_NE(removeDirectoryButton, nullptr);
    ASSERT_NE(startScanButton, nullptr);
    ASSERT_NE(exportAction, nullptr);

    int addDirectoryRequestCount = 0;
    int removeDirectoryRequestCount = 0;
    int startScanRequestCount = 0;
    int exportRequestCount = 0;

    QObject::connect(&mainWindow, &MainWindow::addDirectoryButtonClicked, &mainWindow,
                     [&addDirectoryRequestCount] { ++addDirectoryRequestCount; });
    QObject::connect(&mainWindow, &MainWindow::removeDirectoryButtonClicked, &mainWindow,
                     [&removeDirectoryRequestCount] { ++removeDirectoryRequestCount; });
    QObject::connect(&mainWindow, &MainWindow::startScanButtonClicked, &mainWindow,
                     [&startScanRequestCount] { ++startScanRequestCount; });
    QObject::connect(&mainWindow, &MainWindow::exportToHtmlRequested, &mainWindow,
                     [&exportRequestCount] { ++exportRequestCount; });

    ASSERT_TRUE(removeDirectoryButton->isEnabled());

    addDirectoryButton->click();
    EXPECT_EQ(addDirectoryRequestCount, 1);
    EXPECT_EQ(removeDirectoryRequestCount, 0);
    EXPECT_EQ(startScanRequestCount, 0);
    EXPECT_EQ(exportRequestCount, 0);

    removeDirectoryButton->click();
    EXPECT_EQ(addDirectoryRequestCount, 1);
    EXPECT_EQ(removeDirectoryRequestCount, 1);
    EXPECT_EQ(startScanRequestCount, 0);
    EXPECT_EQ(exportRequestCount, 0);

    startScanButton->click();
    EXPECT_EQ(addDirectoryRequestCount, 1);
    EXPECT_EQ(removeDirectoryRequestCount, 1);
    EXPECT_EQ(startScanRequestCount, 1);
    EXPECT_EQ(exportRequestCount, 0);

    mainWindow.showScanResult(test_helpers::createScanResultWithDuplicates());
    ASSERT_TRUE(exportAction->isEnabled());
    exportAction->trigger();

    EXPECT_EQ(addDirectoryRequestCount, 1);
    EXPECT_EQ(removeDirectoryRequestCount, 1);
    EXPECT_EQ(startScanRequestCount, 1);
    EXPECT_EQ(exportRequestCount, 1);
}
