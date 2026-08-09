#include "scan_test_fixtures.h"

#include "scan_summary/file_content_scan_summary.h"
#include "scan_summary/file_name_scan_summary.h"

#include <QDateTime>

#include <chrono>

TEST_F(ScanSummaryLoggerTest, CheckFileNameSummary_FileNameScanOmitsRecoverableDiskSpace)
{
    const ScanResult result(
        {},
        ScanOutcome::CompletedWithoutDuplicates,
        FileNameScanSummary{
            QDateTime::currentDateTimeUtc(),
            std::chrono::milliseconds(1),
            1,
            2,
            3,
            0,
            0,
            0
        });

    const QString log = captureLog(result);

    EXPECT_TRUE(log.contains("File name scan summary"));
    EXPECT_FALSE(log.contains("Potentially recoverable disk space"));
}

TEST_F(ScanSummaryLoggerTest, CheckFileContentSummary_ByteCountsAndDurationAreFormatted)
{
    const ScanResult result(
        {},
        ScanOutcome::CompletedWithDuplicates,
        FileContentScanSummary{
            QDateTime::currentDateTimeUtc(),
            std::chrono::milliseconds(1),
            1,
            2,
            3,
            1,
            2,
            4,
            2
        });

    const QString log = captureLog(result);

    EXPECT_TRUE(log.contains("File content scan summary"));
    EXPECT_TRUE(log.contains("Disk space occupied by files in duplicate groups: 4 B"));
    EXPECT_TRUE(log.contains("Potentially recoverable disk space: 2 B"));
    EXPECT_TRUE(log.contains("Duration: 1 ms"));
}

TEST_F(ScanSummaryLoggerTest, CheckCommonSummary_TotalScannedBytesAreOmitted)
{
    const ScanResult result(
        {},
        ScanOutcome::CompletedWithoutDuplicates,
        FileNameScanSummary{
            QDateTime::currentDateTimeUtc(),
            std::chrono::milliseconds(1),
            1,
            2,
            3,
            0,
            0,
            0
        });

    EXPECT_FALSE(captureLog(result).contains("Total scanned bytes"));
}
