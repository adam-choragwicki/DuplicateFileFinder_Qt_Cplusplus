#pragma once

#include "file_record.h"

#include <QList>

class DuplicateGroup
{
public:
    void addFile(const FileRecord& file) { files_.append(file); }

    [[nodiscard]] const QList<FileRecord>& getFiles() const { return files_; }

private:
    QList<FileRecord> files_;
};
