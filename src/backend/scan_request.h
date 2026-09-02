#pragma once

#include "types/scan_type.h"
#include <QStringList>

/// @brief Immutable value object describing one requested scan operation.
///
/// The request owns a snapshot of the selected roots so later model changes cannot affect an already scheduled scan.
/// It does not validate paths; filesystem validation occurs during file-tree enumeration.
class ScanRequest
{
public:
    /// Creates a request by taking ownership of the supplied root list.
    /// @param[in] rootDirectoryPaths Root directory trees to scan.
    /// @param[in] scanType Duplicate-detection workflow to execute.
    ScanRequest(QStringList rootDirectoryPaths, const ScanType scanType)
        : rootDirectoryPaths_(std::move(rootDirectoryPaths)), scanType_(scanType) {}

    /// Returns the owned root directory paths in request order.
    [[nodiscard]] const QStringList& getRootDirectoryPaths() const { return rootDirectoryPaths_; }
    /// Returns the requested duplicate-detection workflow.
    [[nodiscard]] ScanType getScanType() const { return scanType_; }

private:
    const QStringList rootDirectoryPaths_;
    const ScanType scanType_;
};
