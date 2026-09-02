#pragma once

/// @brief Describes the final outcome of a duplicate-file scan.
enum class ScanOutcome
{
    /// The scan completed successfully and produced at least one duplicate group.
    CompletedWithDuplicates,
    /// At least one readable file was scanned successfully, but no duplicates were found.
    CompletedWithoutDuplicates,
    /// Enumeration completed, but no readable files were available for duplicate detection.
    NoFilesFound,
    /// A cancellation request stopped the scan before all stages completed.
    Cancelled,
    /// The scan could not complete because of invalid input or a processing error.
    Failed
};
