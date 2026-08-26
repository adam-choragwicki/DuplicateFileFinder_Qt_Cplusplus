#include "scanner.h"
#include "scan_request.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>

#include <gtest/gtest.h>

#include <optional>
#include <variant>

namespace
{
    /// @brief Provides an isolated directory tree for asynchronous `Scanner` tests.
    class AsynchronousScannerTest : public ::testing::Test
    {
    protected:
        /// @brief Verifies that the fixture's temporary directory was created successfully.
        void SetUp() override
        {
            ASSERT_TRUE(temporaryDirectory_.isValid());
        }

        /// @brief Creates or replaces a file below the fixture's temporary scan root.
        ///
        /// @param relativePath File path relative to the temporary scan root.
        /// @param contents Complete byte sequence to write to the file.
        /// @return `true` when all parent directories and file contents were written successfully.
        [[nodiscard]] bool writeFile(const QString& relativePath, const QByteArray& contents) const
        {
            const QString absoluteFilePath = temporaryDirectory_.filePath(relativePath);

            if (!QDir().mkpath(QFileInfo(absoluteFilePath).absolutePath()))
            {
                return false;
            }

            QFile outputFile(absoluteFilePath);
            return outputFile.open(QIODevice::WriteOnly)
                   && outputFile.write(contents) == contents.size();
        }

        /// @brief Returns the absolute path scanned by each test.
        [[nodiscard]] QString scanRootPath() const
        {
            return temporaryDirectory_.path();
        }

    private:
        QTemporaryDir temporaryDirectory_;
    };
}

