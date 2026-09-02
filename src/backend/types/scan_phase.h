#pragma once

#include <QString>
#include <variant>

/// @brief Identifies the current stage of a file-name-based scan.
enum class FileNameScanPhase
{
    /// Recursively discovering readable files below the selected roots.
    EnumeratingFiles,
    /// Grouping enumerated files by their normalized comparison name.
    GroupingFilesByName,
    /// Calculating final metrics and constructing the scan result.
    BuildingScanResult
};

/// @brief Identifies the current stage of a file-content-based scan.
enum class FileContentScanPhase
{
    /// Recursively discovering readable files below the selected roots.
    EnumeratingFiles,
    /// Selecting size groups that contain at least two possible duplicates.
    IdentifyingEqualSizeCandidates,
    /// Sample-hashing or fully hashing files that remain duplicate candidates.
    HashingDuplicateCandidateFiles,
    /// Comparing matching-hash candidates byte by byte to establish exact equality.
    VerifyingMatchingHashCandidates,
    /// Calculating final metrics and constructing the scan result.
    BuildingScanResult
};

/// Holds a phase belonging to either supported scan workflow.
using ScanPhase = std::variant<FileNameScanPhase, FileContentScanPhase>;

/// Returns user-facing progress text for a file-name scan phase.
/// @param[in] phase Phase to describe.
/// @return User-facing display text, or a generic scanning description for an unknown enum value.
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

/// Returns user-facing progress text for a file-content scan phase.
/// @param[in] phase Phase to describe.
/// @return User-facing display text, or a generic scanning description for an unknown enum value.
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

/// Returns user-facing progress text for the active concrete phase stored in a ScanPhase variant.
/// @param[in] phase File-name or file-content scan phase to describe.
/// @return Display text produced by the overload for the active phase type.
inline QString scanPhaseDescription(const ScanPhase& phase)
{
    return std::visit([](const auto workflowPhase)
    {
        return scanPhaseDescription(workflowPhase);
    }, phase);
}
