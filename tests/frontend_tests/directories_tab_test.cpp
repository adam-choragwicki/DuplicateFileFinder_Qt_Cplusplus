#include "frontend/main_window.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeView>

#include <gtest/gtest.h>

namespace
{
    /// @brief Removes every scan root created during `MainWindow` initialization or a preceding test step.
    ///
    /// @param mainWindow Window whose top-level scan roots are removed.
    void removeAllScanRoots(MainWindow& mainWindow)
    {
        const QStringList scanRootPaths = mainWindow.getScanDirectoryPaths();

        for (const QString& scanRootPath: scanRootPaths)
        {
            mainWindow.removeScanDirectory(scanRootPath);
        }
    }

    /// @brief Selects one directory-tree row and clears any preceding selection.
    ///
    /// @param directoriesTree Tree view whose selection is changed.
    /// @param directoryIndex Model index representing the directory to select.
    void selectDirectory(QTreeView& directoriesTree, const QModelIndex& directoryIndex)
    {
        directoriesTree.setCurrentIndex(directoryIndex);
        directoriesTree.selectionModel()->select(directoryIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
}

/// @brief Verifies that each scan-type choice maps to the expected value and explanatory description.
///
/// @par Test setup
/// Construct `MainWindow` and locate the scan-type combo box and description label populated during initialization.
///
/// @par Procedure
/// Inspect the available labels, select each entry in turn, call `MainWindow::getScanType()`, and read the
/// description shown below the selector after every selection.
///
/// @par Expected results
/// - The combo box contains only “By file name” and “By file content”; no Designer placeholders remain.
/// - Selecting each entry returns `ScanType::ByFileName` or `ScanType::ByFileContent`, respectively.
/// - Each selection displays a concise description of the corresponding scan behavior.
TEST(DirectoriesTabTest, ReturnSelectedScanType_WhenScanTypeSelectionChanges)
{
    MainWindow mainWindow;
    auto* scanTypeComboBox = mainWindow.findChild<QComboBox*>(QStringLiteral("scanType_ComboBox"));
    const auto* scanTypeDescriptionLabel = mainWindow.findChild<QLabel*>(QStringLiteral("scanTypeDescription_Label"));

    ASSERT_NE(scanTypeComboBox, nullptr);
    ASSERT_NE(scanTypeDescriptionLabel, nullptr);
    ASSERT_EQ(scanTypeComboBox->count(), 2);
    EXPECT_EQ(scanTypeComboBox->itemText(0).toStdString(), std::string("By file name"));
    EXPECT_EQ(scanTypeComboBox->itemText(1).toStdString(), std::string("By file content"));

    scanTypeComboBox->setCurrentIndex(0);
    EXPECT_EQ(mainWindow.getScanTypeFromComboBox(), ScanType::ByFileName);
    EXPECT_EQ(scanTypeDescriptionLabel->text().toStdString(), std::string("Finds files with matching names, ignoring letter case and the final extension. Their contents may differ."));

    scanTypeComboBox->setCurrentIndex(1);
    EXPECT_EQ(mainWindow.getScanTypeFromComboBox(), ScanType::ByFileContent);
    EXPECT_EQ(scanTypeDescriptionLabel->text().toStdString(), std::string("Finds files with exactly identical contents, regardless of their names."));
}

/// @brief Verifies that adding a directory creates one normalized top-level scan root.
///
/// @par Test setup
/// Construct `MainWindow`, remove its initial scan root, and create a temporary `root/child` directory tree.
/// Build an input path that reaches `root` through the redundant `child/..` suffix.
///
/// @par Procedure
/// Pass the redundant path to `MainWindow::addScanDirectory()` and inspect the directory tree and the paths
/// returned by `MainWindow::getScanDirectoryPaths()`.
///
/// @par Expected results
/// - The tree contains exactly one top-level item.
/// - The reported scan-root path is the normalized absolute path of `root`.
TEST(DirectoriesTabTest, AddNormalizedScanRoot_WhenDirectoryIsAdded)
{
    MainWindow mainWindow;
    removeAllScanRoots(mainWindow);

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString scanRootPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("root"));
    const QString childPath = QDir(scanRootPath).filePath(QStringLiteral("child"));
    ASSERT_TRUE(QDir().mkpath(childPath));

    const QString redundantScanRootPath = QDir(childPath).filePath(QStringLiteral(".."));
    const QString expectedScanRootPath = QDir(scanRootPath).absolutePath();
    mainWindow.addScanDirectory(redundantScanRootPath);

    const auto* directoriesTree = mainWindow.findChild<QTreeView*>(QStringLiteral("directories_TreeView"));
    ASSERT_NE(directoriesTree, nullptr);
    ASSERT_NE(directoriesTree->model(), nullptr);
    ASSERT_EQ(directoriesTree->model()->rowCount(), 1);

    const QStringList scanRootPaths = mainWindow.getScanDirectoryPaths();
    ASSERT_EQ(scanRootPaths.size(), 1);
    EXPECT_EQ(scanRootPaths.constFirst().toStdString(), expectedScanRootPath.toStdString());
}

/// @brief Verifies that directory removal is available only for a selected top-level scan root.
///
/// @par Test setup
/// Construct `MainWindow`, replace its initial root with a temporary root containing one child directory, and
/// locate the directory tree and Remove directory button.
///
/// @par Procedure
/// Observe the button after adding the selected root, after clearing the selection, after selecting the root
/// again, and after expanding the root and selecting its child.
///
/// @par Expected results
/// - The button is enabled while the top-level root is selected.
/// - The button is disabled when nothing or a child directory is selected.
TEST(DirectoriesTabTest, EnableDirectoryRemoval_OnlyWhenScanRootIsSelected)
{
    MainWindow mainWindow;
    removeAllScanRoots(mainWindow);

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString scanRootPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("root"));
    ASSERT_TRUE(QDir().mkpath(QDir(scanRootPath).filePath(QStringLiteral("child"))));
    mainWindow.addScanDirectory(scanRootPath);

