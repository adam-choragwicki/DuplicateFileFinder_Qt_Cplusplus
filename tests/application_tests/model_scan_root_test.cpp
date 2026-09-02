#include "backend/model.h"

#include <QDir>

#include <gtest/gtest.h>

/// @brief Verifies that adding a broader scan root replaces an existing nested scan root.
///
/// @par Test setup
/// Create parent and nested directory paths, then construct an empty model.
///
/// @par Procedure
/// Add the nested path first and the parent path second.
///
/// @par Expected results
/// Both additions succeed, and the model retains only the parent because it already covers the nested root.
TEST(ModelScanRootTest, ReplaceNestedRoot_WhenBroaderRootIsAdded)
{
    const QString parentRootPath = QDir(QDir::tempPath()).filePath(QStringLiteral("model-test-parent-root"));
    const QString nestedRootPath = QDir(parentRootPath).filePath(QStringLiteral("nested-root"));
    Model model;

    ASSERT_EQ(model.addScanDirectory(nestedRootPath).getAddScanDirectoryOutcome(), Model::AddScanDirectoryOutcome::Added);
    ASSERT_EQ(model.addScanDirectory(parentRootPath).getAddScanDirectoryOutcome(), Model::AddScanDirectoryOutcome::Added);

    EXPECT_EQ(model.getScanDirectoryPaths(), QStringList{QDir(parentRootPath).absolutePath()});
}

/// @brief Verifies that removal matches a normalized scan root exactly and does not match its descendants.
///
/// @par Test setup
/// Add one scan root to an empty model and derive a nested path below it.
///
/// @par Procedure
/// Attempt removal using the nested path, then using the original root with a redundant `.` component.
///
/// @par Expected results
/// The nested path does not remove its parent, while the normalized equivalent of the root removes it and leaves
/// the scan-root list empty.
TEST(ModelScanRootTest, RemoveOnlyMatchingRoot_WhenPathIsNormalized)
{
    const QString rootPath = QDir(QDir::tempPath()).filePath(QStringLiteral("model-test-removal-root"));
    const QString nestedPath = QDir(rootPath).filePath(QStringLiteral("nested-root"));
    Model model;

    ASSERT_EQ(model.addScanDirectory(rootPath).getAddScanDirectoryOutcome(), Model::AddScanDirectoryOutcome::Added);

    EXPECT_FALSE(model.removeScanDirectory(nestedPath));
    EXPECT_TRUE(model.removeScanDirectory(QDir(rootPath).filePath(QStringLiteral("."))));
    EXPECT_TRUE(model.getScanDirectoryPaths().isEmpty());
}

/// @brief Verifies that scan-root removal follows the host platform's path case-sensitivity policy.
///
/// @par Test setup
/// Add a mixed-case scan-root path to an empty model and create a differently cased version of that path.
///
/// @par Procedure
/// Request removal using the differently cased path.
///
/// @par Expected results
/// The path matches and is removed on Windows; it does not match on case-sensitive host platforms.
TEST(ModelScanRootTest, UsePlatformCaseSensitivity_WhenRemovingRoot)
{
    const QString rootPath = QDir(QDir::tempPath()).filePath(QStringLiteral("model-test-MixedCase-root"));
    const QString differentlyCasedRootPath = rootPath.toUpper();
    Model model;

    ASSERT_NE(rootPath, differentlyCasedRootPath);
    ASSERT_EQ(model.addScanDirectory(rootPath).getAddScanDirectoryOutcome(), Model::AddScanDirectoryOutcome::Added);

#if defined(Q_OS_WIN)
    EXPECT_TRUE(model.removeScanDirectory(differentlyCasedRootPath));
#else
    EXPECT_FALSE(model.removeScanDirectory(differentlyCasedRootPath));
#endif
}
