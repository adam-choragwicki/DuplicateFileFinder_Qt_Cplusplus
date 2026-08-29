#pragma once

#include "backend/types/duplicate_group.h"

#include <QString>

class HtmlResultExporter final
{
public:
    /// Writes duplicate groups and their files in the supplied presentation order.
    [[nodiscard]] static bool exportToFile(const QList<DuplicateGroup>& duplicateGroups, const QString& outputFilePath, QString& errorMessage);
};
