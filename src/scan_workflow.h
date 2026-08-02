#pragma once

#include <QString>
#include <stop_token>
#include "types/scan_result.h"

class ScanWorkflow
{
public:
    virtual ~ScanWorkflow() = default;

    [[nodiscard]] virtual ScanResult execute(const QString& rootDirectoryPath, const std::stop_token& stopToken) const = 0;
};
