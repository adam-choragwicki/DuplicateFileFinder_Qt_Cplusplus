#include "frontend/main_window.h"
#include "frontend/scan_directories_tree_model.h"

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
/// Inspect the available labels, select each entry in turn, call `MainWindow::getScanTypeFromComboBox()`, and read the
/// description shown below the selector after every selection.
///
/// @par Expected results
/// - The combo box contains only “By file content” and “By file name”, in that order; no Designer placeholders
///   remain.
/// - File-content scanning is selected initially, and selecting either entry returns its corresponding `ScanType`.
/// - Each selection displays a concise description of the corresponding scan behavior.
TEST(DirectoriesTabTest, ReturnSelectedScanType_WhenScanTypeSelectionChanges)
{
    MainWindow mainWindow;
    auto* scanTypeComboBox = mainWindow.findChild<QComboBox*>(QStringLiteral("scanType_ComboBox"));
    const auto* scanTypeDescriptionLabel = mainWindow.findChild<QLabel*>(QStringLiteral("scanTypeDescription_Label"));

    ASSERT_NE(scanTypeComboBox, nullptr);
    ASSERT_NE(scanTypeDescriptionLabel, nullptr);
    ASSERT_EQ(scanTypeComboBox->count(), 2);
    EXPECT_EQ(scanTypeComboBox->itemText(0).toStdString(), std::string("By file content"));
    EXPECT_EQ(scanTypeComboBox->itemText(1).toStdString(), std::string("By file name"));

    EXPECT_EQ(scanTypeComboBox->currentIndex(), 0);
    EXPECT_EQ(mainWindow.getScanTypeFromComboBox(), ScanType::ByFileContent);
    EXPECT_EQ(scanTypeDescriptionLabel->text().toStdString(), std::string("Finds files with exactly identical contents, regardless of their names."));

    scanTypeComboBox->setCurrentIndex(1);
    EXPECT_EQ(mainWindow.getScanTypeFromComboBox(), ScanType::ByFileName);
    EXPECT_EQ(scanTypeDescriptionLabel->text().toStdString(), std::string("Finds files with matching names, ignoring letter case and the final extension. Their contents may differ."));
}

/// @brief Verifies that setting directory paths creates one normalized top-level scan root.
///
/// @par Test setup
/// Construct `MainWindow` and create a temporary `root/child` directory tree. Build an input path that reaches
/// `root` through the redundant `child/..` suffix.
///
/// @par Procedure
/// Pass the redundant path to `MainWindow::setScanDirectoryPaths()` and inspect the directory tree's root item.
///
/// @par Expected results
/// - The tree contains exactly one top-level item.
/// - The root item's absolute-path data is the normalized absolute path of `root`.
TEST(DirectoriesTabTest, DisplayNormalizedScanRoot_WhenDirectoryPathsAreSet)
{
    MainWindow mainWindow;

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString scanRootPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("root"));
    const QString childPath = QDir(scanRootPath).filePath(QStringLiteral("child"));
    ASSERT_TRUE(QDir().mkpath(childPath));

    const QString redundantScanRootPath = QDir(childPath).filePath(QStringLiteral(".."));
    const QString expectedScanRootPath = QDir(scanRootPath).absolutePath();
    mainWindow.setScanDirectoryPaths({redundantScanRootPath});

    const auto* directoriesTree = mainWindow.findChild<QTreeView*>(QStringLiteral("directories_TreeView"));
    ASSERT_NE(directoriesTree, nullptr);
    ASSERT_NE(directoriesTree->model(), nullptr);
    ASSERT_EQ(directoriesTree->model()->rowCount(), 1);

    const QModelIndex scanRootIndex = directoriesTree->model()->index(0, 0);
    ASSERT_TRUE(scanRootIndex.isValid());
    EXPECT_EQ(scanRootIndex.data(ScanDirectoriesTreeModel::AbsolutePathRole).toString(), expectedScanRootPath);
}

