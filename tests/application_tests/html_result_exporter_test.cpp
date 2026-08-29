#include "html_result_exporter.h"
#include "test_helpers/scoped_default_locale.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <gtest/gtest.h>

/// @brief Verifies that exporting duplicate groups produces a complete UTF-8 HTML report with safely escaped data.
///
/// @par Test setup
/// Create one duplicate group containing Unicode text, HTML-sensitive characters, directory paths, and file
/// sizes on either side of the adaptive-size threshold. Select a writable temporary report destination.
///
/// @par Procedure
/// Export the group, read the generated file as UTF-8, and inspect its document structure and both result rows.
///
/// @par Expected results
/// - Export succeeds, clears the error message, and creates a complete HTML document.
/// - Unicode text is preserved while ampersands and angle brackets from file data are HTML-escaped.
/// - Both rows retain their group identifier and are styled correctly as reference and duplicate files.
/// - The table labels, visible group number, row colors, and size alignment follow the GUI results table.
/// - Directory paths and adaptive, locale-aware file sizes use the same display text as the GUI.
TEST(HtmlResultExporterTest, WriteEscapedUtf8Report_WhenDuplicateGroupsAreExported)
{
    const test_helpers::ScopedDefaultLocale defaultLocale{QLocale::c()};
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
    EXPECT_TRUE(report.contains(QStringLiteral("<th>File name</th><th>Directories</th><th>Size</th>")));
    EXPECT_FALSE(report.contains(QStringLiteral("Size (KB)")));
    EXPECT_FALSE(report.contains(QStringLiteral("Match %")));
    EXPECT_TRUE(report.contains(QStringLiteral("<tr class=\"reference\"")));
    EXPECT_TRUE(report.contains(QStringLiteral("<tr class=\"duplicate\"")));
    EXPECT_TRUE(report.contains(QStringLiteral("background-color: #C0C0C0")));
    EXPECT_TRUE(report.contains(QStringLiteral("color: #2032E8")));
    EXPECT_TRUE(report.contains(QStringLiteral("<th class=\"group-number\" scope=\"row\">1</th>")));
    EXPECT_TRUE(report.contains(QStringLiteral("td.size")));
    EXPECT_TRUE(report.contains(QStringLiteral("text-align: right")));

    EXPECT_TRUE(report.contains(QStringLiteral("R&amp;D &lt;résumé&gt;.txt")));
    EXPECT_TRUE(report.contains(QStringLiteral("copy &gt; final.txt")));
    EXPECT_FALSE(report.contains(QStringLiteral("R&D <résumé>.txt")));

    EXPECT_TRUE(report.contains(referenceDirectory.toHtmlEscaped()));
    EXPECT_TRUE(report.contains(duplicateDirectory.toHtmlEscaped()));
    EXPECT_TRUE(report.contains(QStringLiteral("<td class=\"size\">1 B</td>\n</tr>")));
    EXPECT_TRUE(report.contains(QStringLiteral("<td class=\"size\">1.00 kB</td>\n</tr>")));
}

/// @brief Verifies that export preserves the presentation order supplied by the results table.
///
/// @par Test setup
/// Create two groups in an intentionally non-alphabetical order and give each group two distinctly named files.
///
/// @par Procedure
/// Export the groups and locate every filename and visible group number in the generated document.
///
/// @par Expected results
/// - Groups occur in exactly the supplied order and every duplicate stays immediately after its reference file.
/// - Reference rows display consecutive, one-based group numbers; duplicate rows have blank group-number cells.
TEST(HtmlResultExporterTest, PreservePresentationOrder_WhenDuplicateGroupsAreExported)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    DuplicateGroup secondGroup;
    secondGroup.addFile(FileRecord{QStringLiteral("second.txt"), QStringLiteral("C:/second"), 2048});
    secondGroup.addFile(FileRecord{QStringLiteral("second-copy.txt"), QStringLiteral("C:/second/copy"), 2048});

    DuplicateGroup firstGroup;
    firstGroup.addFile(FileRecord{QStringLiteral("first.txt"), QStringLiteral("C:/first"), 1024});
    firstGroup.addFile(FileRecord{QStringLiteral("first-copy.txt"), QStringLiteral("C:/first/copy"), 1024});

    const QString outputFilePath = temporaryDirectory.filePath(QStringLiteral("ordered-report.html"));
    QString errorMessage;
    ASSERT_TRUE(HtmlResultExporter::exportToFile(
        QList<DuplicateGroup>{secondGroup, firstGroup}, outputFilePath, errorMessage));

    QFile outputFile(outputFilePath);
    ASSERT_TRUE(outputFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString report = QString::fromUtf8(outputFile.readAll());

    const qsizetype secondReferencePosition = report.indexOf(QStringLiteral(">second.txt</td>"));
    const qsizetype secondDuplicatePosition = report.indexOf(QStringLiteral(">second-copy.txt</td>"));
    const qsizetype firstReferencePosition = report.indexOf(QStringLiteral(">first.txt</td>"));
    const qsizetype firstDuplicatePosition = report.indexOf(QStringLiteral(">first-copy.txt</td>"));

    ASSERT_GE(secondReferencePosition, 0);
    ASSERT_GE(secondDuplicatePosition, 0);
    ASSERT_GE(firstReferencePosition, 0);
    ASSERT_GE(firstDuplicatePosition, 0);
    EXPECT_LT(secondReferencePosition, secondDuplicatePosition);
    EXPECT_LT(secondDuplicatePosition, firstReferencePosition);
    EXPECT_LT(firstReferencePosition, firstDuplicatePosition);

    EXPECT_EQ(report.count(QStringLiteral("<th class=\"group-number\" scope=\"row\">1</th>")), 1);
    EXPECT_EQ(report.count(QStringLiteral("<th class=\"group-number\" scope=\"row\">2</th>")), 1);
    EXPECT_EQ(report.count(QStringLiteral("<th class=\"group-number\" scope=\"row\"></th>")), 2);
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
