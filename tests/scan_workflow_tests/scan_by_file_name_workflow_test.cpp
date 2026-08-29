#include "scan_workflow_test_fixture.h"

#include "file_name_scan_workflow.h"
#include "scan_summary/file_name_scan_summary.h"

#include <stop_token>
#include <variant>

namespace
{
    class ScanByFileNameTest : public ScanWorkflowTest
    {};
}

/// @brief Verifies filename-based duplicate grouping against the maintained smoke-test directory tree.
///
/// @par Test setup
/// Use the repository smoke-test tree and construct the expected three-file and two-file duplicate groups from
/// their known paths and LF-normalized sizes.
///
/// @par Procedure
/// Execute `FileNameScanWorkflow` over the smoke-test root and inspect its outcome, groups, and summary variant.
///
/// @par Expected results
/// The scan completes with exactly the two expected groups and stores a `FileNameScanSummary` in the result.
TEST_F(ScanByFileNameTest, CheckDuplicateGroups_SmokeTest)
{
    const QString scanRootPath = getSmokeTestScanRootPath();

    const DuplicateGroup expectedThreeFileDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "dir1/duplicate_three.txt", 51),
        createExpectedFileRecord(scanRootPath, "dir2/duplicate_three.txt", 90),
        createExpectedFileRecord(scanRootPath, "dir3/duplicate_three.txt", 51)
    });

    const DuplicateGroup expectedTwoFileDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "dir1/duplicate_two.txt", 49),
        createExpectedFileRecord(scanRootPath, "dir2/duplicate_two.txt", 50)
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

/// @brief Verifies that filename matching ignores the final extension.
///
/// @par Test setup
/// Create `report.txt` and `report.pdf` with different contents and sizes in separate directories, plus an
/// unrelated file, and construct the expected report group.
///
/// @par Procedure
/// Execute the filename-based workflow over the temporary root and compare the returned group with the expected
/// file records.
///
/// @par Expected results
/// The scan completes with exactly one duplicate group containing both report files; the unrelated file is not
/// grouped.
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

/// @brief Verifies that filename matching is case-insensitive on every operating system.
///
/// @par Test setup
/// Create `Report.txt` and `report.txt` in separate directories with different contents, plus an unrelated file,
/// and construct the expected two-file group.
///
/// @par Procedure
/// Execute the filename-based workflow and compare its only duplicate group with the expected records.
///
/// @par Expected results
/// The differently cased names form one duplicate group independently of host filesystem case rules, while the
/// unrelated file remains excluded.
TEST_F(ScanByFileNameTest, CheckDuplicateGroups_FileNamesDifferOnlyByLetterCase)
{
    ASSERT_TRUE(writeFile("first/Report.txt", "first"));
    ASSERT_TRUE(writeFile("second/report.txt", "second"));
    ASSERT_TRUE(writeFile("third/unrelated.txt", "unrelated"));

    const QString scanRootPath = getTemporaryScanRootPath();
    const DuplicateGroup expectedDuplicateGroup = createExpectedDuplicateGroup({createExpectedFileRecord(scanRootPath, "first/Report.txt", 5),
                                                                                createExpectedFileRecord(scanRootPath, "second/report.txt", 6)});

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{scanRootPath},
                                                                 std::stop_source().get_token(),
                                                                 ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedDuplicateGroup));
}

/// @brief Verifies matching of equal filenames that have no extension.
///
/// @par Test setup
/// Create two extensionless `LICENSE` files with different contents and sizes, plus an extensionless `README`,
/// and construct the expected LICENSE group.
///
/// @par Procedure
/// Execute the filename-based workflow and inspect its outcome and returned group.
///
/// @par Expected results
/// Exactly one duplicate group contains both LICENSE files, and README is excluded.
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

