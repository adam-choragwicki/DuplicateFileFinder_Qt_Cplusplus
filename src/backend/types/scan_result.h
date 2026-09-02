#pragma once

#include "duplicate_group.h"
#include "scan_summary/file_name_scan_summary.h"
#include "scan_summary/file_content_scan_summary.h"
#include "scan_outcome.h"

#include <QList>

/// Holds the concrete summary produced by either supported scan workflow.
using ScanSummaryDetails = std::variant<FileNameScanSummary, FileContentScanSummary>;

/// @brief Owns the duplicate groups, outcome, and workflow-specific summary produced by a scan.
///
/// The summary alternative identifies which scan workflow produced the result. Callers constructing or modifying a
/// result are responsible for keeping its outcome, groups, and summary semantically consistent.
class ScanResult
{
public:
    /// Creates a result produced by a file-name-based scan.
    /// @param[in] duplicateGroups Duplicate groups to move into the result.
    /// @param[in] outcome Final scan outcome.
    /// @param[in] scanSummary File-name scan metrics and timing information to move into the result.
    ScanResult(QList<DuplicateGroup> duplicateGroups, const ScanOutcome outcome, FileNameScanSummary scanSummary)
        : duplicateGroups_(std::move(duplicateGroups)),
          scanSummaryDetails_(std::move(scanSummary)),
          outcome_(outcome)
    {}

    /// Creates a result produced by a file-content-based scan.
    /// @param[in] duplicateGroups Duplicate groups to move into the result.
    /// @param[in] outcome Final scan outcome.
    /// @param[in] scanSummary File-content scan metrics and timing information to move into the result.
    ScanResult(QList<DuplicateGroup> duplicateGroups, const ScanOutcome outcome, FileContentScanSummary scanSummary)
        : duplicateGroups_(std::move(duplicateGroups)),
          scanSummaryDetails_(std::move(scanSummary)),
          outcome_(outcome)
    {}

    /// Returns the duplicate groups in workflow-produced order.
    [[nodiscard]] const QList<DuplicateGroup>& getDuplicateGroups() const { return duplicateGroups_; }
    /// Returns the concrete file-name or file-content summary.
    [[nodiscard]] const ScanSummaryDetails& getScanSummaryDetails() const { return scanSummaryDetails_; }
    /// Returns the current scan outcome.
    [[nodiscard]] ScanOutcome getOutcome() const { return outcome_; }

    /// Returns the number of inaccessible files reported by the stored concrete summary.
    [[nodiscard]] quint64 getProblematicFilesCount() const
    {
        return std::visit([](const auto& summary)
                          {
                              return summary.getProblematicFilesCount();
                          },
                          scanSummaryDetails_);
    }

    /// Replaces the outcome without changing duplicate groups or summary details.
    /// @param[in] outcomeValue New scan outcome, typically used to apply a late cancellation request.
    void setOutcome(const ScanOutcome outcomeValue) { outcome_ = outcomeValue; }

private:
    QList<DuplicateGroup> duplicateGroups_;
    ScanSummaryDetails scanSummaryDetails_;
    ScanOutcome outcome_;
};
