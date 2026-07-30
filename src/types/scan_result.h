#pragma once

#include "duplicate_group.h"
#include "scan_summary.h"

#include <QList>

class ScanResult
{
public:
    [[nodiscard]] const QList<DuplicateGroup>& getDuplicateGroups() const { return duplicateGroups; }
    [[nodiscard]] const ScanSummary& getScanSummary() const { return scanSummary; }
    [[nodiscard]] bool isScanCancelled() const { return scanCancelled; }

    void appendDuplicateGroup(const DuplicateGroup& duplicateGroup) { duplicateGroups.push_back(duplicateGroup); }
    void setScanSummary(const ScanSummary& summary) { scanSummary = summary; }

    void setScanCancelled() { scanCancelled = true; }

private:
    QList<DuplicateGroup> duplicateGroups;
    ScanSummary scanSummary;
    bool scanCancelled{};
};
