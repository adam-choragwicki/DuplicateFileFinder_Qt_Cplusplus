#include "file_tree_enumerator.h"

#include "path_comparison_policy.h"
#include "path_utils.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

FileTreeEnumerationResult FileTreeEnumerator::enumerateRecursively(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const FileVisitor& fileVisitor)
{
    FileTreeEnumerationMetrics combinedFileTreeEnumerationMetrics;

    if (stopToken.stop_requested())
    {
        return {FileTreeEnumerationStatus::Cancelled, combinedFileTreeEnumerationMetrics};
    }

    if (rootDirectoryPaths.isEmpty())
    {
        qCritical() << "Cannot scan because no root directories were provided";
        return {FileTreeEnumerationStatus::InvalidRootDirectory, combinedFileTreeEnumerationMetrics};
    }

    // Avoid enumerating the same tree more than once: {/data, /data/photos, /data} is reduced to {/data},
    // while non-overlapping roots such as {/data, /backup} are both retained.
    const QStringList nonOverlappingRootDirectoryPaths = getNonOverlappingRootDirectoryPaths(rootDirectoryPaths);

    for (const QString& rootDirectoryPath: nonOverlappingRootDirectoryPaths)
    {
        const FileTreeEnumerationResult rootEnumerationResult = enumerateSingleRootRecursively(rootDirectoryPath, stopToken, fileVisitor);
        combinedFileTreeEnumerationMetrics.merge(rootEnumerationResult.getMetrics());

        if (rootEnumerationResult.getStatus() != FileTreeEnumerationStatus::Completed)
        {
            return {rootEnumerationResult.getStatus(), combinedFileTreeEnumerationMetrics};
        }

        if (stopToken.stop_requested())
        {
            return {FileTreeEnumerationStatus::Cancelled, combinedFileTreeEnumerationMetrics};
        }
    }

    return {FileTreeEnumerationStatus::Completed, combinedFileTreeEnumerationMetrics};
}

QStringList FileTreeEnumerator::getNonOverlappingRootDirectoryPaths(const QStringList& rootDirectoryPaths)
{
    QList<RootDirectory> rootDirectories;
    rootDirectories.reserve(rootDirectoryPaths.size());

    // Build comparison records for all supplied roots. Canonical paths allow aliases such as a symbolic link and its
    // target to be recognized as the same tree, while original paths are retained for enumeration and error reporting.
    for (const QString& rootDirectoryPath: rootDirectoryPaths)
    {
        const QFileInfo rootDirectoryInfo(rootDirectoryPath);
        const bool isValid = rootDirectoryInfo.exists()
                             && rootDirectoryInfo.isDir()
                             && rootDirectoryInfo.isReadable();
        const QString normalizedPath = isValid
                                           ? QDir::fromNativeSeparators(rootDirectoryInfo.canonicalFilePath())
                                           : QString{};

        rootDirectories.append({.originalPath = rootDirectoryPath, .normalizedPath = normalizedPath, .isValid = isValid});
    }

    QStringList nonOverlappingPaths;

    // Preserve input order while retaining only roots that are not covered by another valid root.
    for (qsizetype rootIndex = 0; rootIndex < rootDirectories.size(); ++rootIndex)
    {
        const RootDirectory& rootDirectory = rootDirectories.at(rootIndex);

        if (!rootDirectory.isValid)
        {
            // Invalid roots must still reach enumeration so the workflow reports a failure at their input position.
            nonOverlappingPaths.append(rootDirectory.originalPath);
            continue;
        }

        bool isCoveredByAnotherRoot = false;

        // Compare with every other root because a broader root may occur later in the input list.
        for (qsizetype otherRootIndex = 0; otherRootIndex < rootDirectories.size(); ++otherRootIndex)
        {
            if (rootIndex == otherRootIndex || !rootDirectories.at(otherRootIndex).isValid)
            {
                continue;
            }

            const QString& otherRootPath = rootDirectories.at(otherRootIndex).normalizedPath;
            const bool isRepeatedRoot = rootDirectory.normalizedPath.compare(otherRootPath, PathComparisonPolicy::caseSensitivity) == 0;

            // Keep only the first occurrence of an identical root. A nested root is redundant regardless of whether
            // its covering parent occurs before or after it.
            if ((isRepeatedRoot && otherRootIndex < rootIndex)
                || PathUtils::isSubdirectoryOf(rootDirectory.normalizedPath, otherRootPath))
            {
                isCoveredByAnotherRoot = true;
                break;
            }
        }

        if (!isCoveredByAnotherRoot)
        {
            nonOverlappingPaths.append(rootDirectory.originalPath);
        }
    }

    return nonOverlappingPaths;
}

FileTreeEnumerationResult FileTreeEnumerator::enumerateSingleRootRecursively(const QString& rootDirectoryPath, const std::stop_token& stopToken, const FileVisitor& fileVisitor)
{
    FileTreeEnumerationMetrics metrics;

    if (stopToken.stop_requested())
    {
        return {FileTreeEnumerationStatus::Cancelled, metrics};
    }

    const QFileInfo rootDirectoryInfo(rootDirectoryPath);

    if (!rootDirectoryInfo.exists() || !rootDirectoryInfo.isDir() || !rootDirectoryInfo.isReadable())
    {
        qCritical() << "Cannot scan directory:" << rootDirectoryPath;
        return {FileTreeEnumerationStatus::InvalidRootDirectory, metrics};
    }

    metrics.incrementScannedDirectoriesCount();

    QDirIterator iterator(rootDirectoryPath,
                          QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);

    while (iterator.hasNext())
    {
        if (stopToken.stop_requested())
        {
            return {FileTreeEnumerationStatus::Cancelled, metrics};
        }

        iterator.next();
        const QFileInfo fileInfo = iterator.fileInfo();

        if (fileInfo.isDir())
        {
            metrics.incrementScannedDirectoriesCount();
            continue;
        }

        if (!fileInfo.isFile())
        {
            continue;
        }

        QFile inputFile(fileInfo.absoluteFilePath());

        if (!inputFile.open(QIODevice::ReadOnly))
        {
            metrics.incrementProblematicFilesCount();
            qWarning() << "Skipping file that cannot be opened for reading:" << inputFile.fileName() << '-' << inputFile.errorString();
            continue;
        }

        const qint64 fileSizeBytes = inputFile.size();

        if (fileSizeBytes < 0)
        {
            metrics.incrementProblematicFilesCount();
            qWarning() << "Skipping file which size cannot be read:" << inputFile.fileName() << '-' << inputFile.errorString();
            continue;
        }

        metrics.incrementScannedFilesCount();
        metrics.addToTotalScannedBytes(fileSizeBytes);
        fileVisitor(FileRecord{fileInfo.fileName(), fileInfo.absolutePath(), fileSizeBytes});
    }

    return {FileTreeEnumerationStatus::Completed, metrics};
}
