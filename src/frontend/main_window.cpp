#include "main_window.h"
#include "results_tab.h"
#include "scan_directories_tree_model.h"
#include "ui_main_window.h"

#include <QComboBox>
#include <QDir>
#include <QMessageBox>
#include <QScreen>
#include <QTabBar>
#include <QToolButton>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    directoriesTreeModel_ = new ScanDirectoriesTreeModel(this);

    setWindowTitle("Duplicate file finder");

    initializeWindowGeometry();
    initializeUI();

    connect(ui->addDirectory_PushButton, &QPushButton::clicked, this, &MainWindow::addDirectoryButtonClicked);
    connect(ui->removeDirectory_PushButton, &QPushButton::clicked, this, &MainWindow::removeDirectoryButtonClicked);
    connect(ui->resultsTab, &ResultsTab::revealFileInSystemFileManagerRequested, this, &MainWindow::revealFileInSystemFileManagerRequested);
    connect(ui->startScan_PushButton, &QPushButton::clicked, this, &MainWindow::startScanButtonClicked);

    connect(ui->exportToHtml_Action, &QAction::triggered, this, &MainWindow::exportToHtmlRequested);
    connect(ui->quit_Action, &QAction::triggered, this, &QWidget::close);
    connect(ui->aboutDuplicateFileFinder_Action, &QAction::triggered, this, &MainWindow::showAboutDialog);

    connect(ui->directories_TreeView, &QTreeView::expanded,
            directoriesTreeModel_, &ScanDirectoriesTreeModel::populateChildren);
    connect(ui->directories_TreeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateDirectoryActionStates);
    connect(ui->scanType_ComboBox, &QComboBox::currentIndexChanged, this,
            [this]
            {
                updateScanTypeDescription();
                emit scanTypeSelectionChanged(getScanTypeFromComboBox());
            });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initializeWindowGeometry()
{
    constexpr int initialScreenWidthPercentage = 75;
    constexpr int initialScreenHeightPercentage = 75;

    const QScreen* initialScreen = QGuiApplication::primaryScreen();
    if (!initialScreen)
    {
        // A GUI application normally has a primary screen. Keep a usable fallback for unusual platform
        // integrations or tests that provide no screen geometry.
        resize(800, 500);
        return;
    }

    // availableGeometry() excludes desktop-reserved areas such as the taskbar or system dock. Qt expresses it
    // in device-independent pixels, so the percentage remains appropriate when display scaling is enabled.
    const QRect availableGeometry = initialScreen->availableGeometry();

    const QSize initialSize{availableGeometry.width() * initialScreenWidthPercentage / 100,
                            availableGeometry.height() * initialScreenHeightPercentage / 100};

    resize(initialSize);

    // Position the initial window in the center of the usable screen area.
    move(availableGeometry.center() - QPoint(initialSize.width() / 2, initialSize.height() / 2));
}

void MainWindow::initializeUI()
{
    // Clear placeholder values from combo boxes
    ui->scanType_ComboBox->clear();

    populateScanTypeComboBox();
    initializeDirectoriesTree();
    initializeResultsTabCloseButton();
    clearScanResult(); // result tab is not visible from the start
}

void MainWindow::initializeDirectoriesTree()
{
    ui->directories_TreeView->setModel(directoriesTreeModel_);
    ui->directories_TreeView->setUniformRowHeights(true);
    ui->directories_TreeView->clearSelection();
    updateDirectoryActionStates();
}

void MainWindow::populateScanTypeComboBox()
{
    ui->scanType_ComboBox->addItem("By file content", QVariant::fromValue(ScanType::ByFileContent));
    ui->scanType_ComboBox->addItem("By file name", QVariant::fromValue(ScanType::ByFileName));

    // Center align text
    for (int i = 0; i < ui->scanType_ComboBox->count(); ++i)
    {
        ui->scanType_ComboBox->setItemData(i, Qt::AlignCenter, Qt::TextAlignmentRole);
    }

    updateScanTypeDescription();
}

void MainWindow::updateScanTypeDescription() const
{
    switch (getScanTypeFromComboBox())
    {
        case ScanType::ByFileName:
            ui->scanTypeDescription_Label->setText(QStringLiteral("Finds files with matching names, ignoring letter case and the final extension. "
                "Their contents may differ."));
            break;

        case ScanType::ByFileContent:
            ui->scanTypeDescription_Label->setText(QStringLiteral("Finds files with exactly identical contents, regardless of their names."));
            break;
    }
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
    if (scanResult.getDuplicateGroups().isEmpty())
    {
        clearScanResult();
        return;
    }

    const int resultsTabIndex = ui->main_TabWidget->indexOf(ui->resultsTab);
    ui->main_TabWidget->setTabVisible(resultsTabIndex, true);
    ui->resultsTab->showDuplicateGroups(scanResult.getDuplicateGroups());
    ui->exportToHtml_Action->setEnabled(true);
    ui->main_TabWidget->setCurrentWidget(ui->resultsTab);
}

