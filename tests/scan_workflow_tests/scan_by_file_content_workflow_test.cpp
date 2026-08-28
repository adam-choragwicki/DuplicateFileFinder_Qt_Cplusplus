#include "scan_workflow_test_fixture.h"

#include "file_content_scan_workflow.h"
#include "file_hasher.h"
#include "scan_summary/file_content_scan_summary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <limits>
#include <optional>
#include <stop_token>
#include <utility>
#include <variant>

namespace
{
    class ScanByFileContentTest : public ScanWorkflowTest
    {
    protected:
        static constexpr qint64 forceLargeFileSamplingThresholdBytes = 1;
        static constexpr qsizetype sampledTestFileSizeBytes = 4 * 1024 * 1024;

        static FileContentScanWorkflow createScanWorkflow(
            FileContentScanWorkflow::FileHashCalculator fileHashCalculator,
            FileContentScanWorkflow::FileSampleHashCalculator fileSampleHashCalculator = &FileHasher::calculateFileSampleHash,
            const qint64 fileSamplingThresholdBytes = std::numeric_limits<qint64>::max())
        {
            return FileContentScanWorkflow(FileContentScanWorkflow::HashingConfiguration{
                std::move(fileHashCalculator),
                std::move(fileSampleHashCalculator),
                fileSamplingThresholdBytes
            });
        }
    };
}

/// @brief Verifies file-content-based duplicate detection against the maintained smoke-test directory tree.
///
/// @par Test setup
/// Use the repository smoke-test tree and construct the expected group from the two known files with identical
/// contents but different names and extensions.
///
/// @par Procedure
/// Execute `FileContentScanWorkflow`, compare the returned group with the expected records, and inspect the
/// file-content-based scan summary.
///
/// @par Expected results
/// The scan returns exactly one duplicate group and reports recoverable bytes equal to the size of one retained
/// duplicate copy.
TEST_F(ScanByFileContentTest, CheckDuplicateGroups_SmokeTest)
{
    const QString scanRootPath = getSmokeTestScanRootPath();

    const DuplicateGroup expectedDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "dir1/unique_1.txt", 47),
        createExpectedFileRecord(scanRootPath, "dir2/unique_2.log", 47)
    });

    const ScanResult scanResult = FileContentScanWorkflow().execute(QStringList{scanRootPath},
                                                                    std::stop_source().get_token(),
                                                                    ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedDuplicateGroup));

    const auto* summary = std::get_if<FileContentScanSummary>(&scanResult.getScanSummaryDetails());
    ASSERT_NE(summary, nullptr);

    const quint64 expectedRecoverableBytes = static_cast<quint64>(QFileInfo(QDir(scanRootPath).filePath("dir1/unique_1.txt")).size());
    EXPECT_EQ(summary->getTotalAmountOfPotentiallyRecoverableBytes(), expectedRecoverableBytes);
}

/// @brief Verifies the phase types and transition order reported by a successful file-content-based scan.
///
/// @par Test setup
/// Use the smoke-test directory and collect every progress update emitted by `FileContentScanWorkflow`.
///
/// @par Procedure
/// Execute the workflow, verify that every update contains a file-content phase, and reduce the updates to
/// distinct phase transitions.
///
/// @par Expected results
/// The phases appear only in this order: enumeration, equal-size candidate identification, hashing, exact-content
/// verification, and result building.
TEST_F(ScanByFileContentTest, CheckProgressPhases_SuccessfulScan)
{
    QList<ScanProgress> progressUpdates;

    const ScanResult scanResult = FileContentScanWorkflow().execute(QStringList{getSmokeTestScanRootPath()},
                                                                    std::stop_source().get_token(),
                                                                    [&progressUpdates](const ScanProgress& scanProgress)
                                                                    {
                                                                        progressUpdates.append(scanProgress);
                                                                    });

    ASSERT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    EXPECT_TRUE(containsOnlyPhasesFor<FileContentScanPhase>(progressUpdates));
    EXPECT_EQ(getPhaseTransitions<FileContentScanPhase>(progressUpdates), (QList<FileContentScanPhase>{
                  FileContentScanPhase::EnumeratingFiles,
                  FileContentScanPhase::IdentifyingEqualSizeCandidates,
                  FileContentScanPhase::HashingDuplicateCandidateFiles,
                  FileContentScanPhase::VerifyingMatchingHashCandidates,
                  FileContentScanPhase::BuildingScanResult}));
}

