#pragma once

#include "types/file_record.h"

#include <stop_token>

enum class FileComparisonResult
{
    Equal,
    Different,
    Cancelled,
    Failed
};

class FileComparator final
{
public:
    [[nodiscard]] static FileComparisonResult compareFileContents(const FileRecord& firstFile, const FileRecord& secondFile, const std::stop_token& stopToken);
};
