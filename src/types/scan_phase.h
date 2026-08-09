#pragma once

#include <QString>
#include <variant>

enum class FileNameScanPhase
{
    EnumeratingFiles,
    GroupingFilesByName,
    BuildingScanResult
};

enum class FileContentScanPhase
{
    EnumeratingFiles,
    IdentifyingEqualSizeCandidates,
    HashingDuplicateCandidateFiles,
    VerifyingMatchingHashCandidates,
    BuildingScanResult
};

// variant enabling use of visitor in scanPhaseDescription
using ScanPhase = std::variant<FileNameScanPhase, FileContentScanPhase>;

inline QString scanPhaseDescription(const FileNameScanPhase phase)
{
    switch (phase)
    {
        case FileNameScanPhase::EnumeratingFiles:
            return QStringLiteral("Enumerating files...");
        case FileNameScanPhase::GroupingFilesByName:
            return QStringLiteral("Grouping files by name...");
        case FileNameScanPhase::BuildingScanResult:
            return QStringLiteral("Building scan result...");
    }

    return QStringLiteral("Scanning...");
}

inline QString scanPhaseDescription(const FileContentScanPhase phase)
{
    switch (phase)
    {
        case FileContentScanPhase::EnumeratingFiles:
            return QStringLiteral("Enumerating files...");
        case FileContentScanPhase::IdentifyingEqualSizeCandidates:
            return QStringLiteral("Identifying equal-size candidates...");
        case FileContentScanPhase::HashingDuplicateCandidateFiles:
            return QStringLiteral("Hashing and grouping duplicate-candidate files...");
        case FileContentScanPhase::VerifyingMatchingHashCandidates:
            return QStringLiteral("Verifying matching-hash files byte by byte...");
        case FileContentScanPhase::BuildingScanResult:
            return QStringLiteral("Building scan result...");
    }

    return QStringLiteral("Scanning...");
}

inline QString scanPhaseDescription(const ScanPhase& phase)
{
    return std::visit([](const auto workflowPhase)
    {
        return scanPhaseDescription(workflowPhase);
    }, phase);
}
