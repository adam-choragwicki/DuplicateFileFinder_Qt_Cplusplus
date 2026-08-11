#pragma once

#include "types/scan_type.h"
#include <QStringList>

class ScanRequest
{
public:
    ScanRequest(QStringList rootDirectoryPaths, const ScanType scanType)
        : rootDirectoryPaths_(std::move(rootDirectoryPaths)), scanType_(scanType) {}

    [[nodiscard]] const QStringList& getRootDirectoryPaths() const { return rootDirectoryPaths_; }
    [[nodiscard]] ScanType getScanType() const { return scanType_; }

private:
    const QStringList rootDirectoryPaths_;
    const ScanType scanType_;
};
