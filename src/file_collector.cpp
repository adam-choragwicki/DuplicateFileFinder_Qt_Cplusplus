#include "file_collector.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

Qt::CaseSensitivity FileCollector::getScanRootPathCaseSensitivity()
{
#if defined(Q_OS_WIN)
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

bool FileCollector::isProperSubdirectoryOf(const QString& directoryPath, const QString& possibleParentPath)
{
    QString parentPathPrefix = possibleParentPath;

    if (!parentPathPrefix.endsWith('/'))
    {
        parentPathPrefix.append('/');
    }

    return directoryPath.startsWith(parentPathPrefix, getScanRootPathCaseSensitivity());
}

QStringList FileCollector::getNonOverlappingRootDirectoryPaths(const QStringList& rootDirectoryPaths)
{
    QList<RootDirectory> rootDirectories;
    rootDirectories.reserve(rootDirectoryPaths.size());

    for (const QString& rootDirectoryPath: rootDirectoryPaths)
    {
        const QFileInfo rootDirectoryInfo(rootDirectoryPath);
        const bool isValid = rootDirectoryInfo.exists()
                             && rootDirectoryInfo.isDir()
                             && rootDirectoryInfo.isReadable();
        const QString normalizedPath = isValid
                                           ? QDir::fromNativeSeparators(rootDirectoryInfo.canonicalFilePath())
                                           : QString{};

        rootDirectories.append({rootDirectoryPath, normalizedPath, isValid});
    }

    QStringList nonOverlappingPaths;

    for (qsizetype rootIndex = 0; rootIndex < rootDirectories.size(); ++rootIndex)
    {
        const RootDirectory& rootDirectory = rootDirectories.at(rootIndex);

        if (!rootDirectory.isValid)
        {
            // Invalid roots must still reach collection so the workflow reports a failure at their input position.
            nonOverlappingPaths.append(rootDirectory.originalPath);
            continue;
        }

        bool isCoveredByAnotherRoot = false;

        for (qsizetype otherRootIndex = 0; otherRootIndex < rootDirectories.size(); ++otherRootIndex)
        {
            if (rootIndex == otherRootIndex || !rootDirectories.at(otherRootIndex).isValid)
            {
                continue;
            }

            const QString& otherRootPath = rootDirectories.at(otherRootIndex).normalizedPath;
            const bool isRepeatedRoot = rootDirectory.normalizedPath.compare(otherRootPath, getScanRootPathCaseSensitivity()) == 0;

            if ((isRepeatedRoot && otherRootIndex < rootIndex)
                || isProperSubdirectoryOf(rootDirectory.normalizedPath, otherRootPath))
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

FileCollectionResult FileCollector::collectRecursively(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const FileVisitor& fileVisitor)
{
    FileCollectionMetrics combinedMetrics;

    if (stopToken.stop_requested())
    {
        return {FileCollectionStatus::Cancelled, combinedMetrics};
    }

    if (rootDirectoryPaths.isEmpty())
    {
        qCritical() << "Cannot scan because no root directories were provided";
        return {FileCollectionStatus::InvalidRootDirectory, combinedMetrics};
    }

    const QStringList nonOverlappingRootDirectoryPaths = getNonOverlappingRootDirectoryPaths(rootDirectoryPaths);

    for (const QString& rootDirectoryPath: nonOverlappingRootDirectoryPaths)
    {
        const FileCollectionResult rootCollectionResult = collectSingleRootRecursively(rootDirectoryPath, stopToken, fileVisitor);
        combinedMetrics.mergeFileCollectionMetrics(rootCollectionResult.getMetrics());

        if (rootCollectionResult.getStatus() != FileCollectionStatus::Completed)
        {
            return {rootCollectionResult.getStatus(), combinedMetrics};
        }

        if (stopToken.stop_requested())
        {
            return {FileCollectionStatus::Cancelled, combinedMetrics};
        }
    }

    return {FileCollectionStatus::Completed, combinedMetrics};
}

FileCollectionResult FileCollector::collectSingleRootRecursively(const QString& rootDirectoryPath, const std::stop_token& stopToken, const FileVisitor& fileVisitor)
{
    FileCollectionMetrics metrics;

    if (stopToken.stop_requested())
    {
        return {FileCollectionStatus::Cancelled, metrics};
    }

    const QFileInfo rootDirectoryInfo(rootDirectoryPath);

    if (!rootDirectoryInfo.exists() || !rootDirectoryInfo.isDir() || !rootDirectoryInfo.isReadable())
    {
        qCritical() << "Cannot scan directory:" << rootDirectoryPath;
        return {FileCollectionStatus::InvalidRootDirectory, metrics};
    }

    metrics.incrementScannedDirectoriesCount();

    QDirIterator iterator(rootDirectoryPath,
                          QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);

    while (iterator.hasNext())
    {
        if (stopToken.stop_requested())
        {
            return {FileCollectionStatus::Cancelled, metrics};
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
            qWarning() << "Skipping file that cannot be opened for reading:" << inputFile.fileName() << '-'
                       << inputFile.errorString();
            continue;
        }

        const qint64 fileSizeBytes = inputFile.size();

        if (fileSizeBytes < 0)
        {
            metrics.incrementProblematicFilesCount();
            qWarning() << "Skipping file which size cannot be read:" << inputFile.fileName() << '-'
                       << inputFile.errorString();
            continue;
        }

        metrics.incrementScannedFilesCount();
        metrics.addToTotalScannedBytes(fileSizeBytes);
        fileVisitor(FileRecord{fileInfo.fileName(), fileInfo.absolutePath(), fileSizeBytes});
    }

    return {FileCollectionStatus::Completed, metrics};
}
