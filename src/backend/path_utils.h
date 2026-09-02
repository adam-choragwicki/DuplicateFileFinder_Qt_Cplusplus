#pragma once

#include <QString>

/// Provides platform-aware lexical operations for directory paths.
namespace PathUtils
{
    /// Returns an absolute, cleaned directory path using Qt's '/' separator format without resolving symbolic links.
    /// @param[in] directoryPath Absolute or relative directory path to normalize.
    /// @return Normalized absolute path suitable for lexical storage and comparison.
    [[nodiscard]] QString normalizeDirectoryPath(const QString& directoryPath);

    /**
     * @brief Checks whether a directory is covered by a possible parent directory.
     *
     * Both paths are normalized before comparison and use the platform-appropriate case sensitivity.
     *
     * @param[in] directoryPath Path of the directory whose relationship is being tested.
     * @param[in] possibleParentDirectoryPath Path of the directory that may be the same directory or an ancestor.
     * @return true if directoryPath is equal to or located below possibleParentDirectoryPath; otherwise false.
     */
    [[nodiscard]] bool isSameDirectoryOrSubdirectoryOf(const QString& directoryPath, const QString& possibleParentDirectoryPath);

    /**
     * @brief Checks whether a directory is strictly below a possible parent directory.
     *
     * Both paths are normalized before comparison and use the platform-appropriate case sensitivity.
     *
     * @param[in] directoryPath Path of the directory whose relationship is being tested.
     * @param[in] possibleParentDirectoryPath Path of the directory that may be an ancestor.
     * @return true if directoryPath is located below, but is not equal to, possibleParentDirectoryPath; otherwise false.
     */
    [[nodiscard]] bool isSubdirectoryOf(const QString& directoryPath, const QString& possibleParentDirectoryPath);
}
