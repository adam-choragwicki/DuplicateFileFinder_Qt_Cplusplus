#pragma once

#include <QMetaType>

enum class ScanType
{
    ByFileName,
    ByFileContent
};

Q_DECLARE_METATYPE(ScanType)
