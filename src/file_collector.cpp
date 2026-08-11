#include "file_collector.h"

#include <QDirIterator>
#include <QFileInfo>

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

    for (const QString& rootDirectoryPath: rootDirectoryPaths)
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

        metrics.incrementScannedFilesCount();
        metrics.addToTotalScannedBytes(fileInfo.size());
        fileVisitor(FileRecord{fileInfo.fileName(), fileInfo.absolutePath(), fileInfo.size()});
    }

    return {FileCollectionStatus::Completed, metrics};
}
