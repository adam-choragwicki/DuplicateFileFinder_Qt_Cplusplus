#pragma once

#include "duplicate_group.h"

#include <QList>

class ScanResult
{
public:
    [[nodiscard]] const QList<DuplicateGroup>& getDuplicateGroups() const { return duplicateGroups; }
    [[nodiscard]] bool isScanCancelled() const { return scanCancelled; }

    void appendDuplicateGroup(const DuplicateGroup& duplicateGroup) { duplicateGroups.push_back(duplicateGroup); }

    void setScanCancelled() { scanCancelled = true; }

private:
    QList<DuplicateGroup> duplicateGroups;
    bool scanCancelled{};
};
