#pragma once

#include "backend/types/duplicate_group.h"

/// @brief Serializes duplicate-file results as a standalone UTF-8 HTML document.
class HtmlResultExporter final
{
public:
    /// Writes duplicate groups and their files in the supplied presentation order.
    /// @param[in] duplicateGroups Groups to serialize in document order.
    /// @param[in] outputFilePath Destination file path to create or replace.
    /// @param[out] errorMessage Receives a user-facing explanation on failure and is cleared on success.
    /// @return true when the complete document was written successfully; otherwise false.
    [[nodiscard]] static bool exportToFile(const QList<DuplicateGroup>& duplicateGroups, const QString& outputFilePath, QString& errorMessage);
};
