#pragma once

/// @brief Identifies the condition that ended file-tree enumeration.
enum class FileTreeEnumerationStatus
{
    /// Every non-redundant root directory was enumerated successfully.
    Completed,
    /// Enumeration stopped after the supplied stop token reported a cancellation request.
    Cancelled,
    /// No roots were supplied, or a root does not identify an existing, readable directory.
    InvalidRootDirectory
};
