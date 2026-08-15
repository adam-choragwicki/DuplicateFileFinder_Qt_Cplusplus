#include "scan_test_fixtures.h"

#include "file_name_scan_workflow.h"
#include "scan_summary/file_name_scan_summary.h"

#include <stop_token>
#include <variant>

namespace
{
    class ScanByFileNameTest : public ScanWorkflowTest
    {};
}

/// Verifies that scanning the smoke-test file system scenario tree by file name returns the two expected duplicate groups and a file-name scan summary.
TEST_F(ScanByFileNameTest, CheckDuplicateGroups_SmokeTest)
{
    const QString scanRootPath = getSmokeTestScanRootPath();

    const DuplicateGroup expectedThreeFileDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "dir1/duplicate_three.txt", 52),
        createExpectedFileRecord(scanRootPath, "dir2/duplicate_three.txt", 91),
        createExpectedFileRecord(scanRootPath, "dir3/duplicate_three.txt", 52)
    });

    const DuplicateGroup expectedTwoFileDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "dir1/duplicate_two.txt", 50),
        createExpectedFileRecord(scanRootPath, "dir2/duplicate_two.txt", 51)
    });

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{scanRootPath},
                                                                 std::stop_source().get_token(),
                                                                 ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 2);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedThreeFileDuplicateGroup));
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedTwoFileDuplicateGroup));
    EXPECT_TRUE(std::holds_alternative<FileNameScanSummary>(scanResult.getScanSummaryDetails()));
}

/// Verifies that files with the same base name are grouped when only their final extensions differ.
TEST_F(ScanByFileNameTest, CheckDuplicateGroups_FileNamesDifferOnlyByFinalExtension)
{
    ASSERT_TRUE(writeFile("documents/report.txt", "text report"));
    ASSERT_TRUE(writeFile("exports/report.pdf", "PDF report with different contents and size"));
    ASSERT_TRUE(writeFile("other/unrelated.txt", "unrelated"));

    const QString scanRootPath = getTemporaryScanRootPath();
    const DuplicateGroup expectedDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "documents/report.txt", 11),
        createExpectedFileRecord(scanRootPath, "exports/report.pdf", 43)
    });

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{scanRootPath},
                                                                 std::stop_source().get_token(),
                                                                 ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedDuplicateGroup));
}

/// Verifies that equal extensionless file names are grouped without requiring a dot or extension.
TEST_F(ScanByFileNameTest, CheckDuplicateGroups_ExtensionlessFileNames)
{
    ASSERT_TRUE(writeFile("first/LICENSE", "first license"));
    ASSERT_TRUE(writeFile("second/LICENSE", "second license with different contents"));
    ASSERT_TRUE(writeFile("third/README", "read me"));

    const QString scanRootPath = getTemporaryScanRootPath();
    const DuplicateGroup expectedDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "first/LICENSE", 13),
        createExpectedFileRecord(scanRootPath, "second/LICENSE", 38)
    });

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{scanRootPath},
                                                                 std::stop_source().get_token(),
                                                                 ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedDuplicateGroup));
}

/// Verifies that a leading dot marks a file name rather than an extension and does not merge unrelated hidden files.
TEST_F(ScanByFileNameTest, CheckDuplicateGroups_LeadingDotFileNamesAreNotExtensions)
{
    ASSERT_TRUE(writeFile("first/.bashrc", "first settings"));
    ASSERT_TRUE(writeFile("second/.bashrc", "second settings"));
    ASSERT_TRUE(writeFile("third/.profile", "profile settings"));

    const QString scanRootPath = getTemporaryScanRootPath();
    const DuplicateGroup expectedDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "first/.bashrc", 14),
        createExpectedFileRecord(scanRootPath, "second/.bashrc", 15)
    });

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{scanRootPath},
                                                                 std::stop_source().get_token(),
                                                                 ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedDuplicateGroup));
}

