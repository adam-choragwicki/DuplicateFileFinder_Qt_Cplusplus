#include "scan_workflow_test_fixture.h"
#include "unreadable_file_scan_test_helpers.h"

#include "file_content_scan_workflow.h"
#include "file_name_scan_workflow.h"

#include <QDir>

#include <stop_token>
#include <string>
#include <type_traits>

namespace
{
    /// GoogleTest requires a typed-test fixture to be a class template whose template parameter receives the current workflow type.
    template<typename Workflow>
    class SharedScanWorkflowTest : public ScanWorkflowTest
    {};

    /// GoogleTest type list that makes every TYPED_TEST below run once for each workflow.
    using ScanWorkflowImplementations = ::testing::Types<FileNameScanWorkflow, FileContentScanWorkflow>;

    /// Optional GoogleTest typed-test name generator, used to produce readable test names.
    struct ScanWorkflowTypeNames
    {
        /// GetName is the static interface expected by GoogleTest for a custom type-name generator.
        template<typename Workflow>
        static std::string GetName(int)
        {
            if constexpr (std::is_same_v<Workflow, FileNameScanWorkflow>)
            {
                return "FileName";
            }

            return "FileContent";
        }
    };

    /// Registers the fixture, workflow type list, and custom name generator with GoogleTest.
    TYPED_TEST_SUITE(SharedScanWorkflowTest, ScanWorkflowImplementations, ScanWorkflowTypeNames);
}

