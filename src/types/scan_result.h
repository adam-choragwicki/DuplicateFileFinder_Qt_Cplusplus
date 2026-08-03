#pragma once

#include "duplicate_group.h"
#include "scan_summary/file_name_scan_summary.h"
#include "scan_summary/file_content_scan_summary.h"
#include "scan_outcome.h"

#include <QList>

using ScanSummaryDetails = std::variant<FileNameScanSummary, FileContentScanSummary>;

class ScanResult
{
public:
    ScanResult(QList<DuplicateGroup> duplicateGroups, const ScanOutcome outcome, FileNameScanSummary scanSummary)
        : duplicateGroups_(std::move(duplicateGroups)),
          scanSummaryDetails_(std::move(scanSummary)),
          outcome_(outcome)
    {}

    ScanResult(QList<DuplicateGroup> duplicateGroups, const ScanOutcome outcome, FileContentScanSummary scanSummary)
        : duplicateGroups_(std::move(duplicateGroups)),
          scanSummaryDetails_(std::move(scanSummary)),
          outcome_(outcome)
    {}

    [[nodiscard]] const QList<DuplicateGroup>& getDuplicateGroups() const { return duplicateGroups_; }
    [[nodiscard]] const ScanSummaryDetails& getScanSummaryDetails() const { return scanSummaryDetails_; }
    [[nodiscard]] ScanOutcome getOutcome() const { return outcome_; }
    [[nodiscard]] bool isScanCancelled() const { return outcome_ == ScanOutcome::Cancelled; }

    void setOutcome(const ScanOutcome outcomeValue) { outcome_ = outcomeValue; }

private:
    QList<DuplicateGroup> duplicateGroups_;
    ScanSummaryDetails scanSummaryDetails_;
    ScanOutcome outcome_;
};
