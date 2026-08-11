#include "main_window.h"
#include "results_tab.h"
#include "ui_main_window.h"

#include <QComboBox>
#include <QDir>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("Duplicate file finder");

    initializeUI();

    connect(ui->addDirectory_PushButton, &QPushButton::clicked, this, &MainWindow::addDirectoryButtonClicked);
    connect(ui->removeDirectory_PushButton, &QPushButton::clicked, this, &MainWindow::removeDirectoryButtonClicked);
    connect(ui->resultsTab, &ResultsTab::revealFileInSystemFileManagerRequested, this, &MainWindow::revealFileInSystemFileManagerRequested);
    connect(ui->startScan_PushButton, &QPushButton::clicked, this, &MainWindow::startScanButtonClicked);

    connect(ui->exportToHtml_Action, &QAction::triggered, this, &MainWindow::exportToHtmlRequested);
    connect(ui->quit_Action, &QAction::triggered, this, &QWidget::close);
    connect(ui->aboutDuplicateFileFinder_Action, &QAction::triggered, this, &MainWindow::showAboutDialog);

    connect(ui->directories_TreeWidget, &QTreeWidget::itemExpanded, this, &MainWindow::populateDirectoryTreeItem);
    connect(ui->directories_TreeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::updateDirectoryActionStates);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initializeUI()
{
    // Clear placeholder values from combo boxes
    ui->scanType_ComboBox->clear();

    populateScanTypeComboBox();
    initializeDirectoriesTree();
}

void MainWindow::initializeDirectoriesTree()
{
    ui->directories_TreeWidget->clear();
    ui->directories_TreeWidget->setUniformRowHeights(true);

    addScanDirectory(getInitialDirectoryScanPath());
    ui->directories_TreeWidget->clearSelection();
    updateDirectoryActionStates();
}

void MainWindow::populateScanTypeComboBox()
{
    ui->scanType_ComboBox->addItem("By file name", QVariant::fromValue(ScanType::ByFileName));
    ui->scanType_ComboBox->addItem("By file content", QVariant::fromValue(ScanType::ByFileContent));

    // Center align text
    for (int i = 0; i < ui->scanType_ComboBox->count(); ++i)
    {
        ui->scanType_ComboBox->setItemData(i, Qt::AlignCenter, Qt::TextAlignmentRole);
    }

    ui->scanType_ComboBox->setCurrentIndex(0); // choose "By file name"
}

void MainWindow::showAboutDialog()
{
    QMessageBox aboutDialog(
        QMessageBox::Information,
        QStringLiteral("About Duplicate file finder"),
        QStringLiteral("Duplicate file finder\nVersion %1")
        .arg(QString::fromUtf8(APP_VERSION)),
        QMessageBox::Ok,
        this);
    aboutDialog.exec();
}

void MainWindow::showScanResult(const ScanResult& scanResult)
{
    ui->resultsTab->showDuplicateGroups(scanResult.getDuplicateGroups());

    ui->exportToHtml_Action->setEnabled(true);
    ui->main_TabWidget->setCurrentWidget(ui->resultsTab);
}

QStringList MainWindow::getScanDirectoryPaths() const
{
    QStringList directoryPaths;

    for (int rootIndex = 0; rootIndex < ui->directories_TreeWidget->topLevelItemCount(); ++rootIndex)
    {
        const QTreeWidgetItem* rootDirectoryItem = ui->directories_TreeWidget->topLevelItem(rootIndex);
        directoryPaths.append(rootDirectoryItem->data(0, directoryPathDataRole_).toString());
    }

    return directoryPaths;
}

void MainWindow::addScanDirectory(const QString& directoryPath)
{
    const QString normalizedDirectoryPath = QDir(directoryPath).absolutePath();
    QTreeWidgetItem* directoryItem = createDirectoryTreeItem(normalizedDirectoryPath,
                                                             QDir::toNativeSeparators(normalizedDirectoryPath));

    ui->directories_TreeWidget->addTopLevelItem(directoryItem);
    ui->directories_TreeWidget->setCurrentItem(directoryItem);
    updateDirectoryActionStates();
}

void MainWindow::removeScanDirectory(const QString& directoryPath)
{
    for (int rootIndex = 0; rootIndex < ui->directories_TreeWidget->topLevelItemCount(); ++rootIndex)
    {
        const QTreeWidgetItem* rootDirectoryItem = ui->directories_TreeWidget->topLevelItem(rootIndex);

        if (rootDirectoryItem->data(0, directoryPathDataRole_).toString() == directoryPath)
        {
            delete ui->directories_TreeWidget->takeTopLevelItem(rootIndex);
            updateDirectoryActionStates();
            return;
        }
    }
}

