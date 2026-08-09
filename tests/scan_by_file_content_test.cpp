#include "scan_test_fixtures.h"

#include "file_content_scan_workflow.h"
#include "scan_summary/file_content_scan_summary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <optional>
#include <stop_token>
#include <variant>

/// Verifies that scanning the smoke-test file system scenario tree by file content returns files with identical content and reports the expected recoverable bytes.
TEST_F(ScanByFileContentTest, CheckDuplicateGroups_SmokeTest)
{
    const QString scanRootPath = getSmokeTestScanRootPath();

    const DuplicateGroup expectedDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "dir1/unique_1.txt", 47),
        createExpectedFileRecord(scanRootPath, "dir2/unique_2.log", 47)
    });

    const ScanResult scanResult = FileContentScanWorkflow().execute(scanRootPath, std::stop_source().get_token(), ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedDuplicateGroup));

    const auto* summary = std::get_if<FileContentScanSummary>(&scanResult.getScanSummaryDetails());
    ASSERT_NE(summary, nullptr);

    const quint64 expectedRecoverableBytes = static_cast<quint64>(QFileInfo(QDir(scanRootPath).filePath("dir1/unique_1.txt")).size());
    EXPECT_EQ(summary->getTotalAmountOfPotentiallyRecoverableBytes(), expectedRecoverableBytes);
}

/// Verifies that a successful content scan reports only the expected workflow phases and reports them in execution order.
TEST_F(ScanByFileContentTest, CheckProgressPhases_SuccessfulScan)
{
    QList<ScanProgress> progressUpdates;

    const ScanResult scanResult = FileContentScanWorkflow().execute(
        getSmokeTestScanRootPath(),
        std::stop_source().get_token(),
        [&progressUpdates](const ScanProgress& scanProgress)
        {
            progressUpdates.append(scanProgress);
        });

    ASSERT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    EXPECT_TRUE(containsOnlyPhasesFor<FileContentScanPhase>(progressUpdates));
    EXPECT_EQ(
        getPhaseTransitions<FileContentScanPhase>(progressUpdates),
        (QList<FileContentScanPhase>{
            FileContentScanPhase::EnumeratingFiles,
            FileContentScanPhase::IdentifyingEqualSizeCandidates,
            FileContentScanPhase::HashingDuplicateCandidateFiles,
            FileContentScanPhase::VerifyingMatchingHashCandidates,
            FileContentScanPhase::BuildingScanResult}));
}

