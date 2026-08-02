#pragma once

#include "scan_workflow.h"

class FileContentScanWorkflow final : public ScanWorkflow
{
public:
    [[nodiscard]] ScanResult execute(const QString& rootDirectoryPath, const std::stop_token& stopToken) const override;
};
