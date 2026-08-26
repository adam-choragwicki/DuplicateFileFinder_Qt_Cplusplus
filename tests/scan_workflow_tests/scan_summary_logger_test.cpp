#include "scan_summary_logger_test_fixture.h"

#include "scan_summary/file_content_scan_summary.h"
#include "scan_summary/file_name_scan_summary.h"

#include <QDateTime>

#include <chrono>

/// @brief Verifies the complete ordered output produced for a file-name scan summary.
///
/// @par Test setup
/// Construct a file-name scan result with a fixed local completion time and distinct values for every common
/// summary metric, including the internal total-scanned-byte value.
///
/// @par Procedure
/// Capture every message written by `ScanSummaryLogger` and compare the complete message list line by line with
/// the required file-name scan summary.
///
/// @par Expected results
/// - Every supported common field is logged exactly once, in the required order and format.
/// - No additional fields are logged; in particular, total scanned bytes and potentially recoverable disk space
///   are absent.
TEST_F(ScanSummaryLoggerTest, LogExactSummary_WhenFileNameBasedScanResultIsProvided)
{
    const QDateTime completedAt{QDate(2025, 1, 2), QTime(3, 4, 5)};
    const ScanResult result(
        {},
        ScanOutcome::CompletedWithDuplicates,
        FileNameScanSummary{
            completedAt,
            std::chrono::milliseconds(1),
            1,
            2,
            3,
            4,
            5,
            6,
            7
        });

    const QStringList expectedMessages{
        QStringLiteral("File name scan summary:"),
        QStringLiteral("  Completed at: 2025-01-02 03:04:05"),
        QStringLiteral("  Duration: 1 ms"),
        QStringLiteral("  Scanned directories: 1"),
        QStringLiteral("  Scanned files: 2"),
        QStringLiteral("  Problematic files skipped: 3"),
        QStringLiteral("  Duplicate groups: 5"),
        QStringLiteral("  Files in duplicate groups: 6"),
        QStringLiteral("  Disk space occupied by files in duplicate groups: 7 B")
    };

    verifyExactLogMessages(captureLogMessages(result), expectedMessages);
}

/// @brief Verifies the complete ordered output produced for a file-content-based scan summary.
///
/// @par Test setup
/// Construct a file-content-based scan result with a fixed local completion time and distinct values for every
/// common and content-specific summary metric, including the internal total-scanned-byte value.
///
/// @par Procedure
/// Capture every message written by `ScanSummaryLogger` and compare the complete message list line by line with
/// the required file-content-based scan summary.
///
/// @par Expected results
/// - Every supported common field and the potentially recoverable disk-space field are logged exactly once, in
///   the required order and format.
/// - No additional fields are logged; in particular, total scanned bytes is absent.
TEST_F(ScanSummaryLoggerTest, LogExactSummary_WhenFileContentBasedScanResultIsProvided)
{
    const QDateTime completedAt{QDate(2025, 1, 2), QTime(3, 4, 5)};
    const ScanResult result(
        {},
        ScanOutcome::CompletedWithDuplicates,
        FileContentScanSummary{
            completedAt,
            std::chrono::milliseconds(1),
            1,
            2,
            3,
            4,
            5,
            6,
            7,
            8
        });

    const QStringList expectedMessages{
        QStringLiteral("File content scan summary:"),
        QStringLiteral("  Completed at: 2025-01-02 03:04:05"),
        QStringLiteral("  Duration: 1 ms"),
        QStringLiteral("  Scanned directories: 1"),
        QStringLiteral("  Scanned files: 2"),
        QStringLiteral("  Problematic files skipped: 3"),
        QStringLiteral("  Duplicate groups: 5"),
        QStringLiteral("  Files in duplicate groups: 6"),
        QStringLiteral("  Disk space occupied by files in duplicate groups: 7 B"),
        QStringLiteral("  Potentially recoverable disk space: 8 B")
    };

    verifyExactLogMessages(captureLogMessages(result), expectedMessages);
}

