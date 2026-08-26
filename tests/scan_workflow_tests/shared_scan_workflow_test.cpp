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

/// Verifies that scanning an empty directory produces a no-files-found outcome.
TYPED_TEST(SharedScanWorkflowTest, CheckNoFilesOutcome_EmptyDirectory)
{
    const ScanResult scanResult = TypeParam().execute(QStringList{this->getTemporaryScanRootPath()},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::NoFilesFound);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that a scan returns a cancelled outcome when stopping is requested before it begins.
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

/// Verifies that scanning a nonexistent root directory produces a failed outcome.
TYPED_TEST(SharedScanWorkflowTest, CheckFailedOutcome_RootDirectoryDoesNotExist)
{
    const QString missingRootPath = QDir(this->getTemporaryScanRootPath()).filePath("missing");
    const ScanResult scanResult = TypeParam().execute(QStringList{missingRootPath},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Failed);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that scanning one unique file completes successfully without duplicate groups.
TYPED_TEST(SharedScanWorkflowTest, CheckCompletedWithoutDuplicatesOutcome_DirectoryContainsOneUniqueFile)
{
    ASSERT_TRUE(this->writeFile("only-file", "unique"));

    const ScanResult scanResult = TypeParam().execute(QStringList{this->getTemporaryScanRootPath()},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithoutDuplicates);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that matching files from two separate scan roots form one duplicate group.
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

/// Verifies that one scan across three roots finds two independent cross-root duplicate groups:
///
/// first-root/alpha.txt  <->  second-root/alpha.log
/// second-root/beta.bin <->  third-root/beta.txt
///
/// The unrelated file in third-root must not be included in either group.
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

/// Verifies that an empty root does not prevent files in a later root from being scanned.
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

/// Verifies that the first root is enumerated before an invalid second root causes the scan to fail.
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

/// Verifies that a scan can be cancelled after enumeration has advanced from the first root into the second root.
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

/// Verifies that invoking a workflow without any root directories fails cleanly.
TYPED_TEST(SharedScanWorkflowTest, CheckFailedOutcome_EmptyRootList)
{
    const ScanResult scanResult = TypeParam().execute({},
                                                      std::stop_source().get_token(),
                                                      this->ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Failed);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that repeated and nested roots passed directly to a workflow do not scan any file more than once.
/// This is tested by recording the final enumeration progress count and requiring it to equal the two distinct
/// files in parent-root, despite passing parent-root twice and its nested-root separately.
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

/// Verifies that a file which is already unreadable when discovered is logged, counted, and ignored by both workflows.
/// The readable file remains the only contributor to the scan's file and byte totals.
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

/// Verifies that a directory containing only a discovery-time unreadable file behaves like a directory with no files,
/// while the skipped file is still represented by the problematic-file summary count.
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