/// @brief Verifies the successful asynchronous scanner lifecycle and its completion result.
///
/// @par Test setup
/// Create two files with matching names below one temporary root. Attach observers to progress, completion,
/// and cancellation signals, and prepare a timeout that prevents an asynchronous failure from hanging the test.
///
/// @par Procedure
/// Start a file-name scan, verify that the scanner immediately becomes busy, and run the Qt event loop until
/// a terminal signal or timeout occurs.
///
/// @par Expected results
/// - At least one file-name progress update is emitted and begins with zero processed files and an unknown total.
/// - Exactly one completion signal and no cancellation signal are emitted.
/// - The result contains one two-file duplicate group and reports `CompletedWithDuplicates`.
/// - The scanner returns to its idle state before announcing completion.
TEST_F(AsynchronousScannerTest, EmitCompletionAndBecomeIdle_WhenAsynchronousScanFinishes)
{
    ASSERT_TRUE(writeFile(QStringLiteral("first/report.txt"), QByteArrayLiteral("first contents")));
    ASSERT_TRUE(writeFile(QStringLiteral("second/report.txt"), QByteArrayLiteral("second contents")));

    Scanner scanner;

    QList<ScanProgress> progressUpdates;
    std::optional<ScanResult> completedResult;
    int completionCount = 0;
    int cancellationCount = 0;

    // This value is sampled inside the completion observer, rather than after the event loop exits, to prove
    // that Scanner clears its busy flag before clients are notified by scanComplete.
    bool scannerWasIdleAtCompletion = false;

    // A timeout is a test-harness failure: it means neither terminal signal arrived, possibly because the
    // worker, QFutureWatcher, or queued event delivery stopped working.
    bool timedOut = false;

    // Scanner finishes through Qt events: QFutureWatcher reports completion on Scanner's thread and the
    // progress timer also runs there. This local loop keeps processing those events while the test waits,
    // without starting another QApplication or blocking the entire test process.
    QEventLoop eventLoop;
    QTimer timeoutTimer;

    // The watchdog must fire only once. A repeating timeout could keep modifying state while assertions run.
    timeoutTimer.setSingleShot(true);

    // Observe every progressChanged emission. Scanner emits the initial zero/unknown-total update directly
    // when scan() starts, then its UI-thread timer publishes snapshots written by the worker thread.
    // Using eventLoop as the QObject context also disconnects this lambda automatically when the loop dies.
    QObject::connect(&scanner, &Scanner::progressChanged, &eventLoop,
                     [&progressUpdates](const ScanProgress& progress)
                     {
                         // Copy each value because the signal argument is a temporary observation; retaining
                         // references would not be valid after signal delivery returns.
                         progressUpdates.append(progress);
                     });

    // This is the expected terminal observer. It records all information that must be inspected at the exact
    // moment Scanner announces success, then releases the nested event loop so the test can assert it.
    QObject::connect(&scanner, &Scanner::scanComplete, &eventLoop,
                     [&](const ScanResult& scanResult)
                     {
                         // A counter, rather than a boolean, detects accidental duplicate terminal emissions.
                         ++completionCount;

                         // scanResult is passed by reference only for the duration of the signal. Store a copy
                         // so its outcome and duplicate groups remain available after this callback returns.
                         completedResult = scanResult;

                         // Scanner is required to become idle before clients react to completion. Otherwise a
                         // client could receive scanComplete and still be prevented from starting another scan.
                         scannerWasIdleAtCompletion = !scanner.isScanning();

                         // A terminal signal has arrived, so the watchdog is no longer needed.
                         timeoutTimer.stop();
                         eventLoop.quit();
                     });

    // Cancellation is an unexpected but valid terminal route. Observe it and stop waiting immediately so the
    // assertions report the wrong signal instead of hiding the problem behind a five-second timeout.
    QObject::connect(&scanner, &Scanner::scanCancelled, &eventLoop,
                     [&]
                     {
                         ++cancellationCount;
                         timeoutTimer.stop();
                         eventLoop.quit();
                     });

    // The watchdog protects the test suite from hanging forever if no terminal signal is delivered. It does
    // not cancel Scanner; it only marks the wait as failed.
    QObject::connect(&timeoutTimer, &QTimer::timeout, &eventLoop,
                     [&]
                     {
                         timedOut = true;
                         eventLoop.quit();
                     });

    // Start the watchdog before scheduling work so it covers the complete asynchronous lifecycle, including
    // QtConcurrent scheduling and QFutureWatcher notification.
    timeoutTimer.start(5000);

    // scan() selects FileNameScanWorkflow, initializes progress and busy state, schedules the workflow on the
    // thread pool, and returns without waiting for the filesystem scan to finish.
    scanner.scan(ScanRequest{{scanRootPath()}, ScanType::ByFileName});

    // No event-loop wait has occurred yet, so Scanner must already expose that it accepted active work.
    EXPECT_TRUE(scanner.isScanning());

    // Normally neither terminal signal can be handled until the loop starts. Keep the guard for the legal case
    // where Qt delivers completion unusually early; entering a fresh loop after the observer already called
    // quit() could otherwise make the test wait until the watchdog expires.
    if (completionCount == 0 && cancellationCount == 0)
    {
        eventLoop.exec();
    }

    // First distinguish a broken asynchronous lifecycle from an ordinary expectation failure below.
    ASSERT_FALSE(timedOut);

    // Successful work must choose exactly one terminal route: scanComplete once, scanCancelled never.
    EXPECT_EQ(completionCount, 1);
    EXPECT_EQ(cancellationCount, 0);

    // Check both the state observed during signal delivery and the stable state after the event loop returns.
    EXPECT_TRUE(scannerWasIdleAtCompletion);
    EXPECT_FALSE(scanner.isScanning());

    // These assertions focus on Scanner's initial progress contract. Detailed phase transitions and counters
    // are already verified by scan_workflow_tests through the workflows' direct progress callback.
    ASSERT_FALSE(progressUpdates.isEmpty());
    EXPECT_TRUE(std::holds_alternative<FileNameScanPhase>(progressUpdates.constFirst().scanPhase));
    EXPECT_EQ(progressUpdates.constFirst().processedFilesCount, 0);
    EXPECT_FALSE(progressUpdates.constFirst().totalFilesCount.has_value());

    // The small result check proves that the ScanResult produced on the worker thread crosses the
    // QFutureWatcher/scanComplete boundary intact. It intentionally does not repeat every grouping assertion.
    ASSERT_TRUE(completedResult.has_value());
    EXPECT_EQ(completedResult->getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(completedResult->getDuplicateGroups().size(), 1);
    EXPECT_EQ(completedResult->getDuplicateGroups().constFirst().getFiles().size(), 2);
}

/// @brief Verifies that cancelling an active asynchronous scan produces only the cancellation terminal signal.
///
/// @par Test setup
/// Create a scanner for a valid temporary root. Attach completion and cancellation observers and prepare a
/// timeout that prevents an asynchronous failure from hanging the test.
///
/// @par Procedure
/// Start a file-name scan, request cancellation before processing queued completion events, and run the Qt
/// event loop until a terminal signal or timeout occurs.
///
/// @par Expected results
/// - The scanner enters its busy state before cancellation is requested.
/// - Exactly one cancellation signal and no completion signal are emitted.
/// - The scanner returns to its idle state before announcing cancellation.
TEST_F(AsynchronousScannerTest, EmitCancellationAndBecomeIdle_WhenActiveScanIsCancelled)
{
    // This test verifies Scanner's wrapper-level rule: a stop request made before its completion
    // handler runs must select scanCancelled, even if the background workflow finishes very quickly.
    Scanner scanner;

    // Count both terminal signals so the test proves they are mutually exclusive, not merely that the expected
    // cancellation signal happened at least once.
    int completionCount = 0;
    int cancellationCount = 0;

    // Sample the busy flag inside the expected observer to verify ordering at the public signal boundary.
    bool scannerWasIdleAtCancellation = false;

    // This becomes true only if Qt fails to deliver either terminal signal within the allowed time.
    bool timedOut = false;

    // QFutureWatcher completion and the Scanner terminal signals are delivered through Qt's event system.
    // The local loop lets those events run while keeping the wait scoped to this individual test.
    QEventLoop eventLoop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    // Completion is the unexpected terminal route after cancelScan(). Stop waiting immediately if it occurs so
    // GoogleTest can report completionCount == 1 and cancellationCount == 0 without waiting for the watchdog.
    QObject::connect(&scanner, &Scanner::scanComplete, &eventLoop,
                     [&](const ScanResult&)
                     {
                         ++completionCount;
                         timeoutTimer.stop();
                         eventLoop.quit();
                     });

    // Cancellation is the expected terminal observer. Besides counting the signal, it checks Scanner's state
    // during delivery: clients receiving scanCancelled must already be allowed to start another scan.
    QObject::connect(&scanner, &Scanner::scanCancelled, &eventLoop,
                     [&]
                     {
                         ++cancellationCount;
                         scannerWasIdleAtCancellation = !scanner.isScanning();
                         timeoutTimer.stop();
                         eventLoop.quit();
                     });

    // The timeout is only a deadlock/missing-signal guard for the test harness. It intentionally does not call
    // cancelScan(), because cancellation is the behavior under test and is requested explicitly below.
    QObject::connect(&timeoutTimer, &QTimer::timeout, &eventLoop,
                     [&]
                     {
                         timedOut = true;
                         eventLoop.quit();
                     });

    // Cover the entire scheduling and completion path with the watchdog.
    timeoutTimer.start(5000);

    // scan() returns after scheduling FileNameScanWorkflow on Qt's thread pool. The valid temporary root may be
    // processed quickly, but Scanner cannot dispatch its watcher completion until Qt events are processed.
    scanner.scan(ScanRequest{{scanRootPath()}, ScanType::ByFileName});

    // Prove that scan() synchronously entered the busy state before cancellation is requested.
    ASSERT_TRUE(scanner.isScanning());

    // cancelScan() requests the shared stop token. It does not synchronously emit scanCancelled or clear the
    // busy flag; Scanner does that later in its QFutureWatcher completion handler.
    scanner.cancelScan();

    // Avoid entering the nested loop if a terminal observer has already run. In the ordinary path this starts
    // event processing so QFutureWatcher can turn the requested stop into scanCancelled.
    if (completionCount == 0 && cancellationCount == 0)
    {
        eventLoop.exec();
    }

    // A timeout means Scanner never completed its public asynchronous lifecycle.
    ASSERT_FALSE(timedOut);

    // Cancellation must select exactly one terminal route and suppress scanComplete.
    EXPECT_EQ(completionCount, 0);
    EXPECT_EQ(cancellationCount, 1);

    // Verify idle-state ordering both at signal delivery and after the wait has finished.
    EXPECT_TRUE(scannerWasIdleAtCancellation);
    EXPECT_FALSE(scanner.isScanning());
}