/// @brief Verifies progress counter semantics for a successful single-root file-content-based scan.
///
/// @par Test setup
/// Create two identical four-byte files and one unique-size file, then collect every workflow progress update.
///
/// @par Procedure
/// Execute the scan and inspect counters independently for enumeration, size checking, hashing, content
/// verification, and result building.
///
/// @par Expected results
/// Enumeration and size checking reach all three files; only the two equal-size candidates are hashed and
/// verified; every phase is monotonic and bounded; result building reports `3/3`.
TEST_F(ScanByFileContentTest, CheckProgressCounters_SuccessfulScan)
{
    ASSERT_TRUE(writeFile("first/duplicate.bin", "same"));
    ASSERT_TRUE(writeFile("second/duplicate.dat", "same"));
    ASSERT_TRUE(writeFile("unique.txt", "unique"));

    QList<ScanProgress> progressUpdates;
    const ScanResult scanResult = FileContentScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
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

/// @brief Verifies that file-content-based scan progress remains cumulative across multiple roots.
///
/// @par Test setup
/// Create two files in the first root and three in the second. Only one cross-root pair has equal size and
/// identical contents; collect all workflow progress updates.
///
/// @par Procedure
/// Execute the workflow over both roots and inspect the enumeration, size-checking, hashing, verification, and
/// result-building counters.
///
/// @par Expected results
/// Enumeration reports `0` through `5` without restarting, size checking reaches five, hashing and verification
/// each reach the two candidates, all counters are monotonic, and result building reports `5/5`.
TEST_F(ScanByFileContentTest, CheckProgressCounters_MultipleRoots)
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
    const ScanResult scanResult = FileContentScanWorkflow().execute(rootPaths,
                                                                    std::stop_source().get_token(),
                                                                    [&progressUpdates](const ScanProgress& scanProgress)
                                                                    {
                                                                        progressUpdates.append(scanProgress);
                                                                    });

    ASSERT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);

    constexpr quint64 expectedFilesCount = 5;
    constexpr quint64 expectedCandidateFilesCount = 2;
    QList<quint64> enumerationCounts;
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
            enumerationCounts.append(scanProgress.processedFilesCount);
        }
        else if (*phase == FileContentScanPhase::IdentifyingEqualSizeCandidates)
        {
            ASSERT_TRUE(scanProgress.totalFilesCount.has_value());
            EXPECT_EQ(*scanProgress.totalFilesCount, expectedFilesCount);
            EXPECT_GE(scanProgress.processedFilesCount, lastSizeCheckedFilesCount);
            EXPECT_LE(scanProgress.processedFilesCount, expectedFilesCount);
            lastSizeCheckedFilesCount = scanProgress.processedFilesCount;
        }
        else if (*phase == FileContentScanPhase::HashingDuplicateCandidateFiles)
        {
            ASSERT_TRUE(scanProgress.totalFilesCount.has_value());
            EXPECT_EQ(*scanProgress.totalFilesCount, expectedCandidateFilesCount);
            EXPECT_GE(scanProgress.processedFilesCount, lastHashedFilesCount);
            EXPECT_LE(scanProgress.processedFilesCount, expectedCandidateFilesCount);
            lastHashedFilesCount = scanProgress.processedFilesCount;
        }
        else if (*phase == FileContentScanPhase::VerifyingMatchingHashCandidates)
        {
            ASSERT_TRUE(scanProgress.totalFilesCount.has_value());
            EXPECT_EQ(*scanProgress.totalFilesCount, expectedCandidateFilesCount);
            EXPECT_GE(scanProgress.processedFilesCount, lastVerifiedFilesCount);
            EXPECT_LE(scanProgress.processedFilesCount, expectedCandidateFilesCount);
            lastVerifiedFilesCount = scanProgress.processedFilesCount;
        }
        else if (*phase == FileContentScanPhase::BuildingScanResult)
        {
            EXPECT_EQ(scanProgress.processedFilesCount, expectedFilesCount);
            EXPECT_EQ(scanProgress.totalFilesCount, expectedFilesCount);
            buildingResultPhaseWasReported = true;
        }
    }

    EXPECT_EQ(enumerationCounts, (QList<quint64>{0, 1, 2, 3, 4, 5}));
    EXPECT_EQ(lastSizeCheckedFilesCount, expectedFilesCount);
    EXPECT_EQ(lastHashedFilesCount, expectedCandidateFilesCount);
    EXPECT_EQ(lastVerifiedFilesCount, expectedCandidateFilesCount);
    EXPECT_TRUE(buildingResultPhaseWasReported);
}