/// Verifies that only the final extension is ignored when comparing file names containing multiple dots.
TEST_F(ScanByFileNameTest, CheckDuplicateGroups_OnlyFinalExtensionIsIgnored)
{
    ASSERT_TRUE(writeFile("first/archive.tar.gz", "gzip archive"));
    ASSERT_TRUE(writeFile("second/archive.tar.zip", "zip archive with different contents"));
    ASSERT_TRUE(writeFile("third/archive.zip", "different base name"));

    const QString scanRootPath = getTemporaryScanRootPath();
    const DuplicateGroup expectedDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "first/archive.tar.gz", 12),
        createExpectedFileRecord(scanRootPath, "second/archive.tar.zip", 35)
    });

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{scanRootPath},
                                                                 std::stop_source().get_token(),
                                                                 ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedDuplicateGroup));
}

/// Verifies that a successful file-name scan reports only the expected workflow phases and reports them in execution order.
TEST_F(ScanByFileNameTest, CheckProgressPhases_SuccessfulScan)
{
    QList<ScanProgress> progressUpdates;

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{getSmokeTestScanRootPath()},
                                                                 std::stop_source().get_token(),
                                                                 [&progressUpdates](const ScanProgress& scanProgress)
                                                                 {
                                                                     progressUpdates.append(scanProgress);
                                                                 });

    ASSERT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    EXPECT_TRUE(containsOnlyPhasesFor<FileNameScanPhase>(progressUpdates));
    EXPECT_EQ(getPhaseTransitions<FileNameScanPhase>(progressUpdates), (QList<FileNameScanPhase>{
                  FileNameScanPhase::EnumeratingFiles,
                  FileNameScanPhase::GroupingFilesByName,
                  FileNameScanPhase::BuildingScanResult}));
}

/// Verifies that file-name scan progress counters are monotonic within each phase and finish at the known file total.
TEST_F(ScanByFileNameTest, CheckProgressCounters_SuccessfulScan)
{
    ASSERT_TRUE(writeFile("first/shared.txt", "same"));
    ASSERT_TRUE(writeFile("second/shared.log", "same"));
    ASSERT_TRUE(writeFile("unique.txt", "unique"));

    QList<ScanProgress> progressUpdates;
    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
                                                                 std::stop_source().get_token(),
                                                                 [&progressUpdates](const ScanProgress& scanProgress)
                                                                 {
                                                                     progressUpdates.append(scanProgress);
                                                                 });

    ASSERT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);

    constexpr quint64 expectedFilesCount = 3;
    quint64 lastEnumeratedFilesCount = 0;
    quint64 lastGroupedFilesCount = 0;
    bool buildingResultPhaseWasReported = false;

    for (const ScanProgress& scanProgress: progressUpdates)
    {
        const auto* phase = std::get_if<FileNameScanPhase>(&scanProgress.scanPhase);
        ASSERT_NE(phase, nullptr);

        if (*phase == FileNameScanPhase::EnumeratingFiles)
        {
            EXPECT_FALSE(scanProgress.totalFilesCount.has_value());
            EXPECT_GE(scanProgress.processedFilesCount, lastEnumeratedFilesCount);
            lastEnumeratedFilesCount = scanProgress.processedFilesCount;
        }
        else if (*phase == FileNameScanPhase::GroupingFilesByName)
        {
            ASSERT_TRUE(scanProgress.totalFilesCount.has_value());
            EXPECT_EQ(*scanProgress.totalFilesCount, expectedFilesCount);
            EXPECT_GE(scanProgress.processedFilesCount, lastGroupedFilesCount);
            EXPECT_LE(scanProgress.processedFilesCount, *scanProgress.totalFilesCount);
            lastGroupedFilesCount = scanProgress.processedFilesCount;
        }
        else if (*phase == FileNameScanPhase::BuildingScanResult)
        {
            ASSERT_TRUE(scanProgress.totalFilesCount.has_value());
            EXPECT_EQ(scanProgress.processedFilesCount, expectedFilesCount);
            EXPECT_EQ(*scanProgress.totalFilesCount, expectedFilesCount);
            buildingResultPhaseWasReported = true;
        }
    }

    EXPECT_EQ(lastEnumeratedFilesCount, expectedFilesCount);
    EXPECT_EQ(lastGroupedFilesCount, expectedFilesCount);
    EXPECT_TRUE(buildingResultPhaseWasReported);
}

