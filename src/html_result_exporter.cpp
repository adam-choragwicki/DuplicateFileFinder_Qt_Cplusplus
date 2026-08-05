#include "html_result_exporter.h"

#include <QDir>
#include <QSaveFile>
#include <QTextStream>

namespace
{
    [[nodiscard]] qint64 sizeInWholeKilobytes(const qint64 sizeBytes)
    {
        if (sizeBytes <= 0)
        {
            return 0;
        }

        constexpr qint64 bytesPerKilobyte = 1024;
        return sizeBytes / bytesPerKilobyte + (sizeBytes % bytesPerKilobyte != 0 ? 1 : 0);
    }
}

bool HtmlResultExporter::exportToFile(
    const QList<DuplicateGroup>& duplicateGroups,
    const QString& outputFilePath,
    QString& errorMessage)
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
BODY
{
    background-color:white;
}

BODY,A,P,UL,TABLE,TR,TD
{
    font-family:Tahoma,Arial,sans-serif;
    font-size:10pt;
    color: #4477AA;
}

TABLE
{
    background-color: #225588;
    margin-left: auto;
    margin-right: auto;
    width: 90%;
}

TR
{
    background-color: white;
}

TH
{
    font-weight: bold;
    color: black;
    background-color: #C8D6E5;
}

TH TD
{
    color:black;
}

TD
{
    padding-left: 2pt;
}

TD.indented
{
    padding-left: 12pt;
}

H1
{
    font-family:"Courier New",monospace;
    color:#6da500;
    font-size:18pt;
    border-color: #70A0CF;
    border-width: 1pt;
    border-style: solid;
    margin-top: 16pt;
    margin-left: 5%;
    margin-right: 5%;
    padding-top: 2pt;
    padding-bottom:2pt;
    text-align: center;
}
    </style>
</head>
<body>
<h1>Duplicate File Finder Results</h1>
<table>
<tbody><tr><th>Filename</th><th>Folder</th><th>Size (KB)</th><th>Match %</th></tr>

)";

    for (qsizetype groupIndex = 0; groupIndex < duplicateGroups.size(); ++groupIndex)
    {
        const QList<FileRecord>& files = duplicateGroups.at(groupIndex).getFiles();

        for (qsizetype fileIndex = 0; fileIndex < files.size(); ++fileIndex)
        {
            const FileRecord& file = files.at(fileIndex);
            const bool isReferenceFile = fileIndex == 0;

            outputStream << "<tr data-group-id=\"" << groupIndex << "\" data-reference=\""
                         << (isReferenceFile ? "true" : "false") << "\">\n"
                         << "    <td class=\"" << (isReferenceFile ? "" : "indented") << "\">"
                         << file.getFileName().toHtmlEscaped() << "</td><td>"
                         << QDir::toNativeSeparators(file.getDirectoryPath()).toHtmlEscaped() << "</td><td>"
                         << sizeInWholeKilobytes(file.getSizeBytes()) << "</td><td>100</td>\n"
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