/// @brief Verifies file-content-based scan summary metrics for a controlled nested directory tree.
///
/// @par Test setup
/// Create two identical files in separate child directories and one unique file in the root. Calculate expected
/// total, duplicate, and recoverable byte counts from their contents.
///
/// @par Procedure
/// Execute the workflow, retrieve `FileContentScanSummary`, and inspect all directory, file, byte, duplicate, and
/// recoverable-space counters.
///
/// @par Expected results
/// The summary reports three directories, three files, the exact total bytes, one two-file duplicate group, its
/// occupied bytes, and one duplicate file's size as potentially recoverable.
TEST_F(ScanByFileContentTest, CheckSummaryMetrics_ControlledDirectoryTree)
{
    const QByteArray duplicateContents("same");
    const QByteArray uniqueContents("unique");

    ASSERT_TRUE(writeFile("first/shared.txt", duplicateContents));
    ASSERT_TRUE(writeFile("second/shared.log", duplicateContents));
    ASSERT_TRUE(writeFile("unique.txt", uniqueContents));

    const ScanResult scanResult = FileContentScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
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

/// @brief Verifies that file-content-based scan summary metrics aggregate all supplied roots.
///
/// @par Test setup
/// Create two roots containing four directories and four files in total. Include one identical cross-root pair;
/// all files occupy 16 bytes, the pair occupies eight, and removing one copy would recover four.
///
/// @par Procedure
/// Execute the workflow over both roots and inspect every count stored in `FileContentScanSummary`.
///
/// @par Expected results
/// The summary reports four directories, four files, 16 scanned bytes, one two-file duplicate group occupying
/// eight bytes, and four potentially recoverable bytes.
TEST_F(ScanByFileContentTest, CheckSummaryMetrics_MultipleRoots)
{
    ASSERT_TRUE(writeFile("first-root/nested/shared.txt", "same"));
    ASSERT_TRUE(writeFile("first-root/unique-one", "one"));
    ASSERT_TRUE(writeFile("second-root/shared.log", "same"));
    ASSERT_TRUE(writeFile("second-root/deep/unique-two", "12345"));

    const QStringList rootPaths{
        getTemporaryDirectoryPath("first-root"),
        getTemporaryDirectoryPath("second-root")
    };
    const ScanResult scanResult = FileContentScanWorkflow().execute(rootPaths,
                                                                    std::stop_source().get_token(),
                                                                    ignoreProgressCallback);

    ASSERT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    const auto* summary = std::get_if<FileContentScanSummary>(&scanResult.getScanSummaryDetails());
    ASSERT_NE(summary, nullptr);

    EXPECT_EQ(summary->getScannedDirectoriesCount(), 4);
    EXPECT_EQ(summary->getScannedFilesCount(), 4);
    EXPECT_EQ(summary->getTotalScannedBytes(), 16);
    EXPECT_EQ(summary->getDuplicateGroupsCount(), 1);
    EXPECT_EQ(summary->getTotalFilesInDuplicateGroupsCount(), 2);
    EXPECT_EQ(summary->getTotalBytesOccupiedByFilesInDuplicateGroups(), 8);
    EXPECT_EQ(summary->getTotalAmountOfPotentiallyRecoverableBytes(), 4);
}

/// @brief Verifies cancellation during file enumeration in a file-content-based scan.
///
/// @par Test setup
/// Create two files and configure a progress callback to request stop after the first file is enumerated.
///
/// @par Procedure
/// Execute the workflow with the callback-controlled stop token and record whether cancellation was requested.
///
/// @par Expected results
/// Cancellation is requested during enumeration, the result outcome is `Cancelled`, and no duplicate groups are
/// returned.
TEST_F(ScanByFileContentTest, CheckCancelledOutcome_StopRequestedDuringFileEnumeration)
{
    ASSERT_TRUE(writeFile("first.txt", "first file contents"));
    ASSERT_TRUE(writeFile("second.txt", "second file contents"));

    std::stop_source stopSource;
    bool stopWasRequestedAfterFirstFile = false;

    const ScanResult scanResult = FileContentScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
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

/// @brief Verifies cancellation during candidate hashing in a file-content-based scan.
///
/// @par Test setup
/// Create two identical files and configure a progress callback to request stop after the first candidate is
/// hashed.
///
/// @par Procedure
/// Execute the workflow with the callback-controlled stop token and record whether cancellation occurred in the
/// hashing phase.
///
/// @par Expected results
/// The stop request is made after hashing begins, the outcome is `Cancelled`, and no duplicate group is returned.
TEST_F(ScanByFileContentTest, CheckCancelledOutcome_StopRequestedDuringHashing)
{
    ASSERT_TRUE(writeFile("one", "same"));
    ASSERT_TRUE(writeFile("two", "same"));

    std::stop_source stopSource;
    bool cancellationRequestedDuringHashing = false;
    const ScanResult scanResult = FileContentScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
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

/// @brief Verifies grouping and recoverable-space accounting for three identical files.
///
/// @par Test setup
/// Write the same byte sequence to three differently named files and construct the expected three-file group.
///
/// @par Procedure
/// Execute the file-content-based workflow, compare the group with the expected records, and inspect its summary
/// metrics.
///
/// @par Expected results
/// One group contains all three files, occupied duplicate bytes equal three file sizes, and recoverable bytes
/// equal two file sizes because one copy must be retained.
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

    const ScanResult scanResult = FileContentScanWorkflow().execute(QStringList{scanRootPath}, std::stop_source().get_token(), ignoreProgressCallback);

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

/// @brief Verifies grouping of large binary and empty files without treating equal size as proof of duplication.
///
/// @par Test setup
/// Create two identical binary files larger than one MiB, one same-size file with different bytes, one unique-size
/// file, and two empty files. Construct expected binary, empty, and invalid mixed-content groups.
///
/// @par Procedure
/// Execute the file-content-based workflow, compare its groups with the expected partitions, and inspect duplicate
/// and recoverable-byte summary metrics.
///
/// @par Expected results
/// Exactly the identical binary pair and empty pair are reported. The different-content file is excluded, four
/// files belong to duplicate groups, and only one large binary copy contributes recoverable bytes.
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

    const ScanResult scanResult = FileContentScanWorkflow().execute(QStringList{scanRootPath},
                                                                    std::stop_source().get_token(),
                                                                    ignoreProgressCallback);

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

/// @brief Verifies size pruning, hash partitioning, and recoverable-byte accounting.
///
/// @par Test setup
/// Create five four-byte files forming two duplicate pairs plus one same-size singleton, and one seven-byte
/// unique-size file. Collect workflow progress and construct the expected groups.
///
/// @par Procedure
/// Execute the workflow, inspect its groups, extract the final hashing counter and total, and inspect recoverable
/// bytes.
///
/// @par Expected results
/// Two exact-content groups are returned, the same-size singleton is excluded, all five four-byte candidates are
/// hashed, the unique-size file is pruned, and eight bytes are recoverable.
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
    const ScanResult scanResult = FileContentScanWorkflow().execute(QStringList{scanRootPath},
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

/// @brief Verifies that files below the configured size threshold bypass sampling.
///
/// @par Test setup
/// Create an identical four-byte pair, set the sampling threshold to five bytes, and count both sample and full
/// hash calculator calls.
///
/// @par Procedure
/// Execute the workflow and inspect its duplicate result and calculator call counts.
///
/// @par Expected results
/// Neither small file is sampled, both receive full hashes through the existing path, and the duplicate pair is
/// returned.
TEST_F(ScanByFileContentTest, BypassSampling_ForFilesBelowConfiguredThreshold)
{
    ASSERT_TRUE(writeFile("first.bin", "same"));
    ASSERT_TRUE(writeFile("second.bin", "same"));

    quint64 sampleHashCalculationCount = 0;
    quint64 fullHashCalculationCount = 0;
    const FileContentScanWorkflow scanWorkflow = createScanWorkflow(
        [&fullHashCalculationCount](const FileRecord& file, const std::stop_token& stopToken)
        {
            ++fullHashCalculationCount;
            return FileHasher::calculateFullFileHash(file, stopToken);
        },
        [&sampleHashCalculationCount](const FileRecord&, const std::stop_token&) -> std::optional<QByteArray>
        {
            ++sampleHashCalculationCount;
            return QByteArray("unexpected-sample-hash");
        },
        5);

    const ScanResult scanResult = scanWorkflow.execute(QStringList{getTemporaryScanRootPath()},
                                                       std::stop_source().get_token(),
                                                       ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_EQ(sampleHashCalculationCount, 0);
    EXPECT_EQ(fullHashCalculationCount, 2);
}

/// @brief Verifies that unique samples prevent unnecessary full hashing of large equal-size files.
///
/// @par Test setup
/// Create three large equal-size files whose sampled prefixes differ. Force the production sampling path at the
/// test files' size and wrap the full-hash calculator to count calls.
///
/// @par Procedure
/// Execute the workflow, inspect its result and full-hash call count, and verify final hashing-phase progress.
///
/// @par Expected results
/// The sample hashes rule out all three files, no full hash is calculated, no duplicates are reported, and all
/// three candidates are nevertheless reported as processed.
TEST_F(ScanByFileContentTest, PruneUniqueLargeFileSamples_WithoutCalculatingFullHashes)
{
    ASSERT_TRUE(writeFile("first.bin", QByteArray(sampledTestFileSizeBytes, 'A')));
    ASSERT_TRUE(writeFile("second.bin", QByteArray(sampledTestFileSizeBytes, 'B')));
    ASSERT_TRUE(writeFile("third.bin", QByteArray(sampledTestFileSizeBytes, 'C')));

    quint64 fullHashCalculationCount = 0;
    const FileContentScanWorkflow scanWorkflow = createScanWorkflow(
        [&fullHashCalculationCount](const FileRecord& file, const std::stop_token& stopToken)
        {
            ++fullHashCalculationCount;
            return FileHasher::calculateFullFileHash(file, stopToken);
        },
        &FileHasher::calculateFileSampleHash,
        forceLargeFileSamplingThresholdBytes);

    QList<ScanProgress> progressUpdates;
    const ScanResult scanResult = scanWorkflow.execute(QStringList{getTemporaryScanRootPath()},
                                                       std::stop_source().get_token(),
                                                       [&progressUpdates](const ScanProgress& scanProgress)
                                                       {
                                                           progressUpdates.append(scanProgress);
                                                       });

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithoutDuplicates);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
    EXPECT_EQ(fullHashCalculationCount, 0);

    const auto finalHashingProgress = std::find_if(
        progressUpdates.crbegin(),
        progressUpdates.crend(),
        [](const ScanProgress& scanProgress)
        {
            const auto* phase = std::get_if<FileContentScanPhase>(&scanProgress.scanPhase);
            return phase && *phase == FileContentScanPhase::HashingDuplicateCandidateFiles;
        });

    ASSERT_NE(finalHashingProgress, progressUpdates.crend());
    EXPECT_EQ(finalHashingProgress->processedFilesCount, 3);
    EXPECT_EQ(finalHashingProgress->totalFilesCount, 3);
}

/// @brief Verifies that matching large-file samples remain subject to full hashing and exact verification.
///
/// @par Test setup
/// Create two identical large files and one same-size file with a different sampled prefix. Count full-hash calls
/// while using the production sample calculator.
///
/// @par Procedure
/// Execute the workflow and compare the returned group with the identical pair.
///
/// @par Expected results
/// The unique sampled file is pruned, both matching-sample files receive full hashes, and the identical pair is
/// returned as the sole duplicate group.
TEST_F(ScanByFileContentTest, FullyVerifyLargeFiles_WithMatchingSamples)
{
    const QByteArray duplicateContents(sampledTestFileSizeBytes, 'A');
    ASSERT_TRUE(writeFile("first.bin", duplicateContents));
    ASSERT_TRUE(writeFile("second.bin", duplicateContents));
    ASSERT_TRUE(writeFile("different.bin", QByteArray(sampledTestFileSizeBytes, 'B')));

    quint64 fullHashCalculationCount = 0;
    const FileContentScanWorkflow scanWorkflow = createScanWorkflow(
        [&fullHashCalculationCount](const FileRecord& file, const std::stop_token& stopToken)
        {
            ++fullHashCalculationCount;
            return FileHasher::calculateFullFileHash(file, stopToken);
        },
        &FileHasher::calculateFileSampleHash,
        forceLargeFileSamplingThresholdBytes);

    const QString scanRootPath = getTemporaryScanRootPath();
    const ScanResult scanResult = scanWorkflow.execute(QStringList{scanRootPath},
                                                       std::stop_source().get_token(),
                                                       ignoreProgressCallback);

    const DuplicateGroup expectedDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "first.bin", duplicateContents.size()),
        createExpectedFileRecord(scanRootPath, "second.bin", duplicateContents.size())
    });

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedDuplicateGroup));
    EXPECT_EQ(fullHashCalculationCount, 2);
}

