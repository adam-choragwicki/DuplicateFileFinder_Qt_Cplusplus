#include "path_utils.h"
#include "path_comparison_policy.h"

#include <QDir>

namespace
{
    enum class DirectoryRelationship
    {
        Unrelated,
        SameDirectory,
        Subdirectory
    };

    [[nodiscard]] DirectoryRelationship determineDirectoryRelationship(const QString& directoryPath, const QString& possibleParentDirectoryPath)
    {
        const QString normalizedDirectoryPath = PathUtils::normalizeDirectoryPath(directoryPath);
        const QString normalizedPossibleParentDirectoryPath = PathUtils::normalizeDirectoryPath(possibleParentDirectoryPath);

        if (normalizedDirectoryPath.compare(normalizedPossibleParentDirectoryPath, PathComparisonPolicy::caseSensitivity) == 0)
        {
            return DirectoryRelationship::SameDirectory;
        }

        QString possibleParentDirectoryPathPrefix = normalizedPossibleParentDirectoryPath;

        if (!possibleParentDirectoryPathPrefix.endsWith('/'))
        {
            possibleParentDirectoryPathPrefix.append('/');
        }

        return normalizedDirectoryPath.startsWith(possibleParentDirectoryPathPrefix, PathComparisonPolicy::caseSensitivity)
                   ? DirectoryRelationship::Subdirectory
                   : DirectoryRelationship::Unrelated;
    }
}

QString PathUtils::normalizeDirectoryPath(const QString& directoryPath)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(QDir(directoryPath).absolutePath()));
}

bool PathUtils::isSameDirectoryOrSubdirectoryOf(const QString& directoryPath, const QString& possibleParentDirectoryPath)
{
    return determineDirectoryRelationship(directoryPath, possibleParentDirectoryPath) != DirectoryRelationship::Unrelated;
}

bool PathUtils::isSubdirectoryOf(const QString& directoryPath, const QString& possibleParentDirectoryPath)
{
    return determineDirectoryRelationship(directoryPath, possibleParentDirectoryPath) == DirectoryRelationship::Subdirectory;
}