/// @brief Verifies that directory removal is available only for a selected top-level scan root.
///
/// @par Test setup
/// Construct `MainWindow`, configure a temporary root containing one child directory, and locate the directory tree
/// and Remove directory button.
///
/// @par Procedure
/// Observe the button and selected-root query after setting the root, after clearing the selection, after selecting
/// the root again, and after expanding the root and selecting its child.
///
/// @par Expected results
/// - The button is enabled while the top-level root is selected.
/// - The button is disabled when nothing or a child directory is selected.
/// - The selected-root query returns the root only for a top-level selection and is empty otherwise.
TEST(DirectoriesTabTest, EnableDirectoryRemoval_OnlyWhenScanRootIsSelected)
{
    MainWindow mainWindow;

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString scanRootPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("root"));
    ASSERT_TRUE(QDir().mkpath(QDir(scanRootPath).filePath(QStringLiteral("child"))));
    mainWindow.setScanDirectoryPaths({scanRootPath});

    auto* directoriesTree = mainWindow.findChild<QTreeView*>(QStringLiteral("directories_TreeView"));
    const auto* removeDirectoryButton = mainWindow.findChild<QPushButton*>(QStringLiteral("removeDirectory_PushButton"));
    ASSERT_NE(directoriesTree, nullptr);
    ASSERT_NE(removeDirectoryButton, nullptr);
    ASSERT_NE(directoriesTree->model(), nullptr);
    ASSERT_EQ(directoriesTree->model()->rowCount(), 1);

    const QModelIndex scanRootIndex = directoriesTree->model()->index(0, 0);
    ASSERT_TRUE(scanRootIndex.isValid());
    EXPECT_TRUE(removeDirectoryButton->isEnabled());
    EXPECT_EQ(mainWindow.getSelectedScanDirectoryPath(), QDir(scanRootPath).absolutePath());

    directoriesTree->clearSelection();
    EXPECT_FALSE(removeDirectoryButton->isEnabled());
    EXPECT_TRUE(mainWindow.getSelectedScanDirectoryPath().isEmpty());

    selectDirectory(*directoriesTree, scanRootIndex);
    EXPECT_TRUE(removeDirectoryButton->isEnabled());
    EXPECT_EQ(mainWindow.getSelectedScanDirectoryPath(), QDir(scanRootPath).absolutePath());

    directoriesTree->expand(scanRootIndex);
    ASSERT_EQ(directoriesTree->model()->rowCount(scanRootIndex), 1);
    directoriesTree->clearSelection();
    selectDirectory(*directoriesTree, directoriesTree->model()->index(0, 0, scanRootIndex));
    EXPECT_FALSE(removeDirectoryButton->isEnabled());
    EXPECT_TRUE(mainWindow.getSelectedScanDirectoryPath().isEmpty());
}

/// @brief Verifies that expanding a scan root lazily populates its immediate child directories exactly once.
///
/// @par Test setup
/// Construct `MainWindow` and configure a temporary root containing two immediate child directories, one nested
/// directory, and one ordinary file.
///
/// @par Procedure
/// Expand the root, inspect its children, expand it a second time, and then inspect the root item's absolute path.
///
/// @par Expected results
/// - Only the two immediate directories appear beneath the root, ordered by name; the ordinary file is omitted.
/// - The child containing a nested directory remains expandable, while the empty child does not.
/// - Expanding the root again does not duplicate child items.
/// - The top-level item retains the configured root's absolute path.
TEST(DirectoriesTabTest, PopulateChildDirectoriesOnce_WhenScanRootIsExpanded)
{
    MainWindow mainWindow;

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

    mainWindow.setScanDirectoryPaths({scanRootPath});

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
    EXPECT_FALSE(directoriesTree->model()->hasChildren(secondChildIndex));

    directoriesTree->collapse(scanRootIndex);
    directoriesTree->expand(scanRootIndex);
    EXPECT_EQ(directoriesTree->model()->rowCount(scanRootIndex), 2);

    EXPECT_EQ(scanRootIndex.data(ScanDirectoriesTreeModel::AbsolutePathRole).toString(),
              QDir(scanRootPath).absolutePath());
}
