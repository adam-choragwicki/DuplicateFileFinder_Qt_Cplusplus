#pragma once

enum class ScanOutcome
{
    CompletedWithDuplicates,
    CompletedWithoutDuplicates,
    NoFilesFound,
    Cancelled,
    Failed
};