/// @brief Verifies that differences outside the sampled prefix cannot produce false duplicate results.
///
/// @par Test setup
/// Create two large files with identical sampled prefixes, but change one byte beyond that prefix. Count calls to
/// the production full-hash calculator.
///
/// @par Procedure
/// Execute the workflow and inspect both the result and full-hash call count.
///
/// @par Expected results
/// Matching samples retain both candidates, full hashing reads both complete files, and their off-sample
/// difference prevents a duplicate group from being reported.
TEST_F(ScanByFileContentTest, RejectLargeFilesDifferingOutsideSampledPrefix_AfterFullHashing)
{
    QByteArray firstContents(sampledTestFileSizeBytes, 'A');
    QByteArray secondContents = firstContents;
    secondContents[1024 * 1024 + 128 * 1024] = 'B';

    ASSERT_TRUE(writeFile("first.bin", firstContents));
    ASSERT_TRUE(writeFile("second.bin", secondContents));

    quint64 fullHashCalculationCount = 0;
    const FileContentScanWorkflow scanWorkflow = createScanWorkflow(
        [&fullHashCalculationCount](const FileRecord& file, const std::stop_token& stopToken)
        {
            ++fullHashCalculationCount;
            return FileHasher::calculateFullFileHash(file, stopToken);
        },
        &FileHasher::calculateFileSampleHash,
        forceLargeFileSamplingThresholdBytes);

    const ScanResult scanResult = scanWorkflow.execute(QStringList{getTemporaryScanRootPath()},
                                                       std::stop_source().get_token(),
                                                       ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithoutDuplicates);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
    EXPECT_EQ(fullHashCalculationCount, 2);
}

