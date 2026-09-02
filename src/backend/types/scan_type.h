#pragma once

#include <QMetaType>
#include <QString>

/// @brief Selects the criterion used to identify duplicate files.
enum class ScanType
{
    /// Group files whose names match according to the file-name workflow's comparison policy.
    ByFileName,
    /// Group files whose contents are verified to be byte-for-byte identical.
    ByFileContent
};

/// Returns a human-readable description of a scan type for diagnostic logging.
/// @param[in] scanType Scan type to describe.
/// @return Lowercase description suitable for inclusion in a log message.
/// @warning Terminates the process if scanType is not a recognized enumerator.
[[nodiscard]] inline QString scanTypeToString(const ScanType scanType)
{
    switch (scanType)
    {
        case ScanType::ByFileName:
            return QStringLiteral("by file name");

        case ScanType::ByFileContent:
            return QStringLiteral("by file content");
    }

    qFatal("Unknown ScanType value: %d", qToUnderlying(scanType));
}

Q_DECLARE_METATYPE(ScanType)