/// Verifies that content-scan progress counters are monotonic within each phase and respect each phase's reported total.
TEST_F(ScanByFileContentTest, CheckProgressCounters_SuccessfulScan)
{
    ASSERT_TRUE(writeFile("first/duplicate.bin", "same"));
    ASSERT_TRUE(writeFile("second/duplicate.dat", "same"));
    ASSERT_TRUE(writeFile("unique.txt", "unique"));

    QList<ScanProgress> progressUpdates;
    const ScanResult scanResult = FileContentScanWorkflow().execute(
        getTemporaryScanRootPath(),
        std::stop_source().get_token(),
        [&progressUpdates](const ScanProgress& scanProgress)
        {
            progressUpdates.append(scanProgress);
        });

    ASSERT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);

    constexpr quint64 expectedFilesCount = 3;
    constexpr quint64 expectedHashCandidateFilesCount = 2;
    quint64 lastEnumeratedFilesCount = 0;
    quint64 lastSizeCheckedFilesCount = 0;
    quint64 lastHashedFilesCount = 0;
    quint64 lastVerifiedFilesCount = 0;
    bool buildingResultPhaseWasReported = false;

    for (const ScanProgress& scanProgress: progressUpdates)
    {
        const auto* phase = std::get_if<FileContentScanPhase>(&scanProgress.scanPhase);
        ASSERT_NE(phase, nullptr);

        if (*phase == FileContentScanPhase::EnumeratingFiles)
        {
            EXPECT_FALSE(scanProgress.totalFilesCount.has_value());
            EXPECT_GE(scanProgress.processedFilesCount, lastEnumeratedFilesCount);
            lastEnumeratedFilesCount = scanProgress.processedFilesCount;
        }
        else if (*phase == FileContentScanPhase::IdentifyingEqualSizeCandidates)
        {
            ASSERT_TRUE(scanProgress.totalFilesCount.has_value());
            EXPECT_EQ(*scanProgress.totalFilesCount, expectedFilesCount);
            EXPECT_GE(scanProgress.processedFilesCount, lastSizeCheckedFilesCount);
            EXPECT_LE(scanProgress.processedFilesCount, *scanProgress.totalFilesCount);
            lastSizeCheckedFilesCount = scanProgress.processedFilesCount;
        }
        else if (*phase == FileContentScanPhase::HashingDuplicateCandidateFiles)
        {
            ASSERT_TRUE(scanProgress.totalFilesCount.has_value());
            EXPECT_EQ(*scanProgress.totalFilesCount, expectedHashCandidateFilesCount);
            EXPECT_GE(scanProgress.processedFilesCount, lastHashedFilesCount);
            EXPECT_LE(scanProgress.processedFilesCount, *scanProgress.totalFilesCount);
            lastHashedFilesCount = scanProgress.processedFilesCount;
        }
        else if (*phase == FileContentScanPhase::VerifyingMatchingHashCandidates)
        {
            ASSERT_TRUE(scanProgress.totalFilesCount.has_value());
            EXPECT_EQ(*scanProgress.totalFilesCount, expectedHashCandidateFilesCount);
            EXPECT_GE(scanProgress.processedFilesCount, lastVerifiedFilesCount);
            EXPECT_LE(scanProgress.processedFilesCount, *scanProgress.totalFilesCount);
            lastVerifiedFilesCount = scanProgress.processedFilesCount;
        }
        else if (*phase == FileContentScanPhase::BuildingScanResult)
        {
            ASSERT_TRUE(scanProgress.totalFilesCount.has_value());
            EXPECT_EQ(scanProgress.processedFilesCount, expectedFilesCount);
            EXPECT_EQ(*scanProgress.totalFilesCount, expectedFilesCount);
            buildingResultPhaseWasReported = true;
        }
    }

    EXPECT_EQ(lastEnumeratedFilesCount, expectedFilesCount);
    EXPECT_EQ(lastSizeCheckedFilesCount, expectedFilesCount);
    EXPECT_EQ(lastHashedFilesCount, expectedHashCandidateFilesCount);
    EXPECT_EQ(lastVerifiedFilesCount, expectedHashCandidateFilesCount);
    EXPECT_TRUE(buildingResultPhaseWasReported);
}

/// Verifies the exact file, directory, byte, duplicate, and recoverable-byte metrics for a controlled directory tree.
TEST_F(ScanByFileContentTest, CheckSummaryMetrics_ControlledDirectoryTree)
{
    const QByteArray duplicateContents("same");
    const QByteArray uniqueContents("unique");

    ASSERT_TRUE(writeFile("first/shared.txt", duplicateContents));
    ASSERT_TRUE(writeFile("second/shared.log", duplicateContents));
    ASSERT_TRUE(writeFile("unique.txt", uniqueContents));

    const ScanResult scanResult = FileContentScanWorkflow().execute(
        getTemporaryScanRootPath(),
        std::stop_source().get_token(),
        ignoreProgressCallback);

    ASSERT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    const auto* summary = std::get_if<FileContentScanSummary>(&scanResult.getScanSummaryDetails());
    ASSERT_NE(summary, nullptr);

    const quint64 expectedDuplicateBytes = static_cast<quint64>(duplicateContents.size()) * 2;
    const quint64 expectedTotalScannedBytes = expectedDuplicateBytes + static_cast<quint64>(uniqueContents.size());

    EXPECT_EQ(summary->getScannedDirectoriesCount(), 3);
    EXPECT_EQ(summary->getScannedFilesCount(), 3);
    EXPECT_EQ(summary->getTotalScannedBytes(), expectedTotalScannedBytes);
    EXPECT_EQ(summary->getDuplicateGroupsCount(), 1);
    EXPECT_EQ(summary->getTotalFilesInDuplicateGroupsCount(), 2);
    EXPECT_EQ(summary->getTotalBytesOccupiedByFilesInDuplicateGroups(), expectedDuplicateBytes);
    EXPECT_EQ(summary->getTotalAmountOfPotentiallyRecoverableBytes(), static_cast<quint64>(duplicateContents.size()));
}