    auto* directoriesTree = mainWindow.findChild<QTreeView*>(QStringLiteral("directories_TreeView"));
    const auto* removeDirectoryButton = mainWindow.findChild<QPushButton*>(QStringLiteral("removeDirectory_PushButton"));
    ASSERT_NE(directoriesTree, nullptr);
    ASSERT_NE(removeDirectoryButton, nullptr);
    ASSERT_NE(directoriesTree->model(), nullptr);
    ASSERT_EQ(directoriesTree->model()->rowCount(), 1);

    const QModelIndex scanRootIndex = directoriesTree->model()->index(0, 0);
    ASSERT_TRUE(scanRootIndex.isValid());
    EXPECT_TRUE(removeDirectoryButton->isEnabled());

    directoriesTree->clearSelection();
    EXPECT_FALSE(removeDirectoryButton->isEnabled());

    selectDirectory(*directoriesTree, scanRootIndex);
    EXPECT_TRUE(removeDirectoryButton->isEnabled());

    directoriesTree->expand(scanRootIndex);
    ASSERT_EQ(directoriesTree->model()->rowCount(scanRootIndex), 1);
    directoriesTree->clearSelection();
    selectDirectory(*directoriesTree, directoriesTree->model()->index(0, 0, scanRootIndex));
    EXPECT_FALSE(removeDirectoryButton->isEnabled());
}

/// @brief Verifies that removing a selected child cannot remove its top-level scan root.
///
/// @par Test setup
/// Construct `MainWindow`, add a temporary root containing one child directory, expand it, and select the child.
///
/// @par Procedure
/// Call `MainWindow::removeSelectedScanDirectory()` and inspect both the tree and the reported scan-root paths.
///
/// @par Expected results
/// - The top-level root and its child remain in the tree.
/// - `MainWindow::getScanDirectoryPaths()` still reports the original root as its only entry.
TEST(DirectoriesTabTest, KeepScanRoot_WhenChildDirectoryRemovalIsRequested)
{
    MainWindow mainWindow;
    removeAllScanRoots(mainWindow);

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString scanRootPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("root"));
    ASSERT_TRUE(QDir().mkpath(QDir(scanRootPath).filePath(QStringLiteral("child"))));
    mainWindow.addScanDirectory(scanRootPath);

    auto* directoriesTree = mainWindow.findChild<QTreeView*>(QStringLiteral("directories_TreeView"));
    ASSERT_NE(directoriesTree, nullptr);
    ASSERT_NE(directoriesTree->model(), nullptr);
    ASSERT_EQ(directoriesTree->model()->rowCount(), 1);

    const QModelIndex scanRootIndex = directoriesTree->model()->index(0, 0);
    directoriesTree->expand(scanRootIndex);
    ASSERT_EQ(directoriesTree->model()->rowCount(scanRootIndex), 1);

    directoriesTree->clearSelection();
    selectDirectory(*directoriesTree, directoriesTree->model()->index(0, 0, scanRootIndex));
    mainWindow.removeSelectedScanDirectory();

    ASSERT_EQ(directoriesTree->model()->rowCount(), 1);
    EXPECT_EQ(directoriesTree->model()->rowCount(scanRootIndex), 1);

    const QStringList scanRootPaths = mainWindow.getScanDirectoryPaths();
    ASSERT_EQ(scanRootPaths.size(), 1);
    EXPECT_EQ(scanRootPaths.constFirst().toStdString(), QDir(scanRootPath).absolutePath().toStdString());
}