/// @brief Verifies byte-count unit selection, boundary handling, rounding, and redundant-zero removal.
///
/// @par Test setup
/// Supply a parameter containing a byte count from the B, kB, MB, GB, or TB range and its required formatted
/// representation. Store that count in both byte fields of a file-content-based scan summary.
///
/// @par Procedure
/// Log the summary, select the two byte-count messages by their field labels, and compare them with the exact
/// expected representation supplied by the parameter.
///
/// @par Expected results
/// Both duplicate-space and recoverable-space fields use the expected unit and value, with at most two decimal
/// places and no redundant trailing zeroes.
TEST_P(ScanSummaryByteCountFormattingTest, FormatByteCounts_WhenSummaryIsLogged)
{
    const ByteCountFormattingScenario& scenario = GetParam();
    const ScanResult result(
        {},
        ScanOutcome::CompletedWithDuplicates,
        FileContentScanSummary{
            QDateTime::currentDateTime(),
            std::chrono::milliseconds(0),
            0,
            0,
            0,
            0,
            0,
            0,
            scenario.byteCount,
            scenario.byteCount
        });

    const QStringList messages = captureLogMessages(result);
    const QStringList duplicateSpaceMessages = findMessagesStartingWith(messages,
                                                                        QStringLiteral("  Disk space occupied by files in duplicate groups:"));
    const QStringList recoverableSpaceMessages = findMessagesStartingWith(messages,
                                                                          QStringLiteral("  Potentially recoverable disk space:"));

    ASSERT_EQ(duplicateSpaceMessages.size(), 1);
    ASSERT_EQ(recoverableSpaceMessages.size(), 1);

    const QString expectedFormattedByteCount = QString::fromLatin1(scenario.expectedFormattedByteCount);
    EXPECT_EQ(duplicateSpaceMessages.constFirst().toStdString(),
              QStringLiteral("  Disk space occupied by files in duplicate groups: %1")
              .arg(expectedFormattedByteCount).toStdString());
    EXPECT_EQ(recoverableSpaceMessages.constFirst().toStdString(),
              QStringLiteral("  Potentially recoverable disk space: %1")
              .arg(expectedFormattedByteCount).toStdString());
}

/// @brief Creates the named byte-formatting test instances consumed by the preceding `TEST_P` definition.
///
/// The macro arguments, in order, are:
/// - `ByteUnitsAndBoundaries`: prefix added to the generated GoogleTest suite name.
/// - `ScanSummaryByteCountFormattingTest`: parameterized fixture containing the `TEST_P` definition.
/// - `testing::Values(...)`: scenarios supplied one at a time through `GetParam()`; each scenario provides its
///   readable name, input byte count, and expected formatted text.
/// - The final lambda: returns the scenario's readable name so test output uses names such as `ExactGigabyte`
///   instead of numeric parameter indexes.
INSTANTIATE_TEST_SUITE_P(
    ByteUnitsAndBoundaries,
    ScanSummaryByteCountFormattingTest,
    testing::Values(
        ByteCountFormattingScenario{"ZeroBytes", 0, "0 B"},
        ByteCountFormattingScenario{"LargestByteValue", 1023, "1023 B"},
        ByteCountFormattingScenario{"ExactKilobyte", 1024, "1 kB"},
        ByteCountFormattingScenario{"FractionalKilobytes", 1260, "1.23 kB"},
        ByteCountFormattingScenario{"ExactMegabyte", 1024ULL * 1024, "1 MB"},
        ByteCountFormattingScenario{"ExactGigabyte", 1024ULL * 1024 * 1024, "1 GB"},
        ByteCountFormattingScenario{"ExactTerabyte", 1024ULL * 1024 * 1024 * 1024, "1 TB"}),
    [](const testing::TestParamInfo<ByteCountFormattingScenario>& information)
    {
        return information.param.name;
    });

