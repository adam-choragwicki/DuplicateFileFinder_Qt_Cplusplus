#include "controller.h"
#include "model.h"
#include "test_helpers/scan_result_test_helpers.h"
#include "temporary_scan_directory_test_fixture.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSet>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>

#include <gtest/gtest.h>

#include <functional>

namespace
{
    /// @brief Provides an isolated directory tree for application scan integration tests.
    class ApplicationScanIntegrationTest : public TemporaryScanDirectoryTest {};

    /// @brief Stores the user-visible properties captured from a modal message box.
    struct ObservedMessageBox
    {
        bool wasShown{};
        bool waitTimedOut{};
        QMessageBox::Icon icon{QMessageBox::NoIcon};
        QString windowTitle;
        QString text;
    };

    /// @brief Activates an application action and captures the next modal message box it opens.
    ///
    /// The helper supports both synchronous dialogs opened directly by @p action and dialogs opened later
    /// after asynchronous scan completion. It closes the captured dialog so its nested event loop cannot block
    /// the test executable.
    ///
    /// @param action User or application action expected to eventually open a message box.
    /// @param timeoutMilliseconds Maximum time allowed for the message box to appear.
    /// @return Captured message-box properties and whether the wait timed out.
    [[nodiscard]] ObservedMessageBox activateAndObserveMessageBox(const std::function<void()>& action,
                                                                  const int timeoutMilliseconds)
    {
        ObservedMessageBox observation;
        QEventLoop eventLoop;
        QTimer observationTimer;
        QTimer timeoutTimer;

        observationTimer.setInterval(10);
        timeoutTimer.setSingleShot(true);

        QObject::connect(&observationTimer, &QTimer::timeout, &eventLoop,
                         [&]
                         {
                             auto* messageBox = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                             if (!messageBox)
                             {
                                 return;
                             }

                             observation.wasShown = true;
                             observation.icon = messageBox->icon();
                             observation.windowTitle = messageBox->windowTitle();
                             observation.text = messageBox->text();

                             observationTimer.stop();
                             timeoutTimer.stop();
                             messageBox->accept();
                             eventLoop.quit();
                         });

        QObject::connect(&timeoutTimer, &QTimer::timeout, &eventLoop,
                         [&]
                         {
                             observation.waitTimedOut = true;

                             if (QWidget* activeModalWidget = QApplication::activeModalWidget())
                             {
                                 activeModalWidget->close();
                             }

                             eventLoop.quit();
                         });

        observationTimer.start();
        timeoutTimer.start(timeoutMilliseconds);
        action();

        // A directly opened message box runs its own event loop, allowing the observation timer to capture and
        // close it before action() returns. Only asynchronous actions require this additional wait loop.
        if (!observation.wasShown && !observation.waitTimedOut)
        {
            eventLoop.exec();
        }

        observationTimer.stop();
        timeoutTimer.stop();
        return observation;
    }

    /// @brief Processes Qt events until the main window displays duplicate groups or the timeout expires.
    ///
    /// @param mainWindow Window whose displayed result state is observed.
    /// @param timeoutMilliseconds Maximum time allowed for the asynchronous scan.
    /// @return `true` when duplicate groups became available before the timeout.
    [[nodiscard]] bool waitForDisplayedDuplicateGroups(const MainWindow& mainWindow, const int timeoutMilliseconds)
    {
        if (!mainWindow.getDisplayedDuplicateGroups().isEmpty())
        {
            return true;
        }

        bool duplicateGroupsWereDisplayed = false;
        QEventLoop eventLoop;
        QTimer resultPollTimer;
        QTimer timeoutTimer;

        resultPollTimer.setInterval(10);
        timeoutTimer.setSingleShot(true);

        QObject::connect(&resultPollTimer, &QTimer::timeout, &eventLoop,
                         [&]
                         {
                             if (!mainWindow.getDisplayedDuplicateGroups().isEmpty())
                             {
                                 duplicateGroupsWereDisplayed = true;
                                 timeoutTimer.stop();
                                 eventLoop.quit();
                             }
                         });
        QObject::connect(&timeoutTimer, &QTimer::timeout, &eventLoop,
                         [&]
                         {
                             // A regression may open an unexpected modal outcome dialog. Close it so the
                             // failed integration test reports normally instead of hanging in its nested loop.
                             if (QWidget* activeModalWidget = QApplication::activeModalWidget())
                             {
                                 activeModalWidget->close();
                             }

                             eventLoop.quit();
                         });

        resultPollTimer.start();
        timeoutTimer.start(timeoutMilliseconds);
        eventLoop.exec();

        return duplicateGroupsWereDisplayed;
    }
}

