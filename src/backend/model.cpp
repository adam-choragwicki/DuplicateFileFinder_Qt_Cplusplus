#include "model.h"

#include <QDir>

#include <utility>

Model::Model(QObject* parent) : QObject(parent) {}

const QStringList& Model::getScanDirectoryPaths() const
{
    return scanDirectoryPaths_;
}

ScanType Model::getScanType() const
{
    return scanType_;
}

Model::ScanState Model::getScanState() const
{
    return scanState_;
}

const std::optional<ScanProgress>& Model::getScanProgress() const
{
    return scanProgress_;
}

const std::optional<ScanResult>& Model::getLatestScanResult() const
{
    return latestScanResult_;
}

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
        return {.outcome = AddScanDirectoryOutcome::InvalidPath, .normalizedDirectoryPath = {}, .coveringDirectoryPath = {}};
    }

    const QString normalizedDirectoryPath = normalizeDirectoryPath(directoryPath);

    for (const QString& existingDirectoryPath: scanDirectoryPaths_)
    {
        if (isSameDirectoryOrSubdirectoryOf(normalizedDirectoryPath, existingDirectoryPath))
        {
            return {.outcome = AddScanDirectoryOutcome::AlreadyIncluded,
                    .normalizedDirectoryPath = normalizedDirectoryPath,
                    .coveringDirectoryPath = existingDirectoryPath};
        }
    }

    QStringList updatedDirectoryPaths;

    for (const QString& existingDirectoryPath: scanDirectoryPaths_)
    {
        if (!isSameDirectoryOrSubdirectoryOf(existingDirectoryPath, normalizedDirectoryPath))
        {
            updatedDirectoryPaths.append(existingDirectoryPath);
        }
    }

    updatedDirectoryPaths.append(normalizedDirectoryPath);
    scanDirectoryPaths_ = std::move(updatedDirectoryPaths);

    emit scanDirectoryPathsChanged(scanDirectoryPaths_);
    return {AddScanDirectoryOutcome::Added, normalizedDirectoryPath, {}};
}

bool Model::removeScanDirectory(const QString& directoryPath)
{
    if (directoryPath.trimmed().isEmpty())
    {
        return false;
    }

    const QString normalizedDirectoryPath = normalizeDirectoryPath(directoryPath);

    for (qsizetype directoryIndex = 0; directoryIndex < scanDirectoryPaths_.size(); ++directoryIndex)
    {
        if (isSameDirectoryOrSubdirectoryOf(normalizedDirectoryPath, scanDirectoryPaths_.at(directoryIndex))
            && isSameDirectoryOrSubdirectoryOf(scanDirectoryPaths_.at(directoryIndex), normalizedDirectoryPath))
        {
            scanDirectoryPaths_.removeAt(directoryIndex);
            emit scanDirectoryPathsChanged(scanDirectoryPaths_);
            return true;
        }
    }

    return false;
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

    scanProgress_.reset();
    scanState_ = ScanState::Scanning;
    emit scanStateChanged(scanState_);
    return ScanStartOutcome::Started;
}

void Model::updateScanProgress(const ScanProgress& progress)
{
    if (scanState_ != ScanState::Scanning)
    {
        return;
    }

    scanProgress_ = progress;
    emit scanProgressChanged(progress);
}

void Model::completeScan(const ScanResult& scanResult)
{
    scanProgress_.reset();
    scanState_ = ScanState::Idle;
    latestScanResult_ = scanResult;

    emit scanStateChanged(scanState_);
    emit scanResultChanged();
}

void Model::markScanCancelled()
{
    scanProgress_.reset();
    scanState_ = ScanState::Idle;

    emit scanStateChanged(scanState_);
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

QString Model::normalizeDirectoryPath(const QString& directoryPath)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(QDir(directoryPath).absolutePath()));
}

bool Model::isSameDirectoryOrSubdirectoryOf(const QString& directoryPath,
                                            const QString& possibleParentDirectoryPath)
{
    const QString normalizedDirectoryPath = normalizeDirectoryPath(directoryPath);
    const QString normalizedParentDirectoryPath = normalizeDirectoryPath(possibleParentDirectoryPath);

#if defined(Q_OS_WIN)
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseSensitive;
#endif

    if (normalizedDirectoryPath.compare(normalizedParentDirectoryPath, pathCaseSensitivity) == 0)
    {
        return true;
    }

    QString parentDirectoryPathPrefix = normalizedParentDirectoryPath;

    if (!parentDirectoryPathPrefix.endsWith('/'))
    {
        parentDirectoryPathPrefix.append('/');
    }

    return normalizedDirectoryPath.startsWith(parentDirectoryPathPrefix, pathCaseSensitivity);
}