/// @brief Verifies that a leading dot is treated as part of a filename rather than as an extension separator.
///
/// @par Test setup
/// Create two `.bashrc` files and one `.profile` file in separate directories, then construct the expected
/// `.bashrc` group.
///
/// @par Procedure
/// Execute the filename-based workflow and compare its returned group with the expected hidden files.
///
/// @par Expected results
/// Both `.bashrc` files form the only duplicate group; `.profile` is not incorrectly merged into it.
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

/// @brief Verifies that matching removes only the suffix after the final dot.
///
/// @par Test setup
/// Create `archive.tar.gz`, `archive.tar.zip`, and `archive.zip` with different contents and construct the
/// expected group containing the two `archive.tar` variants.
///
/// @par Procedure
/// Execute the filename-based workflow and inspect the single duplicate group.
///
/// @par Expected results
/// The two `archive.tar.*` files are grouped after their final extensions are removed, while `archive.zip` keeps
/// the different comparison key `archive` and is excluded.
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

/// @brief Verifies the phase types and transition order reported by a successful filename-based scan.
///
/// @par Test setup
/// Use the smoke-test directory and collect every progress update emitted by `FileNameScanWorkflow`.
///
/// @par Procedure
/// Execute the workflow, verify that every update contains a filename-scan phase, and reduce the updates to
/// distinct phase transitions.
///
/// @par Expected results
/// The scan succeeds and reports only `EnumeratingFiles`, `GroupingFilesByName`, and `BuildingScanResult`, in
/// that order.
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

/// @brief Verifies progress counter semantics for a successful single-root filename-based scan.
///
/// @par Test setup
/// Create two matching files and one unique file, then collect all progress updates from the workflow.
///
/// @par Procedure
/// Execute the scan and inspect enumeration, grouping, and result-building counters independently for totals,
/// monotonicity, and bounds.
///
/// @par Expected results
/// Enumeration advances to three with an unknown total, grouping advances monotonically to three with a known
/// total, and the final result-building update reports `3/3`.
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

/// @brief Verifies that filename-scan progress remains cumulative across multiple roots.
///
/// @par Test setup
/// Create two files in the first root and three in the second, including one cross-root duplicate pair, and
/// collect all workflow progress updates.
///
/// @par Procedure
/// Execute the workflow over both roots and separately inspect enumeration counts, grouping counters, and the
/// final result-building update.
///
/// @par Expected results
/// Enumeration reports exactly `0, 1, 2, 3, 4, 5` without restarting at the second root, grouping reaches five
/// monotonically, and result building reports `5/5`.
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

/// @brief Verifies filename-scan summary metrics for a controlled nested directory tree.
///
/// @par Test setup
/// Create two matching files in separate child directories and one unique file in the root. Calculate the
/// expected total and duplicate byte counts from their contents.
///
/// @par Procedure
/// Execute the workflow, retrieve its `FileNameScanSummary`, and inspect all directory, file, byte, and duplicate
/// counters.
///
/// @par Expected results
/// The summary reports three directories, three files, the exact total bytes, one two-file duplicate group, and
/// the exact bytes occupied by that group.
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

/// @brief Verifies that filename-scan summary metrics aggregate all supplied roots.
///
/// @par Test setup
/// Create two roots containing four directories and four files in total. Include one cross-root pair with the
/// same base name; the files occupy 16 bytes overall and the duplicate pair occupies eight.
///
/// @par Procedure
/// Execute the workflow over both roots and inspect every count stored in `FileNameScanSummary`.
///
/// @par Expected results
/// The summary reports four directories, four files, 16 scanned bytes, and one two-file duplicate group occupying
/// eight bytes.
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

/// @brief Verifies cancellation during filename-scan file enumeration.
///
/// @par Test setup
/// Create two files and configure a progress callback to request stop after the first file is enumerated.
///
/// @par Procedure
/// Execute the workflow with the callback-controlled stop token and record whether the request was made.
///
/// @par Expected results
/// The callback requests cancellation during enumeration, the result outcome is `Cancelled`, and no duplicate
/// groups are returned.
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
