#pragma once

#include <QString>

struct FileRecord
{
    QString fileName_;
    QString directoryPath_;
    qint64 sizeBytes_{};
};
