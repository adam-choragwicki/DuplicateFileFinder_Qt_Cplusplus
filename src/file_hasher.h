#pragma once

#include <QCryptographicHash>

class FileHasher
{
public:
    [[nodiscard]] static std::optional<QByteArray> calculateFileHash(const FileRecord& file, const std::stop_token& stopToken)
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

        return hash.result();
    }
};