/// Verifies that scanning an empty directory produces a no-files-found outcome.
TEST_F(ScanByFileContentTest, CheckNoFilesOutcome_EmptyDirectory)
{
    const ScanResult scanResult = FileContentScanWorkflow().execute(
        getTemporaryScanRootPath(),
        std::stop_source().get_token(),
        ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::NoFilesFound);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that a file-content scan returns a cancelled outcome when stopping is requested before the scan begins.
TEST_F(ScanByFileContentTest, CheckCancelledOutcome_StopRequestedBeforeScan)
{
    std::stop_source stopSource;
    stopSource.request_stop();

    const ScanResult scanResult = FileContentScanWorkflow().execute(getTemporaryScanRootPath(), stopSource.get_token(), ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Cancelled);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that scanning a nonexistent root directory produces a failed outcome.
TEST_F(ScanByFileContentTest, CheckFailedOutcome_RootDirectoryDoesNotExist)
{
    const ScanResult scanResult = FileContentScanWorkflow().execute(
        QDir(getTemporaryScanRootPath()).filePath("missing"),
        std::stop_source().get_token(),
        ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Failed);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that scanning one unique file completes successfully without duplicate groups.
TEST_F(ScanByFileContentTest, CheckCompletedWithoutDuplicatesOutcome_DirectoryContainsOneUniqueFile)
{
    ASSERT_TRUE(writeFile("only-file", "unique"));

    const ScanResult scanResult = FileContentScanWorkflow().execute(
        getTemporaryScanRootPath(),
        std::stop_source().get_token(),
        ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithoutDuplicates);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that an active content scan can be cancelled after it has started enumerating files.
TEST_F(ScanByFileContentTest, CheckCancelledOutcome_StopRequestedDuringFileEnumeration)
{
    ASSERT_TRUE(writeFile("first.txt", "first file contents"));
    ASSERT_TRUE(writeFile("second.txt", "second file contents"));

    std::stop_source stopSource;
    bool stopWasRequestedAfterFirstFile = false;

    const ScanResult scanResult = FileContentScanWorkflow().execute(
        getTemporaryScanRootPath(),
        stopSource.get_token(),
        [&stopSource, &stopWasRequestedAfterFirstFile](const ScanProgress& scanProgress)
        {
            const auto* phase = std::get_if<FileContentScanPhase>(&scanProgress.scanPhase);

            if (!stopWasRequestedAfterFirstFile
                && phase
                && *phase == FileContentScanPhase::EnumeratingFiles
                && scanProgress.processedFilesCount == 1)
            {
                stopWasRequestedAfterFirstFile = stopSource.request_stop();
            }
        });

    EXPECT_TRUE(stopWasRequestedAfterFirstFile);
    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Cancelled);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that an active content scan can be cancelled after hashing has begun.
TEST_F(ScanByFileContentTest, CheckCancelledOutcome_StopRequestedDuringHashing)
{
    ASSERT_TRUE(writeFile("one", "same"));
    ASSERT_TRUE(writeFile("two", "same"));

    std::stop_source stopSource;
    bool cancellationRequestedDuringHashing = false;
    const ScanResult scanResult = FileContentScanWorkflow().execute(
        getTemporaryScanRootPath(),
        stopSource.get_token(),
        [&stopSource, &cancellationRequestedDuringHashing](const ScanProgress& scanProgress)
        {
            const auto* phase = std::get_if<FileContentScanPhase>(&scanProgress.scanPhase);

            if (phase
                && *phase == FileContentScanPhase::HashingDuplicateCandidateFiles
                && scanProgress.processedFilesCount == 1)
            {
                cancellationRequestedDuringHashing = stopSource.request_stop();
            }
        });

    EXPECT_TRUE(cancellationRequestedDuringHashing);
    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Cancelled);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that three identical files form one group and that keeping one copy makes the other two files recoverable.
TEST_F(ScanByFileContentTest, CheckDuplicateGroups_ThreeIdenticalFiles)
{
    const QByteArray duplicateContents("duplicate");

    ASSERT_TRUE(writeFile("first/file.bin", duplicateContents));
    ASSERT_TRUE(writeFile("second/file.dat", duplicateContents));
    ASSERT_TRUE(writeFile("third/file.txt", duplicateContents));

    const QString scanRootPath = getTemporaryScanRootPath();
    const DuplicateGroup expectedDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "first/file.bin", duplicateContents.size()),
        createExpectedFileRecord(scanRootPath, "second/file.dat", duplicateContents.size()),
        createExpectedFileRecord(scanRootPath, "third/file.txt", duplicateContents.size())
    });

    const ScanResult scanResult = FileContentScanWorkflow().execute(scanRootPath, std::stop_source().get_token(), ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedDuplicateGroup));

    const auto* summary = std::get_if<FileContentScanSummary>(&scanResult.getScanSummaryDetails());
    ASSERT_NE(summary, nullptr);

    const auto duplicateFileSize = static_cast<quint64>(duplicateContents.size());
    EXPECT_EQ(summary->getTotalFilesInDuplicateGroupsCount(), 3);
    EXPECT_EQ(summary->getTotalBytesOccupiedByFilesInDuplicateGroups(), duplicateFileSize * 3);
    EXPECT_EQ(summary->getTotalAmountOfPotentiallyRecoverableBytes(), duplicateFileSize * 2);
}

