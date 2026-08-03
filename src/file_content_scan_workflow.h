#pragma once

#include "scan_workflow.h"

class FileContentScanWorkflow final : public ScanWorkflow
{
public:
    using FileHasher = std::function<std::optional<QByteArray>(const FileRecord& file, const std::stop_token& stopToken)>; // TODO move to separate class

    FileContentScanWorkflow();

    [[nodiscard]] ScanResult execute(const QString& rootDirectoryPath, const std::stop_token& stopToken) const override;

private:
    FileHasher fileHasher_; // TODO move to separate class

    [[nodiscard]] static std::optional<QByteArray> calculateFileHash(const FileRecord& file, const std::stop_token& stopToken); // TODO move to separate class
};