/// @brief Verifies the result produced when a scan root contains no files.
///
/// @par Test setup
/// Use the fixture's valid but empty temporary directory as the only scan root.
///
/// @par Procedure
/// Execute each workflow implementation with no cancellation request and ignore progress updates.
///
/// @par Expected results
/// The workflow reports `ScanOutcome::NoFilesFound` and returns no duplicate groups.
TYPED_TEST(SharedScanWorkflowTest, CheckNoFilesOutcome_EmptyDirectory)
{
    const ScanResult scanResult = TypeParam().execute(QStringList{this->getTemporaryScanRootPath()},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::NoFilesFound);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// @brief Verifies that each workflow honors a cancellation request made before execution.
///
/// @par Test setup
/// Create a stop source, request cancellation immediately, and use the fixture's valid temporary root.
///
/// @par Procedure
/// Execute each workflow with the already-requested stop token and ignore progress updates.
///
/// @par Expected results
/// The workflow reports `ScanOutcome::Cancelled` and returns no duplicate groups.
TYPED_TEST(SharedScanWorkflowTest, CheckCancelledOutcome_StopRequestedBeforeScan)
{
    std::stop_source stopSource;
    stopSource.request_stop();

    const ScanResult scanResult = TypeParam().execute(QStringList{this->getTemporaryScanRootPath()},
                                                      stopSource.get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Cancelled);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// @brief Verifies that each workflow rejects a scan root that does not exist.
///
/// @par Test setup
/// Construct a missing path below the fixture's valid temporary directory without creating it.
///
/// @par Procedure
/// Execute each workflow with the missing path as its only root and no cancellation request.
///
/// @par Expected results
/// The workflow reports `ScanOutcome::Failed` and returns no duplicate groups.
TYPED_TEST(SharedScanWorkflowTest, CheckFailedOutcome_RootDirectoryDoesNotExist)
{
    const QString missingRootPath = QDir(this->getTemporaryScanRootPath()).filePath("missing");
    const ScanResult scanResult = TypeParam().execute(QStringList{missingRootPath},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Failed);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// @brief Verifies successful completion when the selected root contains one unique file.
///
/// @par Test setup
/// Create one file in the fixture's temporary scan root.
///
/// @par Procedure
/// Execute each workflow over that root with no cancellation request.
///
/// @par Expected results
/// The workflow reports `ScanOutcome::CompletedWithoutDuplicates` and returns no duplicate groups.
TYPED_TEST(SharedScanWorkflowTest, CheckCompletedWithoutDuplicatesOutcome_DirectoryContainsOneUniqueFile)
{
    ASSERT_TRUE(this->writeFile("only-file", "unique"));

    const ScanResult scanResult = TypeParam().execute(QStringList{this->getTemporaryScanRootPath()},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithoutDuplicates);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// @brief Verifies that matching files located in different scan roots form one duplicate group.
///
/// @par Test setup
/// Create one four-byte file in each of two roots. Give them the same base name and identical contents so they
/// qualify as duplicates under both workflow implementations, then construct the expected cross-root group.
///
/// @par Procedure
/// Execute each workflow with both roots and compare the returned group with the expected file records.
///
/// @par Expected results
/// The scan completes with duplicates and returns exactly one group containing both cross-root files.
TYPED_TEST(SharedScanWorkflowTest, CheckDuplicateGroup_FilesAreInDifferentRoots)
{
    ASSERT_TRUE(this->writeFile("first-root/shared.txt", "same"));
    ASSERT_TRUE(this->writeFile("second-root/shared.log", "same"));

    const QString firstRootPath = this->getTemporaryDirectoryPath("first-root");
    const QString secondRootPath = this->getTemporaryDirectoryPath("second-root");
    const DuplicateGroup expectedDuplicateGroup = this->createExpectedDuplicateGroup({
        this->createExpectedFileRecord(firstRootPath, "shared.txt", 4),
        this->createExpectedFileRecord(secondRootPath, "shared.log", 4)
    });

    const ScanResult scanResult = TypeParam().execute(QStringList{firstRootPath, secondRootPath},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_TRUE(this->containsDuplicateGroup(scanResult, expectedDuplicateGroup));
}

/// @brief Verifies that one scan can produce independent duplicate groups spanning several roots.
///
/// @par Test setup
/// Create matching `alpha` files across the first and second roots, matching `beta` files across the second and
/// third roots, and one unrelated file. Names and contents are chosen to match under both workflows.
///
/// @par Procedure
/// Execute each workflow over all three roots and compare its groups with the expected alpha and beta groups.
///
/// @par Expected results
/// The scan completes with exactly two duplicate groups, both expected cross-root groups are present, and the
/// unrelated file is excluded.
TYPED_TEST(SharedScanWorkflowTest, CheckDuplicateGroup_DuplicateGroupsSpreadAcrossSeveralRoots)
{
    ASSERT_TRUE(this->writeFile("first-root/alpha.txt", "alpha"));
    ASSERT_TRUE(this->writeFile("second-root/alpha.log", "alpha"));
    ASSERT_TRUE(this->writeFile("second-root/beta.bin", "beta"));
    ASSERT_TRUE(this->writeFile("third-root/beta.txt", "beta"));
    ASSERT_TRUE(this->writeFile("third-root/unique", "unique"));

    const QString firstRootPath = this->getTemporaryDirectoryPath("first-root");
    const QString secondRootPath = this->getTemporaryDirectoryPath("second-root");
    const QString thirdRootPath = this->getTemporaryDirectoryPath("third-root");

    const DuplicateGroup expectedAlphaGroup = this->createExpectedDuplicateGroup({
        this->createExpectedFileRecord(firstRootPath, "alpha.txt", 5),
        this->createExpectedFileRecord(secondRootPath, "alpha.log", 5)
    });

    const DuplicateGroup expectedBetaGroup = this->createExpectedDuplicateGroup({
        this->createExpectedFileRecord(secondRootPath, "beta.bin", 4),
        this->createExpectedFileRecord(thirdRootPath, "beta.txt", 4)
    });

    const ScanResult scanResult = TypeParam().execute(QStringList{firstRootPath, secondRootPath, thirdRootPath},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 2);
    EXPECT_TRUE(this->containsDuplicateGroup(scanResult, expectedAlphaGroup));
    EXPECT_TRUE(this->containsDuplicateGroup(scanResult, expectedBetaGroup));
}

/// @brief Verifies that an empty first root does not prevent a later non-empty root from being scanned.
///
/// @par Test setup
/// Create an empty first root and a second root containing one unique file.
///
/// @par Procedure
/// Execute each workflow with the empty root followed by the non-empty root.
///
/// @par Expected results
/// Both roots are accepted, the scan completes without duplicates, and no duplicate group is returned.
TYPED_TEST(SharedScanWorkflowTest, CheckCompletedWithoutDuplicatesOutcome_EmptyRootBeforeNonEmptyRoot)
{
    ASSERT_TRUE(this->createDirectory("empty-root"));
    ASSERT_TRUE(this->writeFile("non-empty-root/only-file", "unique"));

    const QStringList rootPaths{
        this->getTemporaryDirectoryPath("empty-root"),
        this->getTemporaryDirectoryPath("non-empty-root")
    };

    const ScanResult scanResult = TypeParam().execute(rootPaths,
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithoutDuplicates);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// @brief Verifies processing order and failure handling when a valid root precedes an invalid root.
///
/// @par Test setup
/// Create one file in a valid first root, leave the second root missing, and prepare a progress observer that
/// records whether one file was enumerated before failure.
///
/// @par Procedure
/// Execute each workflow with the valid root followed by the missing root and observe enumeration progress.
///
/// @par Expected results
/// The first-root file is enumerated, the invalid second root makes the scan fail, and no duplicate groups are
/// returned.
TYPED_TEST(SharedScanWorkflowTest, CheckFailedOutcome_InvalidSecondRootAfterFirstRootEnumerated)
{
    ASSERT_TRUE(this->writeFile("valid-root/enumerated-file", "contents"));

    bool firstRootFileWasEnumerated = false;

    const QStringList rootPaths{
        this->getTemporaryDirectoryPath("valid-root"),
        this->getTemporaryDirectoryPath("missing-root")
    };

    const ScanResult scanResult = TypeParam().execute(rootPaths,
                                                      std::stop_source().get_token(),
                                                      [&firstRootFileWasEnumerated](const ScanProgress& scanProgress)
                                                      {
                                                          if (!scanProgress.totalFilesCount.has_value()
                                                              && scanProgress.processedFilesCount == 1)
                                                          {
                                                              firstRootFileWasEnumerated = true;
                                                          }
                                                      });

    EXPECT_TRUE(firstRootFileWasEnumerated);
    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Failed);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// @brief Verifies cancellation after enumeration advances from the first root into the second root.
///
/// @par Test setup
/// Create one file in the first root and two files in the second. Configure a progress observer to request stop
/// when the cumulative enumeration count reaches two.
///
/// @par Procedure
/// Execute each workflow over both roots with the observer-controlled stop token.
///
/// @par Expected results
/// Cancellation is requested while processing the second root, the outcome is `Cancelled`, and no duplicate
/// groups are returned.
TYPED_TEST(SharedScanWorkflowTest, CheckCancelledOutcome_StopRequestedWhileEnumeratingSecondRoot)
{
    ASSERT_TRUE(this->writeFile("first-root/only-file", "first"));
    ASSERT_TRUE(this->writeFile("second-root/first-file", "second"));
    ASSERT_TRUE(this->writeFile("second-root/second-file", "third"));

    std::stop_source stopSource;
    bool stopWasRequestedInSecondRoot = false;
    const QStringList rootPaths{
        this->getTemporaryDirectoryPath("first-root"),
        this->getTemporaryDirectoryPath("second-root")
    };

    const ScanResult scanResult = TypeParam().execute(rootPaths,
                                                      stopSource.get_token(),
                                                      [&stopSource, &stopWasRequestedInSecondRoot](const ScanProgress& scanProgress)
                                                      {
                                                          if (!stopWasRequestedInSecondRoot
                                                              && !scanProgress.totalFilesCount.has_value()
                                                              && scanProgress.processedFilesCount == 2)
                                                          {
                                                              stopWasRequestedInSecondRoot = stopSource.request_stop();
                                                          }
                                                      });

    EXPECT_TRUE(stopWasRequestedInSecondRoot);
    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Cancelled);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// @brief Verifies that each workflow rejects an empty scan-root list.
///
/// @par Test setup
/// Prepare an empty root list and an unrequested stop token.
///
/// @par Procedure
/// Execute each workflow without supplying any directory.
///
/// @par Expected results
/// The workflow reports `ScanOutcome::Failed` and returns no duplicate groups.
TYPED_TEST(SharedScanWorkflowTest, CheckFailedOutcome_EmptyRootList)
{
    const ScanResult scanResult = TypeParam().execute({},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Failed);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// @brief Verifies that repeated and nested scan roots do not cause files to be scanned more than once.
///
/// @par Test setup
/// Create one top-level file and one nested file. Supply the nested root separately and the parent root twice,
/// then observe the cumulative enumeration count.
///
/// @par Procedure
/// Execute each workflow with the overlapping root list and record the most recent enumeration count.
///
/// @par Expected results
/// The scan completes without duplicates, returns no groups, and enumerates exactly the two distinct files.
TYPED_TEST(SharedScanWorkflowTest, CheckFilesScannedOnce_RepeatedAndNestedRoots)
{
    ASSERT_TRUE(this->writeFile("parent-root/top-level-file", "top"));
    ASSERT_TRUE(this->writeFile("parent-root/nested-root/nested-file", "nested"));

    const QString parentRootPath = this->getTemporaryDirectoryPath("parent-root");
    const QString nestedRootPath = this->getTemporaryDirectoryPath("parent-root/nested-root");

    const QStringList rootPaths{
        nestedRootPath,
        parentRootPath,
        parentRootPath // parentRootPath is repeated
    };

    quint64 lastEnumeratedFilesCount = 0;
    const ScanResult scanResult = TypeParam().execute(rootPaths,
                                                      std::stop_source().get_token(),
                                                      [&lastEnumeratedFilesCount](const ScanProgress& scanProgress)
                                                      {
                                                          if (!scanProgress.totalFilesCount.has_value())
                                                          {
                                                              lastEnumeratedFilesCount = scanProgress.processedFilesCount;
                                                          }
                                                      });

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithoutDuplicates);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
    EXPECT_EQ(lastEnumeratedFilesCount, 2);
}

/// @brief Verifies that a file already unreadable during discovery is logged, counted, and skipped.
///
/// @par Test setup
/// Create one readable and one unreadable file, skip the scenario when the platform cannot enforce restricted
/// access, and capture Qt log messages.
///
/// @par Procedure
/// Execute each workflow over the root, then inspect the outcome, log, problematic-file count, and summary.
///
/// @par Expected results
/// The scan completes without duplicates, reports and logs one problematic file, and counts only the readable
/// eight-byte file in the scanned-file and total-byte metrics.
TYPED_TEST(SharedScanWorkflowTest, CheckProblematicFileSkipped_FileUnreadableWhenDiscovered)
{
    ASSERT_TRUE(this->writeFile("readable-file", "readable"));
    ASSERT_TRUE(this->writeFile("unreadable-file", "unreadable"));

    const QString unreadableFilePath = this->getTemporaryDirectoryPath("unreadable-file");
    const ScopedUnreadableFile unreadableFile(unreadableFilePath);

    if (!unreadableFile.isUnreadable())
    {
        GTEST_SKIP() << "The current account can still read a file after its access was restricted";
    }

    const ScopedLogCapture logCapture;
    const ScanResult scanResult = TypeParam().execute(QStringList{this->getTemporaryScanRootPath()},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithoutDuplicates);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
    EXPECT_EQ(scanResult.getProblematicFilesCount(), 1);
    EXPECT_TRUE(logCapture.contains(unreadableFilePath));

    std::visit(
        [](const auto& summary)
        {
            EXPECT_EQ(summary.getScannedFilesCount(), 1);
            EXPECT_EQ(summary.getTotalScannedBytes(), 8);
        },
        scanResult.getScanSummaryDetails());
}

/// @brief Verifies the outcome when the only discovered file is unreadable.
///
/// @par Test setup
/// Create one file, make it unreadable, and skip the scenario when the platform cannot enforce restricted access.
///
/// @par Procedure
/// Execute each workflow over the root and inspect the result and common summary metrics.
///
/// @par Expected results
/// The workflow reports `NoFilesFound`, returns no groups, counts one problematic file, and reports zero scanned
/// files and zero scanned bytes.
TYPED_TEST(SharedScanWorkflowTest, CheckNoFilesOutcome_OnlyFileUnreadableWhenDiscovered)
{
    ASSERT_TRUE(this->writeFile("unreadable-file", "unreadable"));

    const QString unreadableFilePath = this->getTemporaryDirectoryPath("unreadable-file");
    const ScopedUnreadableFile unreadableFile(unreadableFilePath);

    if (!unreadableFile.isUnreadable())
    {
        GTEST_SKIP() << "The current account can still read a file after its access was restricted";
    }

    const ScanResult scanResult = TypeParam().execute(QStringList{this->getTemporaryScanRootPath()},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::NoFilesFound);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
    EXPECT_EQ(scanResult.getProblematicFilesCount(), 1);

    std::visit(
        [](const auto& summary)
        {
            EXPECT_EQ(summary.getScannedFilesCount(), 0);
            EXPECT_EQ(summary.getTotalScannedBytes(), 0);
        },
        scanResult.getScanSummaryDetails());
}
