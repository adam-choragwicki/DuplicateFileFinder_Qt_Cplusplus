#pragma once

#include <QString>

enum class ScanPhase
{
    EnumeratingFiles,
    GroupingFilesBySize,
    IdentifyingEqualSizeCandidates,
    HashingDuplicateCandidateFiles,
    GroupingCandidatesByHash,
    IdentifyingEqualHashCandidates,
    VerifyingFilesByteForByte,
    GroupingFilesByName,
    BuildingScanResult
};

inline QString scanPhaseDescription(const ScanPhase phase)
{
    switch (phase)
    {
        case ScanPhase::EnumeratingFiles:
            return QStringLiteral("Enumerating files...");
        case ScanPhase::GroupingFilesBySize:
            return QStringLiteral("Grouping files by size...");
        case ScanPhase::IdentifyingEqualSizeCandidates:
            return QStringLiteral("Identifying equal-size candidates...");
        case ScanPhase::HashingDuplicateCandidateFiles:
            return QStringLiteral("Hashing and grouping duplicate-candidate files...");
        case ScanPhase::GroupingCandidatesByHash:
            return QStringLiteral("Grouping candidates by hash...");
        case ScanPhase::IdentifyingEqualHashCandidates:
            return QStringLiteral("Identifying equal-hash candidates...");
        case ScanPhase::VerifyingFilesByteForByte:
            return QStringLiteral("Verifying files byte for byte...");
        case ScanPhase::GroupingFilesByName:
            return QStringLiteral("Grouping files by name...");
        case ScanPhase::BuildingScanResult:
            return QStringLiteral("Building scan result...");
    }

    return QStringLiteral("Scanning...");
}
