#include "file_name_scan_workflow.h"
#include "scan_summary/file_name_scan_summary.h"
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>

namespace
{
    // Set to false to group files by the name before their final extension:
    // "report.txt" and "report.pdf" will then belong to the same duplicate group.
    // Set to true to require the complete file name - including its extension - to match.
    constexpr bool includeFileExtensionInFileNameScan = false;

    [[nodiscard]] QString getFileNameComparisonKey(const QString& fileName)
    {
        if constexpr (includeFileExtensionInFileNameScan)
        {
            return fileName;
        }

        const qsizetype extensionSeparatorIndex = fileName.lastIndexOf('.');
        if (extensionSeparatorIndex < 0)
        {
            return fileName;
        }

        // On Linux a leading dot normally marks a hidden file rather than an extension. Preserve names such as ".bashrc", but still remove the final extension from ".config.json".
        qsizetype firstNonDotCharacterIndex = 0;
        while (firstNonDotCharacterIndex < fileName.size() && fileName.at(firstNonDotCharacterIndex) == '.')
        {
            ++firstNonDotCharacterIndex;
        }

        if (extensionSeparatorIndex < firstNonDotCharacterIndex)
        {
            return fileName;
        }

        // Only the suffix after the final dot is treated as the extension. For example, "archive.tar.gz" produces the comparison key "archive.tar".
        return fileName.left(extensionSeparatorIndex);
    }
}

ScanResult FileNameScanWorkflow::execute(const QStringList& rootDirectoryPaths, const std::stop_token& stopToken, const ScanProgressCallback& scanProgressCallback) const
{
    QElapsedTimer durationTimer;
    durationTimer.start();

    QList<DuplicateGroup> duplicateGroups;
    ScanOutcome outcome = ScanOutcome::Failed;
    QHash<QString, QList<FileRecord>> filesByName;
    quint64 enumeratedFilesCount = 0;

    qInfo() << "Started scan based on file name; include file extension in comparison =" << includeFileExtensionInFileNameScan;

    scanProgressCallback({.scanPhase = FileNameScanPhase::EnumeratingFiles, .processedFilesCount = 0, .totalFilesCount = std::nullopt});

    // Stage 1: Collecting files and grouping them by name
    const FileCollectionResult fileCollectionResult = FileCollector::collectRecursively(rootDirectoryPaths,
                                                                                        stopToken,
                                                                                        [&filesByName, &enumeratedFilesCount, &scanProgressCallback](FileRecord file)
                                                                                        {
                                                                                            // visitor collecting files and grouping them by name
                                                                                            const QString fileNameComparisonKey = getFileNameComparisonKey(file.getFileName());
                                                                                            filesByName[fileNameComparisonKey].append(std::move(file));
                                                                                            ++enumeratedFilesCount;

                                                                                            scanProgressCallback({.scanPhase = FileNameScanPhase::EnumeratingFiles, .processedFilesCount = enumeratedFilesCount, .totalFilesCount = std::nullopt});
                                                                                        });

    if (fileCollectionResult.getStatus() == FileCollectionStatus::InvalidRootDirectory)
    {
        outcome = ScanOutcome::Failed;
    }
    else if (fileCollectionResult.getStatus() == FileCollectionStatus::Cancelled || stopToken.stop_requested())
    {
        outcome = ScanOutcome::Cancelled;
    }
    else
    {
        quint64 processedFilesCount = 0;
        const quint64 collectedFilesCount = fileCollectionResult.getMetrics().getScannedFilesCount();

        scanProgressCallback({.scanPhase = FileNameScanPhase::GroupingFilesByName, .processedFilesCount = 0, .totalFilesCount = collectedFilesCount});

        // Stage 2: Grouping files with duplicate names
        for (auto iterator = filesByName.cbegin(); iterator != filesByName.cend(); ++iterator)
        {
            if (stopToken.stop_requested())
            {
                outcome = ScanOutcome::Cancelled;
                break;
            }

            if (iterator.value().size() < 2)
            {
                ++processedFilesCount;
                scanProgressCallback({.scanPhase = FileNameScanPhase::GroupingFilesByName, .processedFilesCount = processedFilesCount, .totalFilesCount = collectedFilesCount});
                continue;
            }

            DuplicateGroup duplicateGroup;

            for (const FileRecord& file: iterator.value())
            {
                duplicateGroup.addFile(file);
            }

            duplicateGroups.append(std::move(duplicateGroup));
            processedFilesCount += static_cast<quint64>(iterator.value().size());

            scanProgressCallback({.scanPhase = FileNameScanPhase::GroupingFilesByName, .processedFilesCount = processedFilesCount, .totalFilesCount = collectedFilesCount});
        }

        if (!stopToken.stop_requested())
        {
            outcome = classifySuccessfulScan(fileCollectionResult.getMetrics(), duplicateGroups);

            scanProgressCallback({.scanPhase = FileNameScanPhase::BuildingScanResult, .processedFilesCount = collectedFilesCount, .totalFilesCount = collectedFilesCount});
        }
    }

    const DuplicateGroupMetrics duplicateMetrics = calculateDuplicateGroupMetrics(duplicateGroups);

    FileNameScanSummary summary{
        QDateTime::currentDateTimeUtc(),
        std::chrono::milliseconds(durationTimer.elapsed()),
        fileCollectionResult.getMetrics().getScannedDirectoriesCount(),
        fileCollectionResult.getMetrics().getScannedFilesCount(),
        fileCollectionResult.getMetrics().getProblematicFilesCount(),
        fileCollectionResult.getMetrics().getTotalScannedBytes(),
        static_cast<quint64>(duplicateGroups.size()),
        duplicateMetrics.getFilesCount(),
        duplicateMetrics.getTotalBytes()
    };

    return ScanResult(std::move(duplicateGroups), outcome, std::move(summary));
}
