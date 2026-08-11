#include "scan_test_fixtures.h"

#include "file_name_scan_workflow.h"
#include "scan_summary/file_name_scan_summary.h"

#include <stop_token>
#include <variant>

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

/// Verifies that scanning an empty directory produces a no-files-found outcome.
TEST_F(ScanByFileNameTest, CheckNoFilesOutcome_EmptyDirectory)
{
    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
                                                                 std::stop_source().get_token(),
                                                                 ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::NoFilesFound);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that a file-name scan returns a cancelled outcome when stopping is requested before the scan begins.
TEST_F(ScanByFileNameTest, CheckCancelledOutcome_StopRequestedBeforeScan)
{
    std::stop_source stopSource;
    stopSource.request_stop();

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
                                                                 stopSource.get_token(),
                                                                 ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Cancelled);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that scanning a nonexistent root directory produces a failed outcome.
TEST_F(ScanByFileNameTest, CheckFailedOutcome_RootDirectoryDoesNotExist)
{
    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{QDir(getTemporaryScanRootPath()).filePath("missing")},
                                                                 std::stop_source().get_token(),
                                                                 ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Failed);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that scanning one unique file completes successfully without duplicate groups.
TEST_F(ScanByFileNameTest, CheckCompletedWithoutDuplicatesOutcome_DirectoryContainsOneUniqueFile)
{
    ASSERT_TRUE(writeFile("only-file", "unique"));

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
                                                                 std::stop_source().get_token(),
                                                                 ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithoutDuplicates);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that an active file-name scan can be cancelled after it has started enumerating files.
TEST_F(ScanByFileNameTest, CheckCancelledOutcome_StopRequestedDuringFileEnumeration)
{
    // Give the workflow enough input to enter file enumeration and still have work remaining after reporting the first file.
    ASSERT_TRUE(writeFile("first.txt", "first file contents"));
    ASSERT_TRUE(writeFile("second.txt", "second file contents"));

    std::stop_source stopSource;
    // This flag prevents repeated requests and later proves that cancellation was requested from inside the active scan.
    bool stopWasRequestedAfterFirstFile = false;

    const ScanResult scanResult = FileNameScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
                                                                 stopSource.get_token(),
                                                                 [&stopSource, &stopWasRequestedAfterFirstFile](const ScanProgress& scanProgress)
                                                                 {
                                                                     const auto* fileNameScanPhase = std::get_if<FileNameScanPhase>(&scanProgress.scanPhase);

                                                                     // Waiting for the first processed file distinguishes in-progress cancellation from cancellation before execution.
                                                                     if (!stopWasRequestedAfterFirstFile && fileNameScanPhase && *fileNameScanPhase == FileNameScanPhase::EnumeratingFiles && scanProgress.processedFilesCount == 1)
                                                                     {
                                                                         stopWasRequestedAfterFirstFile = stopSource.request_stop();
                                                                     }
                                                                 });

    // Check both sides of the interaction: the callback issued the request and the workflow honored it.
    EXPECT_TRUE(stopWasRequestedAfterFirstFile);
    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Cancelled);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}
