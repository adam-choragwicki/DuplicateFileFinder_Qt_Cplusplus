#pragma once

#include "duplicate_group.h"
#include "scan_outcome.h"
#include "scan_summary.h"

#include <QList>

class ScanResult
{
public:
    ScanResult(QList<DuplicateGroup> duplicateGroups, const ScanOutcome outcome, ScanSummary scanSummary)
        : duplicateGroups_(std::move(duplicateGroups)),
          scanSummary_(std::move(scanSummary)),
          outcome_(outcome)
    {}

    [[nodiscard]] const QList<DuplicateGroup>& getDuplicateGroups() const { return duplicateGroups_; }
    [[nodiscard]] const ScanSummary& getScanSummary() const { return scanSummary_; }
    [[nodiscard]] ScanOutcome getOutcome() const { return outcome_; }
    [[nodiscard]] bool isScanCancelled() const { return outcome_ == ScanOutcome::Cancelled; }

    void setOutcome(const ScanOutcome outcomeValue) { outcome_ = outcomeValue; }

private:
    QList<DuplicateGroup> duplicateGroups_;
    ScanSummary scanSummary_;
    ScanOutcome outcome_;
};
