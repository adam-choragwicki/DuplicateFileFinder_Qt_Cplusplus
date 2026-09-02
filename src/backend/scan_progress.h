#pragma once

#include "types/scan_phase.h"

#include <functional>
#include <optional>

/// @brief Describes the latest observable progress of an active scan stage.
struct ScanProgress
{
    /// Concrete workflow phase currently being executed.
    ScanPhase scanPhase;
    /// Number of files processed within the current phase.
    quint64 processedFilesCount{};

    /// Total files to process in the current phase, or std::nullopt while enumeration has an unknown total.
    /// Once present, the UI can switch from indeterminate progress to an X/Y indicator.
    std::optional<quint64> totalFilesCount;
};

/// Callback used by workflows to publish synchronous progress snapshots to their caller.
using ScanProgressCallback = std::function<void(const ScanProgress&)>;
