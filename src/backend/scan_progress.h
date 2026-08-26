#pragma once

#include "types/scan_phase.h"

#include <functional>
#include <optional>

struct ScanProgress
{
    ScanPhase scanPhase;
    quint64 processedFilesCount{};

    // The number of files cannot be known during a single-pass directory enumeration.
    // Once it becomes known, the UI can switch from an indeterminate indicator to X/Y progress.
    std::optional<quint64> totalFilesCount;
};

using ScanProgressCallback = std::function<void(const ScanProgress&)>;
