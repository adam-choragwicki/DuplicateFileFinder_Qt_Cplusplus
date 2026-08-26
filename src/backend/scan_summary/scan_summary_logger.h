#pragma once

#include "file_content_scan_summary.h"
#include "file_name_scan_summary.h"
#include "types/scan_result.h"
#include "scan_summary.h"

class ScanSummaryLogger final
{
public:
    static void log(const ScanResult& scanResult);

private:
    [[nodiscard]] static QString formatByteCount(quint64 bytes);
    [[nodiscard]] static QString formatDuration(std::chrono::milliseconds duration);
    static void logCommonFields(const ScanSummary& summary);
    static void logFileNameSummary(const FileNameScanSummary& summary);
    static void logFileContentSummary(const FileContentScanSummary& summary);
};

inline QString ScanSummaryLogger::formatByteCount(const quint64 bytes)
{
    constexpr quint64 bytesPerKilobyte = 1024;
    constexpr quint64 bytesPerMegabyte = bytesPerKilobyte * 1024;
    constexpr quint64 bytesPerGigabyte = bytesPerMegabyte * 1024;
    constexpr quint64 bytesPerTerabyte = bytesPerGigabyte * 1024;

    if (bytes < bytesPerKilobyte)
    {
        return QStringLiteral("%1 B").arg(bytes);
    }

    quint64 divisor;
    QString unit;

    if (bytes < bytesPerMegabyte)
    {
        divisor = bytesPerKilobyte;
        unit = QStringLiteral("kB");
    }
    else if (bytes < bytesPerGigabyte)
    {
        divisor = bytesPerMegabyte;
        unit = QStringLiteral("MB");
    }
    else if (bytes < bytesPerTerabyte)
    {
        divisor = bytesPerGigabyte;
        unit = QStringLiteral("GB");
    }
    else
    {
        divisor = bytesPerTerabyte;
        unit = QStringLiteral("TB");
    }

    QString formattedValue = QString::number(static_cast<double>(bytes) / static_cast<double>(divisor), 'f', 2);

    while (formattedValue.endsWith('0'))
    {
        formattedValue.chop(1);
    }
    if (formattedValue.endsWith('.'))
    {
        formattedValue.chop(1);
    }

    return QStringLiteral("%1 %2").arg(formattedValue, unit);
}

inline QString ScanSummaryLogger::formatDuration(const std::chrono::milliseconds duration)
{
    constexpr qint64 millisecondsPerSecond = 1000;
    constexpr qint64 secondsPerMinute = 60;
    constexpr qint64 minutesPerHour = 60;
    constexpr qint64 millisecondsPerMinute = millisecondsPerSecond * secondsPerMinute;
    constexpr qint64 millisecondsPerHour = millisecondsPerMinute * minutesPerHour;

    const qint64 totalMilliseconds = qMax<qint64>(duration.count(), 0);
    if (totalMilliseconds < millisecondsPerSecond)
    {
        return QStringLiteral("%1 ms").arg(totalMilliseconds);
    }

    const auto formatUnit = [](const qint64 value, const QString& singular, const QString& plural)
    {
        return QStringLiteral("%1 %2").arg(value).arg(value == 1 ? singular : plural);
    };

    const qint64 totalSeconds = totalMilliseconds / millisecondsPerSecond;
    if (totalMilliseconds < millisecondsPerMinute)
    {
        return formatUnit(totalSeconds, QStringLiteral("second"), QStringLiteral("seconds"));
    }

    if (totalMilliseconds < millisecondsPerHour)
    {
        const qint64 minutes = totalSeconds / secondsPerMinute;
        const qint64 seconds = totalSeconds % secondsPerMinute;
        return QStringLiteral("%1 and %2").arg(
            formatUnit(minutes, QStringLiteral("minute"), QStringLiteral("minutes")),
            formatUnit(seconds, QStringLiteral("second"), QStringLiteral("seconds")));
    }

    const qint64 totalMinutes = totalSeconds / secondsPerMinute;
    const qint64 hours = totalMinutes / minutesPerHour;
    const qint64 minutes = totalMinutes % minutesPerHour;
    return QStringLiteral("%1 and %2").arg(
        formatUnit(hours, QStringLiteral("hour"), QStringLiteral("hours")),
        formatUnit(minutes, QStringLiteral("minute"), QStringLiteral("minutes")));
}

inline void ScanSummaryLogger::log(const ScanResult& scanResult)
{
    std::visit(
        [](const auto& summary)
        {
            using SummaryType = std::decay_t<decltype(summary)>;

            if constexpr (std::is_same_v<SummaryType, FileNameScanSummary>)
            {
                logFileNameSummary(summary);
            }
            else
            {
                logFileContentSummary(summary);
            }
        },
        scanResult.getScanSummaryDetails());
}

inline void ScanSummaryLogger::logCommonFields(const ScanSummary& summary)
{
    qInfo().noquote() << "  Completed at:" << summary.getCompletedAt().toLocalTime().toString("yyyy-MM-dd hh:mm:ss");
    qInfo().noquote() << "  Duration:" << formatDuration(summary.getDuration());
    qInfo() << "  Scanned directories:" << summary.getScannedDirectoriesCount();
    qInfo() << "  Scanned files:" << summary.getScannedFilesCount();
    qInfo() << "  Problematic files skipped:" << summary.getProblematicFilesCount();
    qInfo() << "  Duplicate groups:" << summary.getDuplicateGroupsCount();
    qInfo() << "  Files in duplicate groups:" << summary.getTotalFilesInDuplicateGroupsCount();
    qInfo().noquote() << "  Disk space occupied by files in duplicate groups:" << formatByteCount(summary.getTotalBytesOccupiedByFilesInDuplicateGroups());
}

inline void ScanSummaryLogger::logFileNameSummary(const FileNameScanSummary& summary)
{
    qInfo() << "File name scan summary:";
    logCommonFields(summary);
}

inline void ScanSummaryLogger::logFileContentSummary(const FileContentScanSummary& summary)
{
    qInfo() << "File content scan summary:";
    logCommonFields(summary);
    qInfo().noquote() << "  Potentially recoverable disk space:" << formatByteCount(summary.getTotalAmountOfPotentiallyRecoverableBytes());
}
