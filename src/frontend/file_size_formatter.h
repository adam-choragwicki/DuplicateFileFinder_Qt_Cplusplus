#pragma once

#include <QLocale>

/// @brief Formats file sizes consistently for on-screen and exported result presentation.
class FileSizeFormatter final
{
public:
    /// Formats exact byte counts consistently wherever scan results are presented.
    /// @param[in] sizeBytes File size in bytes.
    /// @return Locale-aware byte text below 1 KiB or a traditional binary-unit representation otherwise.
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