/// Verifies correct grouping and summary metrics for large binary duplicates, empty files, and same-size files with different content.
TEST_F(ScanByFileContentTest, CheckBinaryAndEmptyDuplicateGroups_LargeAndEmptyFiles)
{
    QByteArray duplicateBytes(1024 * 1024 + 31, 'A');
    duplicateBytes[1024 * 1024] = '\0';
    QByteArray sameSizeDifferentBytes(duplicateBytes.size(), 'B');
    sameSizeDifferentBytes[1024 * 1024] = '\0';

    ASSERT_TRUE(writeFile("a/first.bin", duplicateBytes));
    ASSERT_TRUE(writeFile("b/second.dat", duplicateBytes));
    ASSERT_TRUE(writeFile("c/not-a-duplicate.bin", sameSizeDifferentBytes));
    ASSERT_TRUE(writeFile("d/only-one-size.txt", "a unique length"));
    ASSERT_TRUE(writeFile("e/empty-one", {}));
    ASSERT_TRUE(writeFile("f/empty-two", {}));

    const QString scanRootPath = getTemporaryScanRootPath();

    const DuplicateGroup expectedBinaryDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "a/first.bin", duplicateBytes.size()),
        createExpectedFileRecord(scanRootPath, "b/second.dat", duplicateBytes.size())
    });

    const DuplicateGroup expectedEmptyFileDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "e/empty-one", 0),
        createExpectedFileRecord(scanRootPath, "f/empty-two", 0)
    });

    const DuplicateGroup unexpectedMixedContentGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "a/first.bin", duplicateBytes.size()),
        createExpectedFileRecord(scanRootPath, "c/not-a-duplicate.bin", sameSizeDifferentBytes.size()),
        createExpectedFileRecord(scanRootPath, "b/second.dat", duplicateBytes.size())
    });

    const ScanResult scanResult = FileContentScanWorkflow().execute(scanRootPath, std::stop_source().get_token(), ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 2);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedBinaryDuplicateGroup));
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedEmptyFileDuplicateGroup));
    EXPECT_FALSE(containsDuplicateGroup(scanResult, unexpectedMixedContentGroup));

    const auto* summary = std::get_if<FileContentScanSummary>(&scanResult.getScanSummaryDetails());
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->getDuplicateGroupsCount(), 2);
    EXPECT_EQ(summary->getTotalFilesInDuplicateGroupsCount(), 4);
    EXPECT_EQ(summary->getTotalBytesOccupiedByFilesInDuplicateGroups(), static_cast<quint64>(duplicateBytes.size()) * 2);
    EXPECT_EQ(summary->getTotalAmountOfPotentiallyRecoverableBytes(), static_cast<quint64>(duplicateBytes.size()));
}