void MainWindow::clearScanResult()
{
    ui->resultsTab->clearDuplicateGroups();
    ui->exportToHtml_Action->setEnabled(false);
    ui->main_TabWidget->setCurrentWidget(ui->directoriesTab);

    const int resultsTabIndex = ui->main_TabWidget->indexOf(ui->resultsTab);
    ui->main_TabWidget->setTabVisible(resultsTabIndex, false);
}

QStringList MainWindow::getScanDirectoryPaths() const
{
    return directoriesTreeModel_->getRootDirectoryPaths();
}

QString MainWindow::getSelectedScanDirectoryPath() const
{
    const QModelIndexList selectedDirectoryIndexes = ui->directories_TreeView->selectionModel()->selectedRows();

    if (selectedDirectoryIndexes.size() != 1
        || !directoriesTreeModel_->isRootDirectory(selectedDirectoryIndexes.constFirst()))
    {
        return {};
    }

    return selectedDirectoryIndexes.constFirst().data(ScanDirectoriesTreeModel::AbsolutePathRole).toString();
}

void MainWindow::setScanDirectoryPaths(const QStringList& directoryPaths)
{
    directoriesTreeModel_->setRootDirectoryPaths(directoryPaths);

    if (directoriesTreeModel_->rowCount() > 0)
    {
        ui->directories_TreeView->setCurrentIndex(directoriesTreeModel_->index(directoriesTreeModel_->rowCount() - 1, 0));
    }

    updateDirectoryActionStates();
}

void MainWindow::addScanDirectory(const QString& directoryPath)
{
    QStringList directoryPaths = directoriesTreeModel_->getRootDirectoryPaths();
    directoryPaths.append(QDir(directoryPath).absolutePath());
    setScanDirectoryPaths(directoryPaths);
}

void MainWindow::removeScanDirectory(const QString& directoryPath)
{
    QStringList directoryPaths = directoriesTreeModel_->getRootDirectoryPaths();
    directoryPaths.removeAll(QDir(directoryPath).absolutePath());
    setScanDirectoryPaths(directoryPaths);
}

void MainWindow::removeSelectedScanDirectory()
{
    // Child items only visualize the contents of a selected scan root. Only top-level roots can
    // be removed from the set of directories that will be scanned.
    const QString selectedScanDirectoryPath = getSelectedScanDirectoryPath();

    if (selectedScanDirectoryPath.isEmpty())
    {
        return;
    }

    removeScanDirectory(selectedScanDirectoryPath);
}

const QList<DuplicateGroup>& MainWindow::getDisplayedDuplicateGroups() const
{
    return ui->resultsTab->getDisplayedDuplicateGroups();
}

ScanType MainWindow::getScanTypeFromComboBox() const
{
    const QVariant data = ui->scanType_ComboBox->currentData();

    if (!data.canConvert<ScanType>())
    {
        qFatal("Invalid or missing ScanType in scanType_ComboBox");
    }

    return data.value<ScanType>();
}

void MainWindow::setScanTypeInComboBox(const ScanType scanType)
{
    const int scanTypeIndex = ui->scanType_ComboBox->findData(QVariant::fromValue(scanType));

    if (scanTypeIndex < 0)
    {
        qFatal("Cannot display an unknown ScanType value: %d", static_cast<int>(scanType));
    }

    ui->scanType_ComboBox->setCurrentIndex(scanTypeIndex);
}

void MainWindow::updateDirectoryActionStates()
{
    const QModelIndexList selectedDirectoryIndexes = ui->directories_TreeView->selectionModel()->selectedRows();
    const bool scanRootIsSelected = selectedDirectoryIndexes.size() == 1
                                    && directoriesTreeModel_->isRootDirectory(selectedDirectoryIndexes.constFirst());

    ui->removeDirectory_PushButton->setEnabled(scanRootIsSelected);
}

void MainWindow::initializeResultsTabCloseButton()
{
    auto* closeButton = new QToolButton(ui->main_TabWidget->tabBar());
    closeButton->setObjectName(QStringLiteral("closeResultsTab_ToolButton"));
    closeButton->setIcon(style()->standardIcon(QStyle::SP_TabCloseButton));
    closeButton->setIconSize(QSize(12, 12));
    closeButton->setToolTip(QStringLiteral("Close Results"));
    closeButton->setAccessibleName(QStringLiteral("Close Results tab"));
    closeButton->setFixedSize(12, 12);
    closeButton->setStyleSheet(QStringLiteral(R"(
        QToolButton {
            background: transparent;
            border: none;
            margin: 0;
            padding: 0;
        }
        QToolButton:hover {
            background-color: rgba(128, 128, 128, 45);
        }
        QToolButton:pressed {
            background-color: rgba(128, 128, 128, 85);
        }
    )"));

    const int resultsTabIndex = ui->main_TabWidget->indexOf(ui->resultsTab);
    ui->main_TabWidget->tabBar()->setTabButton(resultsTabIndex, QTabBar::RightSide, closeButton);

    connect(closeButton, &QToolButton::clicked, this, &MainWindow::scanResultTabCloseRequested);
}