/// @brief Verifies correctness when unrelated large files receive the same sample and full hash values.
///
/// @par Test setup
/// Create one identical pair and one different equal-size file. Inject a sample calculator returning one
/// artificial value for every candidate and a full-hash calculator returning a second artificial value.
///
/// @par Procedure
/// Execute the workflow and compare the result with the genuine duplicate pair.
///
/// @par Expected results
/// Both collision layers merely retain additional candidates; final byte comparison prevents a false duplicate,
/// and only the identical pair is returned.
TEST_F(ScanByFileContentTest, KeepCorrectResults_WhenLargeFileSampleAndFullHashesCollide)
{
    ASSERT_TRUE(writeFile("first.bin", "AAAA"));
    ASSERT_TRUE(writeFile("second.bin", "AAAA"));
    ASSERT_TRUE(writeFile("different.bin", "BBBB"));

    quint64 fullHashCalculationCount = 0;
    const FileContentScanWorkflow scanWorkflow = createScanWorkflow(
        [&fullHashCalculationCount](const FileRecord&, const std::stop_token&) -> std::optional<QByteArray>
        {
            ++fullHashCalculationCount;
            return QByteArray("forced-full-hash-collision");
        },
        [](const FileRecord&, const std::stop_token&) -> std::optional<QByteArray>
        {
            return QByteArray("forced-sample-hash-collision");
        },
        forceLargeFileSamplingThresholdBytes);

    const QString scanRootPath = getTemporaryScanRootPath();
    const ScanResult scanResult = scanWorkflow.execute(QStringList{scanRootPath},
                                                       std::stop_source().get_token(),
                                                       ignoreProgressCallback);

    const DuplicateGroup expectedDuplicateGroup = createExpectedDuplicateGroup({
        createExpectedFileRecord(scanRootPath, "first.bin", 4),
        createExpectedFileRecord(scanRootPath, "second.bin", 4)
    });

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::CompletedWithDuplicates);
    ASSERT_EQ(scanResult.getDuplicateGroups().size(), 1);
    EXPECT_TRUE(containsDuplicateGroup(scanResult, expectedDuplicateGroup));
    EXPECT_EQ(fullHashCalculationCount, 3);
}

