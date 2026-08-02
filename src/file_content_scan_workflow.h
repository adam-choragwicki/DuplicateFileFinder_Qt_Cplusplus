#pragma once

#include "scan_workflow.h"

class FileContentScanWorkflow final : public ScanWorkflow
{
public:
    using FileHasher = std::function<std::optional<QByteArray>(const FileRecord& file, const std::stop_token& stopToken)>;

    FileContentScanWorkflow();

    [[nodiscard]] ScanResult execute(const QString& rootDirectoryPath, const std::stop_token& stopToken) const override;

private:
    FileHasher fileHasher_;

    [[nodiscard]] static std::optional<QByteArray> calculateFileHash(const FileRecord& file, const std::stop_token& stopToken);
};