/// Verifies that equal-size files are separated by content hash while files with unique sizes are excluded from hashing.
TEST_F(ScanByFileContentTest, CheckHashGroupingAndSizePruning_RepeatedAndUniqueSizeBuckets)
{
    ASSERT_TRUE(writeFile("a/a-one", "AAAA"));
    ASSERT_TRUE(writeFile("b/a-two", "AAAA"));
    ASSERT_TRUE(writeFile("c/b-one", "BBBB"));
    ASSERT_TRUE(writeFile("d/b-two", "BBBB"));
    ASSERT_TRUE(writeFile("e/c-only", "CCCC"));
    ASSERT_TRUE(writeFile("f/unique-size", "1234567"));

    const QString scanRootPath = getTemporaryScanRootPath();

    const DuplicateGroup expectedFirstDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "a/a-one", 4),
        createExpectedFileRecord(scanRootPath, "b/a-two", 4)
    });

    const DuplicateGroup expectedSecondDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "c/b-one", 4),
        createExpectedFileRecord(scanRootPath, "d/b-two", 4)
    });

    const DuplicateGroup unexpectedSingleFileGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "e/c-only", 4)
    });

    QList<ScanProgress> progressUpdates;
    const ScanResult scanResult = FileContentScanWorkflow().execute(
        scanRootPath,
        std::stop_source().get_token(),
        [&progressUpdates](const ScanProgress& scanProgress)
        {
            progressUpdates.append(scanProgress);
        });

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 2);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedFirstDuplicateGroup));
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedSecondDuplicateGroup));
    EXPECT_FALSE(containsDuplicateGroup(scanResult, unexpectedSingleFileGroup));

    quint64 lastHashedFilesCount = 0;
    std::optional<quint64> hashingTotal;

    for (const ScanProgress& scanProgress: progressUpdates)
    {
        const auto* phase = std::get_if<FileContentScanPhase>(&scanProgress.scanPhase);

        if (phase && *phase == FileContentScanPhase::HashingDuplicateCandidateFiles)
        {
            lastHashedFilesCount = scanProgress.processedFilesCount;
            hashingTotal = scanProgress.totalFilesCount;
        }
    }

    ASSERT_TRUE(hashingTotal.has_value());
    EXPECT_EQ(*hashingTotal, 5);
    EXPECT_EQ(lastHashedFilesCount, 5);

    const auto* summary = std::get_if<FileContentScanSummary>(&scanResult.getScanSummaryDetails());
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->getTotalAmountOfPotentiallyRecoverableBytes(), 8);
}