/// @brief Verifies that the controller rejects a scan request when the model contains no scan directories.
///
/// @par Test setup
/// Construct `Model`, `MainWindow`, and `Controller` without adding a scan directory. Locate the Start scan
/// button and confirm that both authoritative model state and its rendered view contain no roots.
///
/// @par Procedure
/// Click Start scan, capture and close the modal message box opened by the controller, and inspect whether a
/// scan progress dialog or result was created.
///
/// @par Expected results
/// - An informational "Nothing to scan" message explains that no directories are selected.
/// - No progress dialog is created because the controller does not start the scanner.
/// - No duplicate result becomes available.
TEST(ControllerIntegrationTest, RejectScanAndExplainReason_WhenNoDirectoriesAreSelected)
{
    Model model;
    MainWindow mainWindow;
    Controller controller(model, mainWindow);

    auto* startScanButton = mainWindow.findChild<QPushButton*>(QStringLiteral("startScan_PushButton"));

    ASSERT_NE(startScanButton, nullptr);
    ASSERT_TRUE(model.getScanDirectoryPaths().isEmpty());
    ASSERT_TRUE(mainWindow.getScanDirectoryPaths().isEmpty());

    const ObservedMessageBox messageBox = activateAndObserveMessageBox(
        [startScanButton] { startScanButton->click(); },
        1000);

    ASSERT_FALSE(messageBox.waitTimedOut);
    ASSERT_TRUE(messageBox.wasShown);
    EXPECT_EQ(messageBox.icon, QMessageBox::Information);
    EXPECT_EQ(messageBox.windowTitle.toStdString(), std::string("Nothing to scan"));
    EXPECT_EQ(messageBox.text.toStdString(), std::string("Nothing to scan. No directories selected."));
    EXPECT_EQ(mainWindow.findChild<QProgressDialog*>(), nullptr);
    EXPECT_TRUE(mainWindow.getDisplayedDuplicateGroups().isEmpty());
    EXPECT_EQ(QApplication::activeModalWidget(), nullptr);
}

