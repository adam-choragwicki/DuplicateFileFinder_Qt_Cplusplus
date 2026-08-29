#pragma once

#include <QLocale>

class FileSizeFormatter final
{
public:
    /// Formats exact byte counts consistently wherever scan results are presented.
    [[nodiscard]] static QString format(const qint64 sizeBytes)
    {
        const QLocale locale;

        if (sizeBytes < 1024)
        {
            return QStringLiteral("%1 B").arg(locale.toString(sizeBytes));
        }

        return locale.formattedDataSize(sizeBytes, 2, QLocale::DataSizeTraditionalFormat);
    }
};
