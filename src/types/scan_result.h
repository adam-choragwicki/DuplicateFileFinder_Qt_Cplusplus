#pragma once

#include "file_record.h"
#include <QList>

struct ScanResult
{
    QList<FileRecord> files;
    bool cancelled{};
};
