#include "model.h"
#include "path_comparison_policy.h"
#include "path_utils.h"

#include <utility>

Model::AddScanDirectoryResult::AddScanDirectoryResult(const AddScanDirectoryOutcome addScanDirectoryOutcome, QString normalizedDirectoryPath, QString coveringDirectoryPath)
    : addScanDirectoryOutcome_(addScanDirectoryOutcome),
      normalizedDirectoryPath_(std::move(normalizedDirectoryPath)),
      coveringDirectoryPath_(std::move(coveringDirectoryPath))
{}

Model::Model(QObject* parent) : QObject(parent) {}

bool Model::hasDuplicateResults() const
{
    return latestScanResult_.has_value()
           && latestScanResult_->getOutcome() == ScanOutcome::CompletedWithDuplicates
           && !latestScanResult_->getDuplicateGroups().isEmpty();
}

Model::AddScanDirectoryResult Model::addScanDirectory(const QString& directoryPath)
{
    if (directoryPath.trimmed().isEmpty())
    {
        return AddScanDirectoryResult(AddScanDirectoryOutcome::InvalidPath, {}, {});
    }

    const QString normalizedDirectoryPath = PathUtils::normalizeDirectoryPath(directoryPath);

    for (const QString& existingDirectoryPath: scanDirectoryPaths_)
    {
        if (PathUtils::isSameDirectoryOrSubdirectoryOf(normalizedDirectoryPath, existingDirectoryPath))
        {
            return AddScanDirectoryResult(AddScanDirectoryOutcome::AlreadyIncluded,
                                          normalizedDirectoryPath,
                                          existingDirectoryPath);
        }
    }

    // Replace any narrower roots with the broader root being added. For example, adding /data makes an existing /data/photos root redundant because scanning /data already includes that directory.
    scanDirectoryPaths_.removeIf([&normalizedDirectoryPath](const QString& existingDirectoryPath)
    {
        return PathUtils::isSameDirectoryOrSubdirectoryOf(existingDirectoryPath, normalizedDirectoryPath);
    });

    scanDirectoryPaths_.append(normalizedDirectoryPath);

    emit scanDirectoryPathsChanged(scanDirectoryPaths_);
    return AddScanDirectoryResult(AddScanDirectoryOutcome::Added, normalizedDirectoryPath, {});
}

bool Model::removeScanDirectory(const QString& directoryPath)
{
    if (directoryPath.trimmed().isEmpty())
    {
        return false;
    }

    const QString normalizedDirectoryPath = PathUtils::normalizeDirectoryPath(directoryPath);

    const qsizetype directoryIndex = scanDirectoryPaths_.indexOf(normalizedDirectoryPath,
                                                                 0,
                                                                 PathComparisonPolicy::caseSensitivity);

    if (directoryIndex < 0)
    {
        return false;
    }

    scanDirectoryPaths_.removeAt(directoryIndex);
    emit scanDirectoryPathsChanged(scanDirectoryPaths_);
    return true;
}

void Model::setScanType(const ScanType scanType)
{
    if (scanType_ == scanType)
    {
        return;
    }

    scanType_ = scanType;
    emit scanTypeChanged(scanType_);
}

Model::ScanStartOutcome Model::beginScan()
{
    clearScanResult();

    if (scanState_ == ScanState::Scanning)
    {
        return ScanStartOutcome::AlreadyScanning;
    }

    if (scanDirectoryPaths_.isEmpty())
    {
        return ScanStartOutcome::NoDirectoriesSelected;
    }

    scanState_ = ScanState::Scanning;
    return ScanStartOutcome::Started;
}

void Model::completeScan(const ScanResult& scanResult)
{
    scanState_ = ScanState::Idle;
    latestScanResult_ = scanResult;

    emit scanResultChanged();
}

void Model::markScanCancelled()
{
    scanState_ = ScanState::Idle;
}

void Model::clearScanResult()
{
    if (!latestScanResult_.has_value())
    {
        return;
    }

    latestScanResult_.reset();
    emit scanResultChanged();
}
