#pragma once

#include "types/scan_type.h"
#include <QDir>

class ScanRequest
{
public:
    ScanRequest(const QString& rootDirectoryPath, const ScanType scanType) : rootDirectoryPath_(rootDirectoryPath), scanType_(scanType) {}

    [[nodiscard]] QString getRootDirectoryPath() const { return rootDirectoryPath_; }
    [[nodiscard]] ScanType getScanType() const { return scanType_; }

private:
    const QString rootDirectoryPath_;
    const ScanType scanType_;
};