/// @brief Verifies that a large-file sampling failure cannot yield an incomplete successful result.
///
/// @par Test setup
/// Create two equal-size files and inject a sample calculator that fails without requesting cancellation.
///
/// @par Procedure
/// Execute the workflow with forced large-file sampling.
///
/// @par Expected results
/// The scan fails, returns no unverified groups, and never invokes the full-hash calculator.
TEST_F(ScanByFileContentTest, ReturnFailure_WhenLargeFileSamplingFails)
{
    ASSERT_TRUE(writeFile("first.bin", "AAAA"));
    ASSERT_TRUE(writeFile("second.bin", "BBBB"));

    quint64 fullHashCalculationCount = 0;
    const FileContentScanWorkflow scanWorkflow = createScanWorkflow(
        [&fullHashCalculationCount](const FileRecord&, const std::stop_token&) -> std::optional<QByteArray>
        {
            ++fullHashCalculationCount;
            return QByteArray("unexpected-full-hash");
        },
        [](const FileRecord&, const std::stop_token&) -> std::optional<QByteArray>
        {
            return std::nullopt;
        },
        forceLargeFileSamplingThresholdBytes);

    const ScanResult scanResult = scanWorkflow.execute(QStringList{getTemporaryScanRootPath()},
                                                       std::stop_source().get_token(),
                                                       ignoreProgressCallback);

    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Failed);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
    EXPECT_EQ(fullHashCalculationCount, 0);
}

