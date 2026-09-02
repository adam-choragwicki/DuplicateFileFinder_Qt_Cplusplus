#pragma once

#include "types/file_record.h"

#include <stop_token>

/// @brief Describes the outcome of an exact file-content comparison.
enum class FileComparisonResult
{
    /// Both files were read completely and every byte matched.
    Equal,
    /// The current file contents differ in length or in at least one compared byte.
    Different,
    /// A stop request was observed before comparison completed.
    Cancelled,
    /// A file could not be opened or read.
    Failed
};

/// @brief Performs cancellation-aware, byte-for-byte comparison of two file records.
class FileComparator final
{
public:
    /// Reads both files in corresponding chunks until a byte difference, I/O failure, cancellation, or end of input.
    /// @param[in] firstFile First file to compare.
    /// @param[in] secondFile Second file to compare.
    /// @param[in] stopToken Cancellation token observed before and during file reads.
    /// @return Exact comparison outcome, cancellation, or an I/O failure.
    [[nodiscard]] static FileComparisonResult compareFileContents(const FileRecord& firstFile, const FileRecord& secondFile, const std::stop_token& stopToken);
};