/// Verifies that file-name scan progress counters remain cumulative while enumeration advances across multiple roots.
/// Cumulative means that the processed-files count continues from the first root's 2 files while the second
/// root's 3 files are enumerated, rather than restarting at 0.
/// The test requires the exact enumeration sequence 0, 1, 2, 3, 4, 5, nondecreasing grouping counts bounded by the total of five, and a final result update of 5/5.
TEST_F(ScanByFileNameTest, CheckProgressCounters_MultipleRoots)
{
    ASSERT_TRUE(writeFile("first-root/shared.txt", "same"));
    ASSERT_TRUE(writeFile("first-root/first-only", "a"));
    ASSERT_TRUE(writeFile("second-root/shared.log", "same"));
    ASSERT_TRUE(writeFile("second-root/second-only", "bb"));
    ASSERT_TRUE(writeFile("second-root/third-only", "ccc"));

    QList<ScanProgress> progressUpdates;
    const QStringList rootPaths{
        getTemporaryDirectoryPath("first-root"),
        getTemporaryDirectoryPath("second-root")
    };
    const ScanResult scanResult = FileNameScanWorkflow().execute(rootPaths,
                                                                 std::stop_source().get_token(),
                                                                 [&progressUpdates](const ScanProgress& scanProgress)
                                                                 {
                                                                     progressUpdates.append(scanProgress);
                                                                 });

    ASSERT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);

    constexpr quint64 expectedFilesCount = 5;
    QList<quint64> enumerationCounts;
    quint64 lastGroupedFilesCount = 0;
    bool buildingResultPhaseWasReported = false;

    for (const ScanProgress& scanProgress: progressUpdates)
    {
        const auto* phase = std::get_if<FileNameScanPhase>(&scanProgress.scanPhase);
        ASSERT_NE(phase, nullptr);

        if (*phase == FileNameScanPhase::EnumeratingFiles)
        {
            EXPECT_FALSE(scanProgress.totalFilesCount.has_value());
            enumerationCounts.append(scanProgress.processedFilesCount);
        }
        else if (*phase == FileNameScanPhase::GroupingFilesByName)
        {
            ASSERT_TRUE(scanProgress.totalFilesCount.has_value());
            EXPECT_EQ(*scanProgress.totalFilesCount, expectedFilesCount);
            EXPECT_GE(scanProgress.processedFilesCount, lastGroupedFilesCount);
            EXPECT_LE(scanProgress.processedFilesCount, expectedFilesCount);
            lastGroupedFilesCount = scanProgress.processedFilesCount;
        }
        else if (*phase == FileNameScanPhase::BuildingScanResult)
        {
            EXPECT_EQ(scanProgress.processedFilesCount, expectedFilesCount);
            EXPECT_EQ(scanProgress.totalFilesCount, expectedFilesCount);
            buildingResultPhaseWasReported = true;
        }
    }

    EXPECT_EQ(enumerationCounts, (QList<quint64>{0, 1, 2, 3, 4, 5}));
    EXPECT_EQ(lastGroupedFilesCount, expectedFilesCount);
    EXPECT_TRUE(buildingResultPhaseWasReported);
}

