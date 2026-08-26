#include "html_result_exporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <gtest/gtest.h>

/// @brief Verifies that exporting duplicate groups produces a complete UTF-8 HTML report with safely escaped data.
///
/// @par Test setup
/// Create one duplicate group containing Unicode text, HTML-sensitive characters, native directory paths,
/// and file sizes that exercise whole-kilobyte rounding. Select a writable temporary report destination.
///
/// @par Procedure
/// Export the group, read the generated file as UTF-8, and inspect its document structure and both result rows.
///
/// @par Expected results
/// - Export succeeds, clears the error message, and creates a complete HTML document.
/// - Unicode text is preserved while ampersands and angle brackets from file data are HTML-escaped.
/// - Both rows retain their group identifier and are marked correctly as reference and duplicate files.
/// - Directory paths use native separators and positive byte counts are rounded up to whole kilobytes.
TEST(HtmlResultExporterTest, WriteEscapedUtf8Report_WhenDuplicateGroupsAreExported)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString referenceDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("source & review"));
    const QString duplicateDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("copies > final"));

    DuplicateGroup duplicateGroup;
    duplicateGroup.addFile(FileRecord{QStringLiteral("R&D <résumé>.txt"), referenceDirectory, 1});
    duplicateGroup.addFile(FileRecord{QStringLiteral("copy > final.txt"), duplicateDirectory, 1025});

    const QString outputFilePath = temporaryDirectory.filePath(QStringLiteral("report.html"));
    QString errorMessage = QStringLiteral("stale error");

    ASSERT_TRUE(HtmlResultExporter::exportToFile(
        QList<DuplicateGroup>{duplicateGroup}, outputFilePath, errorMessage));
    EXPECT_TRUE(errorMessage.isEmpty());
    EXPECT_TRUE(QFileInfo::exists(outputFilePath));

    QFile outputFile(outputFilePath);
    ASSERT_TRUE(outputFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString report = QString::fromUtf8(outputFile.readAll());

    EXPECT_TRUE(report.startsWith(QStringLiteral("<!DOCTYPE html")));
    EXPECT_TRUE(report.contains(QStringLiteral("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />")));
    EXPECT_TRUE(report.endsWith(QStringLiteral("</html>\n")));

    EXPECT_EQ(report.count(QStringLiteral("data-group-id=\"0\"")), 2);
    EXPECT_EQ(report.count(QStringLiteral("data-reference=\"true\"")), 1);
    EXPECT_EQ(report.count(QStringLiteral("data-reference=\"false\"")), 1);

    EXPECT_TRUE(report.contains(QStringLiteral("R&amp;D &lt;résumé&gt;.txt")));
    EXPECT_TRUE(report.contains(QStringLiteral("copy &gt; final.txt")));
    EXPECT_FALSE(report.contains(QStringLiteral("R&D <résumé>.txt")));

    EXPECT_TRUE(report.contains(QDir::toNativeSeparators(referenceDirectory).toHtmlEscaped()));
    EXPECT_TRUE(report.contains(QDir::toNativeSeparators(duplicateDirectory).toHtmlEscaped()));
    EXPECT_TRUE(report.contains(QStringLiteral("<td>1</td><td>100</td>")));
    EXPECT_TRUE(report.contains(QStringLiteral("<td>2</td><td>100</td>")));
}

/// @brief Verifies that an HTML export reports an error when its destination file cannot be created.
///
/// @par Test setup
/// Select an output path inside a nonexistent subdirectory of a valid temporary directory.
///
/// @par Procedure
/// Attempt to export an empty duplicate-group collection and inspect the return value, error message, and filesystem.
///
/// @par Expected results
/// - Export returns `false`.
/// - A non-empty error message explains that the destination could not be opened.
/// - No partial report file is left behind.
TEST(HtmlResultExporterTest, ReturnFailureAndError_WhenDestinationCannotBeWritten)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString outputFilePath = temporaryDirectory.filePath(QStringLiteral("missing/report.html"));
    QString errorMessage;

    EXPECT_FALSE(HtmlResultExporter::exportToFile({}, outputFilePath, errorMessage));
    EXPECT_FALSE(errorMessage.isEmpty());
    EXPECT_FALSE(QFileInfo::exists(outputFilePath));
}
