#pragma once

#include "file_tree_enumeration_metrics.h"
#include "file_tree_enumeration_status.h"

/// @brief Describes how file-tree enumeration ended and how much work was completed.
///
/// Metrics are retained for cancelled and failed enumerations, so callers can report work completed before the
/// terminating condition was observed.
class FileTreeEnumerationResult
{
public:
    /// Creates an enumeration result by copying the supplied metrics snapshot.
    /// @param[in] status Final enumeration status.
    /// @param[in] metrics Metrics accumulated before enumeration ended.
    FileTreeEnumerationResult(const FileTreeEnumerationStatus status, const FileTreeEnumerationMetrics& metrics)
        : status_(status), metrics_(metrics)
    {}

    /// Returns the condition that ended enumeration.
    [[nodiscard]] FileTreeEnumerationStatus getStatus() const { return status_; }
    /// Returns the metrics accumulated before enumeration ended.
    [[nodiscard]] const FileTreeEnumerationMetrics& getMetrics() const { return metrics_; }

private:
    FileTreeEnumerationStatus status_;
    FileTreeEnumerationMetrics metrics_;
};
