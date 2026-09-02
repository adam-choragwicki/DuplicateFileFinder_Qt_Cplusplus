#pragma once

#include <QCryptographicHash>

/// @brief Provides cancellation-aware SHA-256 hashing for complete files and fixed-size prefixes.
class FileHasher
{
public:
    /// Calculates the SHA-256 digest of an entire file and verifies that the bytes read match its recorded size.
    /// @param[in] file File record identifying the input and its expected size.
    /// @param[in] stopToken Cancellation token observed before and during file reads.
    /// @return Digest on success, or std::nullopt after cancellation, an I/O error, or a size change.
    [[nodiscard]] static std::optional<QByteArray> calculateFullFileHash(const FileRecord& file, const std::stop_token& stopToken)
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

    /// Hashes up to the first 1 MiB of a file after verifying its recorded size.
    /// A matching result is only a reason to continue with a full hash; it is never proof that files are equal.
    /// @param[in] file File record identifying the input and its expected size.
    /// @param[in] stopToken Cancellation token observed before and during prefix reads.
    /// @return Prefix digest on success, or std::nullopt after cancellation, an I/O error, or a size change.
    [[nodiscard]] static std::optional<QByteArray> calculateFileSampleHash(const FileRecord& file, const std::stop_token& stopToken)
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
};