/// @brief Verifies the complete UI-to-scanner path for a successful content scan that finds duplicates.
///
/// @par Test setup
/// Create two differently named files with identical contents below a temporary root. Construct `Model`,
/// `MainWindow`, and `Controller`, add the root to the model, select content scanning in the window, and locate
/// the Start control and result-related widgets.
///
/// @par Procedure
/// Click Start scan and process Qt events until duplicate groups are displayed or the timeout expires. Then
/// inspect the window state, result table, and file records produced by the asynchronous scan.
///
/// @par Expected results
/// - The controller receives the Start request and runs the selected content workflow to completion.
/// - One duplicate group containing both source files is displayed as two result rows.
/// - The Results tab becomes visible and active, and HTML export becomes enabled.
/// - No modal outcome dialog remains open after a successful scan with duplicates.
TEST_F(ApplicationScanIntegrationTest, DisplayDuplicateResults_WhenContentScanIsStartedFromMainWindow)
{
    const QByteArray duplicateContents = QByteArrayLiteral("identical file contents");
    ASSERT_TRUE(writeFile(QStringLiteral("first/original.bin"), duplicateContents));
    ASSERT_TRUE(writeFile(QStringLiteral("second/copy.bin"), duplicateContents));

    Model model;
    MainWindow mainWindow;
    Controller controller(model, mainWindow);

    ASSERT_EQ(model.addScanDirectory(scanRootPath()).outcome, Model::AddScanDirectoryOutcome::Added);

    auto* scanTypeComboBox = mainWindow.findChild<QComboBox*>(QStringLiteral("scanType_ComboBox"));
    auto* startScanButton = mainWindow.findChild<QPushButton*>(QStringLiteral("startScan_PushButton"));
    auto* tabWidget = mainWindow.findChild<QTabWidget*>(QStringLiteral("main_TabWidget"));
    auto* resultsTab = mainWindow.findChild<QWidget*>(QStringLiteral("resultsTab"));
    auto* exportAction = mainWindow.findChild<QAction*>(QStringLiteral("exportToHtml_Action"));
    auto* resultsTable = mainWindow.findChild<QTableWidget*>(QStringLiteral("results_TableWidget"));

    ASSERT_NE(scanTypeComboBox, nullptr);
    ASSERT_NE(startScanButton, nullptr);
    ASSERT_NE(tabWidget, nullptr);
    ASSERT_NE(resultsTab, nullptr);
    ASSERT_NE(exportAction, nullptr);
    ASSERT_NE(resultsTable, nullptr);

    const int contentScanIndex = scanTypeComboBox->findData(QVariant::fromValue(ScanType::ByFileContent));
    ASSERT_GE(contentScanIndex, 0);
    scanTypeComboBox->setCurrentIndex(contentScanIndex);
    ASSERT_EQ(model.getScanType(), ScanType::ByFileContent);

    mainWindow.show();
    QApplication::processEvents();
    startScanButton->click();

    ASSERT_TRUE(waitForDisplayedDuplicateGroups(mainWindow, 5000));

    const QList<DuplicateGroup>& displayedGroups = mainWindow.getDisplayedDuplicateGroups();
    ASSERT_EQ(displayedGroups.size(), 1);
    ASSERT_EQ(displayedGroups.constFirst().getFiles().size(), 2);
    EXPECT_EQ(resultsTable->rowCount(), 2);

    QSet<QString> displayedFilePaths;
    for (const FileRecord& file: displayedGroups.constFirst().getFiles())
    {
        displayedFilePaths.insert(file.getAbsoluteFilePath());
    }

    EXPECT_TRUE(displayedFilePaths.contains(QFileInfo(QDir(scanRootPath()).filePath(QStringLiteral("first/original.bin"))).absoluteFilePath()));
    EXPECT_TRUE(displayedFilePaths.contains(QFileInfo(QDir(scanRootPath()).filePath(QStringLiteral("second/copy.bin"))).absoluteFilePath()));
    EXPECT_TRUE(tabWidget->isTabVisible(tabWidget->indexOf(resultsTab)));
    EXPECT_EQ(tabWidget->currentWidget(), resultsTab);
    EXPECT_TRUE(exportAction->isEnabled());
    EXPECT_EQ(QApplication::activeModalWidget(), nullptr);
}

