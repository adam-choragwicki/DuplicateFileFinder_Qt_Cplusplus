#include "scan_test_fixtures.h"

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