/// @brief Verifies duration boundaries, truncation, negative-value handling, and singular or plural units.
///
/// @par Test setup
/// Supply a parameter containing a duration from the millisecond, second, minute, or hour range and its required
/// formatted representation.
///
/// @par Procedure
/// Store the duration in a file-name scan summary, log it, select the duration message, and compare that complete
/// message with the representation supplied by the parameter.
///
/// @par Expected results
/// The duration uses the required units and singular or plural wording. Sub-unit remainders are truncated, and
/// negative durations are clamped to zero milliseconds.
TEST_P(ScanSummaryDurationFormattingTest, FormatDuration_WhenSummaryIsLogged)
{
    const DurationFormattingScenario& scenario = GetParam();
    const ScanResult result(
        {},
        ScanOutcome::CompletedWithoutDuplicates,
        FileNameScanSummary{
            QDateTime::currentDateTime(),
            scenario.duration,
            0,
            0,
            0,
            0,
            0,
            0,
            0
        });

    const QStringList durationMessages = findMessagesStartingWith(captureLogMessages(result),
                                                                  QStringLiteral("  Duration:"));

    ASSERT_EQ(durationMessages.size(), 1);
    EXPECT_EQ(durationMessages.constFirst().toStdString(),
              QStringLiteral("  Duration: %1")
              .arg(QString::fromLatin1(scenario.expectedFormattedDuration)).toStdString());
}

/// @brief Creates the named duration-formatting test instances consumed by the preceding `TEST_P` definition.
///
/// The macro arguments, in order, are:
/// - `DurationUnitsAndBoundaries`: prefix added to the generated GoogleTest suite name.
/// - `ScanSummaryDurationFormattingTest`: parameterized fixture containing the `TEST_P` definition.
/// - `testing::Values(...)`: scenarios supplied one at a time through `GetParam()`; each scenario provides its
///   readable name, input duration, and expected formatted text.
/// - The final lambda: returns the scenario's readable name so test output uses names such as `OneMinute`
///   instead of numeric parameter indexes.
INSTANTIATE_TEST_SUITE_P(
    DurationUnitsAndBoundaries,
    ScanSummaryDurationFormattingTest,
    testing::Values(
        DurationFormattingScenario{"NegativeDuration", std::chrono::milliseconds(-1), "0 ms"},
        DurationFormattingScenario{"ZeroMilliseconds", std::chrono::milliseconds(0), "0 ms"},
        DurationFormattingScenario{"LargestMillisecondValue", std::chrono::milliseconds(999), "999 ms"},
        DurationFormattingScenario{"OneSecond", std::chrono::milliseconds(1000), "1 second"},
        DurationFormattingScenario{"PluralSeconds", std::chrono::milliseconds(2000), "2 seconds"},
        DurationFormattingScenario{"SubsecondRemainderIsTruncated", std::chrono::milliseconds(59999), "59 seconds"},
        DurationFormattingScenario{"OneMinute", std::chrono::milliseconds(60000), "1 minute and 0 seconds"},
        DurationFormattingScenario{"SingularMinuteAndSecond", std::chrono::milliseconds(61000), "1 minute and 1 second"},
        DurationFormattingScenario{"PluralMinutesAndSeconds", std::chrono::milliseconds(122000), "2 minutes and 2 seconds"},
        DurationFormattingScenario{"OneHour", std::chrono::milliseconds(3600000), "1 hour and 0 minutes"},
        DurationFormattingScenario{"SingularHourAndMinute", std::chrono::milliseconds(3660000), "1 hour and 1 minute"},
        DurationFormattingScenario{"PluralHoursAndMinutes", std::chrono::milliseconds(7320000), "2 hours and 2 minutes"}),
    [](const testing::TestParamInfo<DurationFormattingScenario>& information)
    {
        return information.param.name;
    });