/// Verifies the exact file, directory, byte, and duplicate metrics reported for a controlled nested directory tree.
TEST_F(ScanByFileNameTest, CheckSummaryMetrics_ControlledDirectoryTree)
{
    const QByteArray duplicateContents("same");
    const QByteArray uniqueContents("unique");

    ASSERT_TRUE(writeFile("first/shared.txt", duplicateContents));
    ASSERT_TRUE(writeFile("second/shared.log", duplicateContents));
    ASSERT_TRUE(writeFile("unique.txt", uniqueContents));

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
                                                                 std::stop_source().get_token(),
                                                                 ignoreProgressCallback);

    ASSERT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    const auto* summary = std::get_if<FileNameScanSummary>(&scanResult.getScanSummaryDetails());
    ASSERT_NE(summary, nullptr);

    const quint64 expectedDuplicateBytes = static_cast<quint64>(duplicateContents.size()) * 2;
    const quint64 expectedTotalScannedBytes = expectedDuplicateBytes + static_cast<quint64>(uniqueContents.size());

    EXPECT_EQ(summary->getScannedDirectoriesCount(), 3);
    EXPECT_EQ(summary->getScannedFilesCount(), 3);
    EXPECT_EQ(summary->getTotalScannedBytes(), expectedTotalScannedBytes);
    EXPECT_EQ(summary->getDuplicateGroupsCount(), 1);
    EXPECT_EQ(summary->getTotalFilesInDuplicateGroupsCount(), 2);
    EXPECT_EQ(summary->getTotalBytesOccupiedByFilesInDuplicateGroups(), expectedDuplicateBytes);
}

/// Verifies that file-name summary metrics combine directory, file, byte, and duplicate totals from all roots.
/// The two roots and their nested and deep subdirectories make 4 scanned directories; their 4 files occupy 4 + 3 + 4 + 5 = 16 bytes.
/// shared.txt and shared.log form 1 duplicate group containing 2 files, whose two 4-byte contents occupy 8 bytes in total.
TEST_F(ScanByFileNameTest, CheckSummaryMetrics_MultipleRoots)
{
    ASSERT_TRUE(writeFile("first-root/nested/shared.txt", "same"));
    ASSERT_TRUE(writeFile("first-root/unique-one", "one"));
    ASSERT_TRUE(writeFile("second-root/shared.log", "same"));
    ASSERT_TRUE(writeFile("second-root/deep/unique-two", "12345"));

    const QStringList rootPaths{
        getTemporaryDirectoryPath("first-root"),
        getTemporaryDirectoryPath("second-root")
    };
    const ScanResult scanResult = FileNameScanWorkflow().execute(rootPaths,
                                                                 std::stop_source().get_token(),
                                                                 ignoreProgressCallback);

    ASSERT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    const auto* summary = std::get_if<FileNameScanSummary>(&scanResult.getScanSummaryDetails());
    ASSERT_NE(summary, nullptr);

    EXPECT_EQ(summary->getScannedDirectoriesCount(), 4);
    EXPECT_EQ(summary->getScannedFilesCount(), 4);
    EXPECT_EQ(summary->getTotalScannedBytes(), 16);
    EXPECT_EQ(summary->getDuplicateGroupsCount(), 1);
    EXPECT_EQ(summary->getTotalFilesInDuplicateGroupsCount(), 2);
    EXPECT_EQ(summary->getTotalBytesOccupiedByFilesInDuplicateGroups(), 8);
}

/// Verifies that an active file-name scan can be cancelled after it has started enumerating files.
TEST_F(ScanByFileNameTest, CheckCancelledOutcome_StopRequestedDuringFileEnumeration)
{
    ASSERT_TRUE(writeFile("first.txt", "first file contents"));
    ASSERT_TRUE(writeFile("second.txt", "second file contents"));

    std::stop_source stopSource;
    bool stopWasRequestedAfterFirstFile = false;

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
                                                                 stopSource.get_token(),
                                                                 [&stopSource, &stopWasRequestedAfterFirstFile](const ScanProgress& scanProgress)
                                                                 {
                                                                     const auto* phase = std::get_if<FileNameScanPhase>(&scanProgress.scanPhase);

                                                                     if (!stopWasRequestedAfterFirstFile
                                                                         && phase
                                                                         && *phase == FileNameScanPhase::EnumeratingFiles
                                                                         && scanProgress.processedFilesCount == 1)
                                                                     {
                                                                         stopWasRequestedAfterFirstFile = stopSource.request_stop();
                                                                     }
                                                                 });

    EXPECT_TRUE(stopWasRequestedAfterFirstFile);
    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Cancelled);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}
