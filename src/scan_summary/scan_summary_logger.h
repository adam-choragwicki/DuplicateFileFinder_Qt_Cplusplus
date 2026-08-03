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
    static void logCommonFields(const ScanSummary& summary);
    static void logFileNameSummary(const FileNameScanSummary& summary);
    static void logFileContentSummary(const FileContentScanSummary& summary);
};

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
    qInfo() << "  Duration (ms):" << summary.getDuration().count();
    qInfo() << "  Scanned directories:" << summary.getScannedDirectoriesCount();
    qInfo() << "  Scanned files:" << summary.getScannedFilesCount();
    qInfo() << "  Total scanned bytes:" << summary.getTotalScannedBytes();
    qInfo() << "  Duplicate groups:" << summary.getDuplicateGroupsCount();
    qInfo() << "  Files in duplicate groups:" << summary.getTotalFilesInDuplicateGroupsCount();
    qInfo() << "  Bytes occupied by files in duplicate groups:" << summary.getTotalBytesOccupiedByFilesInDuplicateGroups();
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
    qInfo() << "  Potentially recoverable bytes:" << summary.getTotalAmountOfPotentiallyRecoverableBytes();
}
