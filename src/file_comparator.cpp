#include "file_comparator.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>

namespace
{
    constexpr qsizetype comparisonBufferSize = 1024 * 1024;
}

FileComparisonResult FileComparator::compareFileContents(const FileRecord& firstFile, const FileRecord& secondFile, const std::stop_token& stopToken)
{
    if (stopToken.stop_requested())
    {
        return FileComparisonResult::Cancelled;
    }

    QFile firstInputFile(firstFile.getAbsoluteFilePath());
    QFile secondInputFile(secondFile.getAbsoluteFilePath());

    if (!firstInputFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Cannot compare file:" << firstInputFile.fileName() << '-' << firstInputFile.errorString();
        return FileComparisonResult::Failed;
    }

    if (!secondInputFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Cannot compare file:" << secondInputFile.fileName() << '-' << secondInputFile.errorString();
        return FileComparisonResult::Failed;
    }

    QByteArray firstBuffer(comparisonBufferSize, '\0');
    QByteArray secondBuffer(comparisonBufferSize, '\0');

    while (true)
    {
        if (stopToken.stop_requested())
        {
            return FileComparisonResult::Cancelled;
        }

        const qint64 firstBytesRead = firstInputFile.read(firstBuffer.data(), firstBuffer.size());

        if (firstBytesRead < 0)
        {
            qWarning() << "Cannot read file while comparing:" << firstInputFile.fileName() << '-' << firstInputFile.errorString();
            return FileComparisonResult::Failed;
        }

        if (stopToken.stop_requested())
        {
            return FileComparisonResult::Cancelled;
        }

        const qint64 secondBytesRead = secondInputFile.read(secondBuffer.data(), secondBuffer.size());

        if (secondBytesRead < 0)
        {
            qWarning() << "Cannot read file while comparing:" << secondInputFile.fileName() << '-' << secondInputFile.errorString();
            return FileComparisonResult::Failed;
        }

        if (firstBytesRead != secondBytesRead)
        {
            return FileComparisonResult::Different;
        }

        if (firstBytesRead == 0)
        {
            return FileComparisonResult::Equal;
        }

        if (std::memcmp(firstBuffer.constData(), secondBuffer.constData(), static_cast<std::size_t>(firstBytesRead)) != 0)
        {
            return FileComparisonResult::Different;
        }
    }
}