/// @brief Verifies that expanding a scan root lazily populates its immediate child directories exactly once.
///
/// @par Test setup
/// Construct `MainWindow` and add a temporary root containing two immediate child directories, one nested
/// directory, and one ordinary file.
///
/// @par Procedure
/// Expand the root, inspect its children, expand it a second time, and then inspect the top-level scan-root list.
///
/// @par Expected results
/// - Only the two immediate directories appear beneath the root, ordered by name; the ordinary file is omitted.
/// - The child containing a nested directory remains expandable.
/// - Expanding the root again does not duplicate child items.
/// - Only the top-level root is returned as a scan path.
TEST(DirectoriesTabTest, PopulateChildDirectoriesOnce_WhenScanRootIsExpanded)
{
    MainWindow mainWindow;
    removeAllScanRoots(mainWindow);

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString scanRootPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("root"));
    const QString firstChildPath = QDir(scanRootPath).filePath(QStringLiteral("first"));
    const QString secondChildPath = QDir(scanRootPath).filePath(QStringLiteral("second"));
    ASSERT_TRUE(QDir().mkpath(QDir(firstChildPath).filePath(QStringLiteral("nested"))));
    ASSERT_TRUE(QDir().mkpath(secondChildPath));

    QFile ordinaryFile(QDir(scanRootPath).filePath(QStringLiteral("ordinary.txt")));
    ASSERT_TRUE(ordinaryFile.open(QIODevice::WriteOnly));
    ordinaryFile.close();

    mainWindow.addScanDirectory(scanRootPath);

    auto* directoriesTree = mainWindow.findChild<QTreeView*>(QStringLiteral("directories_TreeView"));
    ASSERT_NE(directoriesTree, nullptr);
    ASSERT_NE(directoriesTree->model(), nullptr);
    ASSERT_EQ(directoriesTree->model()->rowCount(), 1);

    const QModelIndex scanRootIndex = directoriesTree->model()->index(0, 0);
    ASSERT_EQ(directoriesTree->model()->rowCount(scanRootIndex), 0);

    directoriesTree->expand(scanRootIndex);

    ASSERT_EQ(directoriesTree->model()->rowCount(scanRootIndex), 2);
    const QModelIndex firstChildIndex = directoriesTree->model()->index(0, 0, scanRootIndex);
    const QModelIndex secondChildIndex = directoriesTree->model()->index(1, 0, scanRootIndex);
    EXPECT_EQ(firstChildIndex.data().toString().toStdString(), std::string("first"));
    EXPECT_EQ(secondChildIndex.data().toString().toStdString(), std::string("second"));
    EXPECT_TRUE(directoriesTree->model()->hasChildren(firstChildIndex));

    directoriesTree->collapse(scanRootIndex);
    directoriesTree->expand(scanRootIndex);
    EXPECT_EQ(directoriesTree->model()->rowCount(scanRootIndex), 2);

    const QStringList scanRootPaths = mainWindow.getScanDirectoryPaths();
    ASSERT_EQ(scanRootPaths.size(), 1);
    EXPECT_EQ(scanRootPaths.constFirst().toStdString(), QDir(scanRootPath).absolutePath().toStdString());
}
