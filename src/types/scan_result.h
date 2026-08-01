#pragma once

#include "duplicate_group.h"
#include "scan_outcome.h"
#include "scan_summary.h"

#include <QList>

class ScanResult
{
public:
    [[nodiscard]] const QList<DuplicateGroup>& getDuplicateGroups() const { return duplicateGroups; }
    [[nodiscard]] const ScanSummary& getScanSummary() const { return scanSummary; }
    [[nodiscard]] ScanOutcome getOutcome() const { return outcome; }
    [[nodiscard]] bool isScanCancelled() const { return outcome == ScanOutcome::Cancelled; }

    void appendDuplicateGroup(const DuplicateGroup& duplicateGroup) { duplicateGroups.push_back(duplicateGroup); }
    void setScanSummary(const ScanSummary& summary) { scanSummary = summary; }
    void setOutcome(const ScanOutcome outcomeValue) { outcome = outcomeValue; }

private:
    QList<DuplicateGroup> duplicateGroups;
    ScanSummary scanSummary;
    ScanOutcome outcome{ScanOutcome::Failed};
};
