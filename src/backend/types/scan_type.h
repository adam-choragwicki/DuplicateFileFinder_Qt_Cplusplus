#pragma once

#include <QMetaType>
#include <QString>

enum class ScanType
{
    ByFileName,
    ByFileContent
};

[[nodiscard]] inline QString scanTypeToString(const ScanType scanType)
{
    switch (scanType)
    {
        case ScanType::ByFileName:
            return QStringLiteral("by file name");

        case ScanType::ByFileContent:
            return QStringLiteral("by file content");
    }

    qFatal("Unknown ScanType value: %d", scanType);
}

Q_DECLARE_METATYPE(ScanType)
