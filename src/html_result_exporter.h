#pragma once

#include "types/duplicate_group.h"

#include <QString>

class HtmlResultExporter final
{
public:
    [[nodiscard]] static bool exportToFile(const QList<DuplicateGroup>& duplicateGroups, const QString& outputFilePath, QString& errorMessage);
};
