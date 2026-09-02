#pragma once

#include "file_tree_enumeration_result.h"
#include "types/file_record.h"

#include <QStringList>

#include <functional>
#include <stop_token>

/// @brief Recursively enumerates readable files below one or more root directories.
///
/// Repeated roots and roots nested below another supplied root are enumerated only once. Hidden and system entries are
/// included. Each regular file is opened for reading and its size is obtained before a FileRecord is passed to the
/// visitor; inaccessible files are skipped and recorded in the result metrics.
///
/// Enumeration is synchronous, observes the supplied stop token throughout traversal, and does not guarantee the order
/// in which files are visited.
class FileTreeEnumerator final
{
public:
    /// Callback invoked synchronously once for each successfully scanned file.
    using FileVisitor = std::function<void(FileRecord)>;

    /**
     * @brief Recursively enumerates all non-redundant root directory trees.
     *
     * Valid roots are canonicalized for comparison so repeated roots and roots covered by another supplied root do not
     * cause files to be visited more than once. Invalid roots remain in input order and terminate enumeration when
     * reached. Metrics from roots processed before cancellation or an invalid root are preserved in the result.
     *
     * @param[in] rootDirectoryPaths Root directories to enumerate.
     * @param[in] stopToken Cancellation token checked before and during traversal.
     * @param[in] fileVisitor Non-empty callback that receives each successfully scanned file by value.
     * @return Final status and metrics accumulated before enumeration ended.
     */
    [[nodiscard]] static FileTreeEnumerationResult enumerateRecursively(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const FileVisitor& fileVisitor);

private:
    /// Validated representation used to compare a supplied root with the other roots.
    struct RootDirectory
    {
        /// Path exactly as supplied by the caller and used to start enumeration.
        QString originalPath;
        /// Canonical path using Qt's '/' separator format; empty when the root is invalid.
        QString normalizedPath;
        /// Whether the supplied path identifies an existing, readable directory.
        bool isValid;
    };

    /// Removes repeated valid roots and valid roots nested below another supplied root.
    /// Invalid roots are retained so enumeration can report them at their position in the input sequence.
    /// @param[in] rootDirectoryPaths Root paths to validate and compare.
    /// @return Original root paths that still require enumeration or validation.
    [[nodiscard]] static QStringList getNonOverlappingRootDirectoryPaths(const QStringList& rootDirectoryPaths);

    /// Recursively enumerates one root and invokes `fileVisitor` for each successfully scanned file.
    /// @param[in] rootDirectoryPath Root directory to validate and enumerate.
    /// @param[in] stopToken Cancellation token checked before and during traversal.
    /// @param[in] fileVisitor Callback that receives each successfully scanned file by value.
    /// @return Final status and metrics for this root directory.
    [[nodiscard]] static FileTreeEnumerationResult enumerateSingleRootRecursively(const QString& rootDirectoryPath, const std::stop_token& stopToken, const FileVisitor& fileVisitor);
};