/// Verifies that one matching-hash bucket is split into exact-content groups instead of treating a hash collision as proof that all files are duplicates.
TEST_F(ScanByFileContentTest, CheckHashCollision_MatchingHashBucketIsPartitionedByExactContent)
{
    ASSERT_TRUE(writeFile("a/first-a", "AAAA"));
    ASSERT_TRUE(writeFile("b/second-a", "AAAA"));
    ASSERT_TRUE(writeFile("c/first-b", "BBBB"));
    ASSERT_TRUE(writeFile("d/second-b", "BBBB"));
    ASSERT_TRUE(writeFile("e/collision-only", "CCCC"));

    const QString scanRootPath = getTemporaryScanRootPath();

    const DuplicateGroup expectedFirstDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "a/first-a", 4),
        createExpectedFileRecord(scanRootPath, "b/second-a", 4)
    });

    const DuplicateGroup expectedSecondDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "c/first-b", 4),
        createExpectedFileRecord(scanRootPath, "d/second-b", 4)
    });

    const FileContentScanWorkflow scanWorkflow(
        [](const FileRecord&, const std::stop_token&) -> std::optional<QByteArray>
        {
            // Returning one hash for every file simulates a cryptographic hash collision deterministically.
            return QByteArray("forced-hash-collision");
        });

    const ScanResult scanResult = scanWorkflow.execute(
        scanRootPath,
        std::stop_source().get_token(),
        ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 2);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedFirstDuplicateGroup));
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedSecondDuplicateGroup));

    const auto* summary = std::get_if<FileContentScanSummary>(&scanResult.getScanSummaryDetails());
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->getDuplicateGroupsCount(), 2);
    EXPECT_EQ(summary->getTotalFilesInDuplicateGroupsCount(), 4);
    EXPECT_EQ(summary->getTotalAmountOfPotentiallyRecoverableBytes(), 8);
}

/// Verifies that losing a collected hash candidate before it is read causes the content scan to fail.
TEST_F(ScanByFileContentTest, CheckFailedOutcome_CollectedCandidateRemovedBeforeHashing)
{
    ASSERT_TRUE(writeFile("one", "same"));
    ASSERT_TRUE(writeFile("two", "same"));

    const QString fileToRemove = QDir(getTemporaryScanRootPath()).filePath("one");
    bool removedHashCandidate = false;
    const ScanResult scanResult = FileContentScanWorkflow().execute(
        getTemporaryScanRootPath(),
        std::stop_source().get_token(),
        [&fileToRemove, &removedHashCandidate](const ScanProgress& scanProgress)
        {
            const auto* phase = std::get_if<FileContentScanPhase>(&scanProgress.scanPhase);

            if (!removedHashCandidate
                && phase
                && *phase == FileContentScanPhase::HashingDuplicateCandidateFiles
                && scanProgress.processedFilesCount == 0)
            {
                removedHashCandidate = QFile::remove(fileToRemove);
            }
        });

    EXPECT_TRUE(removedHashCandidate);
    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Failed);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// Verifies that losing a matching-hash candidate after hashing but before byte comparison causes the content scan to fail rather than report an unverified duplicate.
TEST_F(ScanByFileContentTest, CheckFailedOutcome_HashCandidateRemovedBeforeByteComparison)
{
    ASSERT_TRUE(writeFile("one", "same"));
    ASSERT_TRUE(writeFile("two", "same"));

    const QString fileToRemove = QDir(getTemporaryScanRootPath()).filePath("one");
    bool removedComparisonCandidate = false;
    const ScanResult scanResult = FileContentScanWorkflow().execute(
        getTemporaryScanRootPath(),
        std::stop_source().get_token(),
        [&fileToRemove, &removedComparisonCandidate](const ScanProgress& scanProgress)
        {
            const auto* phase = std::get_if<FileContentScanPhase>(&scanProgress.scanPhase);

            if (!removedComparisonCandidate
                && phase
                && *phase == FileContentScanPhase::VerifyingMatchingHashCandidates
                && scanProgress.processedFilesCount == 0)
            {
                // Both hashes have already been calculated; removal now exercises comparison failure.
                removedComparisonCandidate = QFile::remove(fileToRemove);
            }
        });

    EXPECT_TRUE(removedComparisonCandidate);
    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Failed);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}
