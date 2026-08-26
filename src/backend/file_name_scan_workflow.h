#pragma once

#include "scan_workflow.h"

class FileNameScanWorkflow final : public ScanWorkflow
{
public:
    [[nodiscard]] ScanResult execute(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const ScanProgressCallback& scanProgressCallback) const override;
};
