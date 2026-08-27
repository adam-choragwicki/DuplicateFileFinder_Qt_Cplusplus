#pragma once

#include "types/scan_result.h"

namespace test_helpers
{
    /// @brief Creates a completed scan result suitable for testing result-dependent application behavior.
    ///
    /// @return A file-name-based scan result containing one duplicate group whose two files have the same
    /// name and reside in different directories.
    [[nodiscard]] inline ScanResult createScanResultWithDuplicates()
    {
        DuplicateGroup duplicateGroup;
        duplicateGroup.addFile(FileRecord{QStringLiteral("duplicate.txt"), QStringLiteral("C:/first"), 8});
        duplicateGroup.addFile(FileRecord{QStringLiteral("duplicate.txt"), QStringLiteral("C:/second"), 8});

        return ScanResult{{duplicateGroup},
                          ScanOutcome::CompletedWithDuplicates,
                          FileNameScanSummary{}};
    }
}
