#pragma once

#include "abstract_scan_workflow.h"

/// @brief Finds duplicate candidates by grouping enumerated files according to their comparison names.
///
/// The workflow reports progress for enumeration, name grouping, and result construction. Depending on its configured
/// policy, the comparison key can omit the final file extension; matching is case-insensitive.
class FileNameScanWorkflow final : public AbstractScanWorkflow
{
public:
    /// Executes a complete file-name-based scan over the supplied roots.
    /// @param[in] rootDirectoryPaths Root directory trees to scan.
    /// @param[in] stopToken Cancellation token observed throughout the workflow.
    /// @param[in] scanProgressCallback Callback receiving synchronous progress updates from the worker thread.
    /// @return Duplicate groups, final outcome, and file-name scan summary.
    [[nodiscard]] ScanResult execute(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const ScanProgressCallback& scanProgressCallback) const override;
};
