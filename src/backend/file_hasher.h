#pragma once

#include "types/file_record.h"
#include <stop_token>

class FileRecord;

/// @brief Provides cancellation-aware SHA-256 hashing for complete files and fixed-size prefixes.
class FileHasher
{
public:
    /// Calculates the SHA-256 digest of an entire file and verifies that the bytes read match its recorded size.
    /// @param[in] file File record identifying the input and its expected size.
    /// @param[in] stopToken Cancellation token observed before and during file reads.
    /// @return Digest on success, or std::nullopt after cancellation, an I/O error, or a size change.
    [[nodiscard]] static std::optional<QByteArray> calculateFullFileHash(const FileRecord& file, const std::stop_token& stopToken);

    /// Hashes up to the first 1 MiB of a file after verifying its recorded size.
    /// A matching result is only a reason to continue with a full hash; it is never proof that files are equal.
    /// @param[in] file File record identifying the input and its expected size.
    /// @param[in] stopToken Cancellation token observed before and during prefix reads.
    /// @return Prefix digest on success, or std::nullopt after cancellation, an I/O error, or a size change.
    [[nodiscard]] static std::optional<QByteArray> calculateFileSampleHash(const FileRecord& file, const std::stop_token& stopToken);
};