/// @brief Verifies cancellation requested from within large-file sampling.
///
/// @par Test setup
/// Create two equal-size files and inject a sample calculator that requests stop before returning no hash.
///
/// @par Procedure
/// Execute the workflow with the calculator sharing its stop source with the workflow.
///
/// @par Expected results
/// The missing sample is classified as cancellation rather than failure, and no unverified group is returned.
TEST_F(ScanByFileContentTest, ReturnCancellation_WhenStopIsRequestedDuringLargeFileSampling)
{
    ASSERT_TRUE(writeFile("first.bin", "AAAA"));
    ASSERT_TRUE(writeFile("second.bin", "BBBB"));

    std::stop_source stopSource;
    const FileContentScanWorkflow scanWorkflow = createScanWorkflow(
        &FileHasher::calculateFullFileHash,
        [&stopSource](const FileRecord&, const std::stop_token&) -> std::optional<QByteArray>
        {
            stopSource.request_stop();
            return std::nullopt;
        },
        forceLargeFileSamplingThresholdBytes);

    const ScanResult scanResult = scanWorkflow.execute(QStringList{getTemporaryScanRootPath()},
                                                       stopSource.get_token(),
                                                       ignoreProgressCallback);

    EXPECT_TRUE(stopSource.stop_requested());
    EXPECT_EQ(scanResult.getOutcome(), ScanOutcome::Cancelled);
    EXPECT_TRUE(scanResult.getDuplicateGroups().isEmpty());
}

