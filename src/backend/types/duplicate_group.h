#pragma once

#include "file_record.h"

#include <QList>

/// @brief Owns an ordered group of files considered duplicates by a scan workflow.
///
/// Scan workflows produce groups containing at least two records. The first record is conventionally treated as the
/// reference file that would be retained, while subsequent records represent additional copies. This class preserves
/// insertion order but does not itself validate the number of files or whether they satisfy a duplicate criterion.
class DuplicateGroup
{
public:
    /// Appends a copy of a file record to the end of the group.
    /// @param[in] file File record to append.
    void addFile(const FileRecord& file) { files_.append(file); }

    /// Returns the file records in insertion order.
    [[nodiscard]] const QList<FileRecord>& getFiles() const { return files_; }

private:
    QList<FileRecord> files_;
};
