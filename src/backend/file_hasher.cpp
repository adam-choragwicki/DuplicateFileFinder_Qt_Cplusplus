#include "file_hasher.h"
#include <QCryptographicHash>

std::optional<QByteArray> FileHasher::calculateFullFileHash(const FileRecord& file, const std::stop_token& stopToken)
{
    constexpr qint64 fileReadBufferSize = 1024 * 1024;

    if (stopToken.stop_requested())
    {
        return std::nullopt;
    }

    QFile inputFile(file.getAbsoluteFilePath());

    if (!inputFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Cannot hash file:" << inputFile.fileName() << '-' << inputFile.errorString();
        return std::nullopt;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    const qint64 bufferSize = std::min(fileReadBufferSize, std::max<qint64>(file.getSizeBytes(), 1));
    QByteArray buffer(bufferSize, '\0');
    qint64 totalBytesRead = 0;

    while (!inputFile.atEnd())
    {
        if (stopToken.stop_requested())
        {
            return std::nullopt;
        }

        const qint64 bytesRead = inputFile.read(buffer.data(), buffer.size());

        if (bytesRead < 0)
        {
            qWarning() << "Cannot read file while hashing:" << inputFile.fileName() << '-' << inputFile.errorString();
            return std::nullopt;
        }

        if (bytesRead == 0)
        {
            break;
        }

        hash.addData(QByteArrayView(buffer.constData(), bytesRead));
        totalBytesRead += bytesRead;
    }

    if (totalBytesRead != file.getSizeBytes())
    {
        qWarning() << "File size changed while hashing:" << inputFile.fileName()
                << "- expected" << file.getSizeBytes() << "bytes but read" << totalBytesRead;
        return std::nullopt;
    }

    return hash.result();
}

std::optional<QByteArray> FileHasher::calculateFileSampleHash(const FileRecord& file, const std::stop_token& stopToken)
{
    constexpr qint64 samplePrefixSize = 1024 * 1024;

    if (stopToken.stop_requested())
    {
        return std::nullopt;
    }

    QFile inputFile(file.getAbsoluteFilePath());

    if (!inputFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Cannot sample file:" << inputFile.fileName() << '-' << inputFile.errorString();
        return std::nullopt;
    }

    if (inputFile.size() != file.getSizeBytes())
    {
        qWarning() << "File size changed before sampling:" << inputFile.fileName()
                << "- expected" << file.getSizeBytes() << "bytes but found" << inputFile.size();
        return std::nullopt;
    }

    const qint64 sampledBytesCount = std::min(samplePrefixSize, file.getSizeBytes());
    QByteArray buffer(std::max<qint64>(sampledBytesCount, 1), '\0');
    QCryptographicHash sampleHash(QCryptographicHash::Sha256);
    qint64 remainingBytes = sampledBytesCount;

    while (remainingBytes > 0)
    {
        if (stopToken.stop_requested())
        {
            return std::nullopt;
        }

        const qint64 requestedBytes = std::min(remainingBytes, static_cast<qint64>(buffer.size()));
        const qint64 bytesRead = inputFile.read(buffer.data(), requestedBytes);

        if (bytesRead <= 0)
        {
            qWarning() << "Cannot read expected prefix while sampling file:" << inputFile.fileName()
                    << '-' << inputFile.errorString();
            return std::nullopt;
        }

        sampleHash.addData(QByteArrayView(buffer.constData(), bytesRead));
        remainingBytes -= bytesRead;
    }

    if (inputFile.size() != file.getSizeBytes())
    {
        qWarning() << "File size changed while sampling:" << inputFile.fileName()
                << "- expected" << file.getSizeBytes() << "bytes but found" << inputFile.size();
        return std::nullopt;
    }

    return sampleHash.result();
}