void MainWindow::removeSelectedScanDirectory()
{
    const QList<QTreeWidgetItem*> selectedDirectoryItems = ui->directories_TreeWidget->selectedItems();
    QTreeWidgetItem* selectedDirectoryItem = selectedDirectoryItems.size() == 1
                                                 ? selectedDirectoryItems.constFirst()
                                                 : nullptr;

    // Child items only visualize the contents of a selected scan root. Only top-level roots can
    // be removed from the set of directories that will be scanned.
    if (!selectedDirectoryItem || selectedDirectoryItem->parent())
    {
        return;
    }

    const int rootIndex = ui->directories_TreeWidget->indexOfTopLevelItem(selectedDirectoryItem);

    if (rootIndex >= 0)
    {
        delete ui->directories_TreeWidget->takeTopLevelItem(rootIndex);
    }

    updateDirectoryActionStates();
}

const QList<DuplicateGroup>& MainWindow::getDisplayedDuplicateGroups() const
{
    return ui->resultsTab->getDisplayedDuplicateGroups();
}

ScanType MainWindow::getScanType() const
{
    const QVariant data = ui->scanType_ComboBox->currentData();

    if (!data.canConvert<ScanType>())
    {
        qFatal("Invalid or missing ScanType in scanType_ComboBox");
    }

    return data.value<ScanType>();
}

QTreeWidgetItem* MainWindow::createDirectoryTreeItem(const QString& directoryPath, const QString& displayedPath) const
{
    auto* directoryItem = new QTreeWidgetItem(QStringList{displayedPath});
    directoryItem->setData(0, directoryPathDataRole_, QDir::cleanPath(directoryPath));
    directoryItem->setData(0, directoryChildrenLoadedDataRole_, false);
    directoryItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    directoryItem->setToolTip(0, QDir::toNativeSeparators(directoryPath));

    // Show an expansion arrow only when this directory currently has child directories.
    if (!findChildDirectories(directoryPath).isEmpty())
    {
        directoryItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }

    return directoryItem;
}

QFileInfoList MainWindow::findChildDirectories(const QString& directoryPath)
{
    constexpr QDir::Filters directoryFilters = QDir::Dirs
                                               | QDir::NoDotAndDotDot
                                               | QDir::Hidden
                                               | QDir::System;

    constexpr QDir::SortFlags directorySorting = QDir::Name | QDir::IgnoreCase;

    return QDir(directoryPath).entryInfoList(directoryFilters, directorySorting);
}

void MainWindow::populateDirectoryTreeItem(QTreeWidgetItem* directoryItem) const
{
    if (!directoryItem || directoryItem->data(0, directoryChildrenLoadedDataRole_).toBool())
    {
        return;
    }

    const QString directoryPath = directoryItem->data(0, directoryPathDataRole_).toString();

    for (const QFileInfo& childDirectory: findChildDirectories(directoryPath))
    {
        directoryItem->addChild(createDirectoryTreeItem(childDirectory.absoluteFilePath(), childDirectory.fileName()));
    }

    directoryItem->setData(0, directoryChildrenLoadedDataRole_, true);
    directoryItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
}

void MainWindow::updateDirectoryActionStates()
{
    const QList<QTreeWidgetItem*> selectedDirectoryItems = ui->directories_TreeWidget->selectedItems();
    const QTreeWidgetItem* selectedDirectoryItem = selectedDirectoryItems.size() == 1
                                                       ? selectedDirectoryItems.constFirst()
                                                       : nullptr;
    const bool scanRootIsSelected = selectedDirectoryItem && !selectedDirectoryItem->parent();

    ui->removeDirectory_PushButton->setEnabled(scanRootIsSelected);
}

QString MainWindow::getInitialDirectoryScanPath() const
{
    //TODO update

    enum class TestType
    {
        SmokeTest,
        ResultPresentationTest,
        BigDirectoryTest
    };

    constexpr TestType testType = TestType::ResultPresentationTest; // adjust test type here

    const QDir fileSystemScenariosDirectory(QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../tests/file_system_scenarios"))));

    switch (testType)
    {
        case TestType::SmokeTest:
            return fileSystemScenariosDirectory.filePath(QStringLiteral("smoke_test"));
        case TestType::ResultPresentationTest:
            return fileSystemScenariosDirectory.filePath(QStringLiteral("result_presentation_test"));
        case TestType::BigDirectoryTest:
            return QStringLiteral("C:/FULL_EXTERNAL_DISK"); // TODO remove eventually
    }

    throw std::runtime_error("Invalid TestType");
}