/// @brief Verifies exact byte comparison when unrelated files share the same calculated hash.
///
/// @par Test setup
/// Create two `AAAA` files, two `BBBB` files, and one `CCCC` file of equal size. Inject a hash calculator that
/// returns the same artificial hash for every file.
///
/// @par Procedure
/// Execute the workflow, compare the returned partitions with the two expected exact-content groups, and inspect
/// duplicate and recoverable summary metrics.
///
/// @par Expected results
/// The collision bucket is split into the two genuine duplicate pairs, the singleton is excluded, and the summary
/// reports four grouped files with eight recoverable bytes.
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

    const FileContentScanWorkflow scanWorkflow = createScanWorkflow(
        [](const FileRecord&, const std::stop_token&) -> std::optional<QByteArray>
        {
            // Returning one hash for every file simulates a cryptographic hash collision deterministically.
            return QByteArray("forced-hash-collision");
        });

    const ScanResult scanResult = scanWorkflow.execute(QStringList{scanRootPath},
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

/// @brief Verifies failure when a collected candidate disappears before hashing.
///
/// @par Test setup
/// Create two identical files and configure a progress callback to remove one candidate at the start of the
/// hashing phase.
///
/// @par Procedure
/// Execute the file-content-based workflow and record whether the candidate was removed before its contents could
/// be hashed.
///
/// @par Expected results
/// Removal succeeds, the workflow reports `Failed`, no unverified group is returned, and the discovery-time
/// problematic-file count remains zero.
TEST_F(ScanByFileContentTest, CheckFailedOutcome_CollectedCandidateRemovedBeforeHashing)
{
    ASSERT_TRUE(writeFile("one", "same"));
    ASSERT_TRUE(writeFile("two", "same"));

    const QString fileToRemove = QDir(getTemporaryScanRootPath()).filePath("one");
    bool removedHashCandidate = false;
    const ScanResult scanResult = FileContentScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
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
    EXPECT_EQ(scanResult.getProblematicFilesCount(), 0);
}

/// @brief Verifies failure when a matching-hash candidate disappears before exact byte comparison.
///
/// @par Test setup
/// Create two identical files and configure a progress callback to remove one after both hashes are available but
/// before content verification starts.
///
/// @par Procedure
/// Execute the file-content-based workflow and record whether removal occurs at the initial verification update.
///
/// @par Expected results
/// Removal succeeds, exact comparison failure produces `ScanOutcome::Failed`, no unverified group is returned,
/// and the discovery-time problematic-file count remains zero.
TEST_F(ScanByFileContentTest, CheckFailedOutcome_HashCandidateRemovedBeforeByteComparison)
{
    ASSERT_TRUE(writeFile("one", "same"));
    ASSERT_TRUE(writeFile("two", "same"));

    const QString fileToRemove = QDir(getTemporaryScanRootPath()).filePath("one");
    bool removedComparisonCandidate = false;
    const ScanResult scanResult = FileContentScanWorkflow().execute(QStringList{getTemporaryScanRootPath()},
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
    EXPECT_EQ(scanResult.getProblematicFilesCount(), 0);
}
