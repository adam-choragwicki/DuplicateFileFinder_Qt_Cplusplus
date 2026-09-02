#pragma once

#include "scan_summary/abstract_scan_summary.h"

/// @brief Concrete summary produced by a file-name-based scan.
///
/// File-name scans require no workflow-specific metrics beyond the fields supplied by AbstractScanSummary.
class FileNameScanSummary final : public AbstractScanSummary
{
public:
    /// Exposes the common summary constructors for file-name scan results.
    using AbstractScanSummary::AbstractScanSummary;
};
