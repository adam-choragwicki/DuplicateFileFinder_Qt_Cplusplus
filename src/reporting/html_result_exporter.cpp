#include "html_result_exporter.h"
#include "frontend/file_size_formatter.h"

#include <QTextStream>
#include <QSaveFile>

bool HtmlResultExporter::exportToFile(const QList<DuplicateGroup>& duplicateGroups, const QString& outputFilePath, QString& errorMessage)
{
    errorMessage.clear();

    QSaveFile outputFile(outputFilePath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        errorMessage = outputFile.errorString();
        return false;
    }

    QTextStream outputStream(&outputFile);
    outputStream.setEncoding(QStringConverter::Utf8);
    outputStream << R"(<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <title>Duplicate File Finder Results</title>
    <style type="text/css">
body
{
    background-color: white;
    color: black;
    font-family: "Segoe UI", Tahoma, Arial, sans-serif;
    font-size: 10pt;
    margin: 16px;
}

table
{
    border-collapse: collapse;
    width: 100%;
}

col.group-number-column
{
    width: 2.5em;
}

col.file-name
{
    width: 30%;
}

col.directory
{
    width: 60%;
}

col.size
{
    width: 10%;
}

th, td
{
    border: 1px solid #808080;
    padding: 3px 6px;
    text-align: left;
    white-space: nowrap;
}

thead th
{
    font-weight: bold;
    color: black;
    background-color: #F7F7F7;
}

th.group-number
{
    background-color: #F0F0F0;
    color: black;
    font-weight: normal;
    min-width: 2.5em;
    text-align: center;
    width: 2.5em;
}

tr.reference
{
    background-color: #C0C0C0;
    color: #2032E8;
    font-weight: bold;
}

tr.duplicate
{
    background-color: white;
    color: black;
}

td.size
{
    text-align: right;
}
    </style>
</head>
<body>
<table>
<colgroup><col class="group-number-column" /><col class="file-name" /><col class="directory" /><col class="size" /></colgroup>
<thead><tr><th class="group-number"></th><th>File name</th><th>Directories</th><th>Size</th></tr></thead>
<tbody>

)";

    for (qsizetype groupIndex = 0; groupIndex < duplicateGroups.size(); ++groupIndex)
    {
        const QList<FileRecord>& files = duplicateGroups.at(groupIndex).getFiles();

        for (qsizetype fileIndex = 0; fileIndex < files.size(); ++fileIndex)
        {
            const FileRecord& file = files.at(fileIndex);
            const bool isReferenceFile = fileIndex == 0;

            outputStream << "<tr class=\"" << (isReferenceFile ? "reference" : "duplicate")
                    << "\" data-group-id=\"" << groupIndex << "\" data-reference=\""
                    << (isReferenceFile ? "true" : "false") << "\">\n"
                    << "    <th class=\"group-number\" scope=\"row\">"
                    << (isReferenceFile ? QString::number(groupIndex + 1) : QString{}) << "</th><td>"
                    << file.getFileName().toHtmlEscaped() << "</td><td>"
                    << file.getDirectoryPath().toHtmlEscaped() << "</td><td class=\"size\">"
                    << FileSizeFormatter::format(file.getSizeBytes()).toHtmlEscaped() << "</td>\n"
                    << "</tr>\n\n";
        }
    }

    outputStream << R"(</tbody></table>
</body>
</html>
)";
    outputStream.flush();

    if (outputStream.status() != QTextStream::Ok)
    {
        errorMessage = QStringLiteral("Failed while writing the HTML report");
        outputFile.cancelWriting();
        return false;
    }

    if (!outputFile.commit())
    {
        errorMessage = outputFile.errorString();
        return false;
    }

    return true;
}