/// @brief Verifies the complete UI-to-scanner failure path when a selected scan root does not exist.
///
/// @par Test setup
/// Construct `Model`, `MainWindow`, and `Controller`, add a nonexistent path below the temporary root to the
/// model, and locate the Start scan control and export action.
///
/// @par Procedure
/// Click Start scan and process Qt events while the real asynchronous scanner rejects the missing root. Capture
/// and close the modal outcome dialog, then inspect the progress and result-related state.
///
/// @par Expected results
/// - The backend failure reaches the controller through the scanner's completion signal.
/// - A critical "Scan failed" message explains that the operation could not be completed.
/// - The progress dialog is closed, no duplicate groups are displayed, and export remains disabled.
TEST_F(ApplicationScanIntegrationTest, ReportFailure_WhenScanRootDoesNotExist)
{
    const QString missingScanRootPath = QDir(scanRootPath()).filePath(QStringLiteral("missing"));
    ASSERT_FALSE(QFileInfo::exists(missingScanRootPath));

    Model model;
    MainWindow mainWindow;
    Controller controller(model, mainWindow);

    ASSERT_EQ(model.addScanDirectory(missingScanRootPath).outcome, Model::AddScanDirectoryOutcome::Added);

    auto* startScanButton = mainWindow.findChild<QPushButton*>(QStringLiteral("startScan_PushButton"));
    auto* exportAction = mainWindow.findChild<QAction*>(QStringLiteral("exportToHtml_Action"));

    ASSERT_NE(startScanButton, nullptr);
    ASSERT_NE(exportAction, nullptr);

    mainWindow.show();
    QApplication::processEvents();

    const ObservedMessageBox messageBox = activateAndObserveMessageBox(
        [startScanButton] { startScanButton->click(); },
        5000);

    ASSERT_FALSE(messageBox.waitTimedOut);
    ASSERT_TRUE(messageBox.wasShown);
    EXPECT_EQ(messageBox.icon, QMessageBox::Critical);
    EXPECT_EQ(messageBox.windowTitle.toStdString(), std::string("Scan failed"));
    EXPECT_EQ(messageBox.text.toStdString(),
              std::string("The scan could not be completed. Check the application log for details."));

    if (const auto* progressDialog = mainWindow.findChild<QProgressDialog*>())
    {
        EXPECT_FALSE(progressDialog->isVisible());
    }

    EXPECT_TRUE(mainWindow.getDisplayedDuplicateGroups().isEmpty());
    EXPECT_FALSE(exportAction->isEnabled());
    EXPECT_EQ(QApplication::activeModalWidget(), nullptr);
}

/// @brief Verifies that closing the Results tab clears both authoritative result state and its presentation.
///
/// @par Test setup
/// Construct and connect `Model`, `MainWindow`, and `Controller`. Put the model through a scan-state transition
/// ending with a duplicate result, then locate the result-related widgets and custom close button.
///
/// @par Procedure
/// Confirm that the completed result is stored and displayed, click the Results-tab close button, and inspect
/// the model and window after the close request has passed through the controller wiring.
///
/// @par Expected results
/// - Before the click, the model contains duplicate results and the Results tab is visible and active.
/// - Clicking the close button removes the latest result from the model.
/// - The Results tab is hidden, Directories becomes active, export is disabled, and no duplicate groups remain displayed.
TEST_F(ApplicationScanIntegrationTest, ClearModelAndViewResult_WhenResultsTabCloseButtonIsClicked)
{
    Model model;
    MainWindow mainWindow;
    Controller controller(model, mainWindow);

    ASSERT_EQ(model.addScanDirectory(scanRootPath()).outcome, Model::AddScanDirectoryOutcome::Added);
    ASSERT_EQ(model.beginScan(), Model::ScanStartOutcome::Started);
    model.completeScan(test_helpers::createScanResultWithDuplicates());

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
    ASSERT_TRUE(model.hasDuplicateResults());
    ASSERT_TRUE(tabWidget->isTabVisible(tabWidget->indexOf(resultsTab)));
    ASSERT_EQ(tabWidget->currentWidget(), resultsTab);

    closeButton->click();

    EXPECT_FALSE(model.getLatestScanResult().has_value());
    EXPECT_FALSE(tabWidget->isTabVisible(tabWidget->indexOf(resultsTab)));
    EXPECT_EQ(tabWidget->currentWidget(), directoriesTab);
    EXPECT_FALSE(exportAction->isEnabled());
    EXPECT_TRUE(mainWindow.getDisplayedDuplicateGroups().isEmpty());
}
