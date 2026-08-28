#include "scan_directories_tree_model.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QStyle>

namespace
{
    // Enumerate all immediate subdirectories that the tree should expose, including hidden and system
    // directories but excluding the "." and ".." entries.
    [[nodiscard]] QFileInfoList findChildDirectories(const QString& directoryPath)
    {
        constexpr QDir::Filters directoryFilters = QDir::Dirs
                                                   | QDir::NoDotAndDotDot
                                                   | QDir::Hidden
                                                   | QDir::System;
        constexpr QDir::SortFlags directorySorting = QDir::Name | QDir::IgnoreCase;

        return QDir(directoryPath).entryInfoList(directoryFilters, directorySorting);
    }
}

ScanDirectoriesTreeModel::ScanDirectoriesTreeModel(QObject* parent) : QAbstractItemModel(parent) {}

ScanDirectoriesTreeModel::~ScanDirectoriesTreeModel() = default;

QModelIndex ScanDirectoriesTreeModel::index(const int row, const int column, const QModelIndex& parent) const
{
    // Every level has one column, and Qt uses an invalid index to signal an unsupported request.
    if (column != 0 || row < 0)
    {
        return {};
    }

    // A null parent node denotes the model's invisible root, whose children are the configured scan roots.
    const DirectoryNode* parentNode = nodeFromIndex(parent);
    const auto& children = childrenOf(parentNode);

    if (static_cast<std::size_t>(row) >= children.size())
    {
        return {};
    }

    // Store the node itself rather than reconstructing it from the row later. Rows are meaningful only within
    // a particular parent, whereas the pointed-to node uniquely identifies the directory in this tree.
    return createIndex(row, column, children.at(static_cast<std::size_t>(row)).get());
}

QModelIndex ScanDirectoriesTreeModel::parent(const QModelIndex& child) const
{
    const DirectoryNode* childNode = nodeFromIndex(child);

    if (!childNode || !childNode->parent)
    {
        // Invalid indexes and roots both have no visible parent.
        return {};
    }

    // Qt requires the parent's row within its own parent, not the child's row within this parent.
    return createIndex(rowOfNode(childNode->parent), 0, childNode->parent);
}

int ScanDirectoriesTreeModel::rowCount(const QModelIndex& parent) const
{
    // Only column zero can own children in a one-column tree.
    if (parent.isValid() && parent.column() != 0)
    {
        return 0;
    }

    // An unexpanded node intentionally reports zero loaded rows. hasChildren() separately tells the view
    // whether expansion may populate it.
    return static_cast<int>(childrenOf(nodeFromIndex(parent)).size());
}

int ScanDirectoriesTreeModel::columnCount(const QModelIndex&) const
{
    // The path is the only displayed property, regardless of tree depth.
    return 1;
}

QVariant ScanDirectoriesTreeModel::data(const QModelIndex& index, const int role) const
{
    const DirectoryNode* node = nodeFromIndex(index);

    if (!node || index.column() != 0)
    {
        return {};
    }

    switch (role)
    {
        case Qt::DisplayRole:
            // Roots use a full native-looking path, while descendants use their compact directory names.
            return node->displayText;

        case Qt::DecorationRole:
            // Ask the current application style for the platform-appropriate directory icon.
            return QApplication::style()->standardIcon(QStyle::SP_DirIcon);

        case Qt::ToolTipRole:
            // Tooltips favor platform-native separators for readability without changing the stored path.
            return QDir::toNativeSeparators(node->absolutePath);

        case AbsolutePathRole:
            // Callers use the normalized value for application logic instead of interpreting DisplayRole.
            return node->absolutePath;

        default:
            return {};
    }
}

QVariant ScanDirectoriesTreeModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if (section == 0 && orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        return QStringLiteral("Path");
    }

    return {};
}

Qt::ItemFlags ScanDirectoriesTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }

    /// Directory items are read-only but selectable.
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool ScanDirectoriesTreeModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        // The invisible root reports children if the model contains at least one top-level directory.
        return !rootDirectories_.empty();
    }

    const DirectoryNode* node = nodeFromIndex(parent);
    // Before expansion, use the inexpensive discovery hint that was cached when the node was created. Once
    // loaded, the actual collection is authoritative, including the valid case of an empty directory.
    return node && (node->childrenLoaded ? !node->children.empty() : node->containsChildDirectories);
}

void ScanDirectoriesTreeModel::setRootDirectoryPaths(const QStringList& directoryPaths)
{
    // Replacing roots destroys the old node tree and invalidates every QModelIndex internal pointer. A reset
    // brackets that lifetime change and makes attached views discard their cached indexes and expansion state.
    beginResetModel();
    rootDirectories_.clear();
    rootDirectories_.reserve(static_cast<std::size_t>(directoryPaths.size()));

    for (const QString& directoryPath: directoryPaths)
    {
        const QString normalizedDirectoryPath = QDir::cleanPath(QDir(directoryPath).absolutePath());
        auto rootDirectory = std::make_unique<DirectoryNode>();
        rootDirectory->absolutePath = normalizedDirectoryPath;
        rootDirectory->displayText = QDir::toNativeSeparators(normalizedDirectoryPath);
        // Probe one level ahead so hasChildren() can show an expansion control without loading child nodes yet.
        rootDirectory->containsChildDirectories = !findChildDirectories(normalizedDirectoryPath).isEmpty();
        rootDirectories_.push_back(std::move(rootDirectory));
    }

    endResetModel();
}

QStringList ScanDirectoriesTreeModel::getRootDirectoryPaths() const
{
    QStringList directoryPaths;
    directoryPaths.reserve(static_cast<qsizetype>(rootDirectories_.size()));

    // Iterating only rootDirectories_ keeps browsed descendants out of the actual scan configuration.
    for (const auto& rootDirectory: rootDirectories_)
    {
        directoryPaths.append(rootDirectory->absolutePath);
    }

    return directoryPaths;
}

bool ScanDirectoriesTreeModel::isRootDirectory(const QModelIndex& index) const
{
    // A valid index directly below the invisible root is one of the user-configured scan directories.
    return index.isValid() && !index.parent().isValid();
}

void ScanDirectoriesTreeModel::populateChildren(const QModelIndex& directoryIndex)
{
    DirectoryNode* directoryNode = nodeFromIndex(directoryIndex);

    // Expansion signals may be repeated. Never enumerate or insert the same directory twice.
    if (!directoryNode || directoryNode->childrenLoaded)
    {
        return;
    }

    const QFileInfoList childDirectories = findChildDirectories(directoryNode->absolutePath);
    // Mark the attempt complete even when no children are found, preventing repeated filesystem queries for
    // empty or inaccessible directories on every expansion attempt.
    directoryNode->childrenLoaded = true;
    directoryNode->containsChildDirectories = !childDirectories.isEmpty();

    if (childDirectories.isEmpty())
    {
        return;
    }

    // Reserve storage before insertion, although unique_ptr already keeps individual node addresses stable.
    beginInsertRows(directoryIndex, 0, static_cast<int>(childDirectories.size()) - 1);
    directoryNode->children.reserve(static_cast<std::size_t>(childDirectories.size()));

    for (const QFileInfo& childDirectory: childDirectories)
    {
        auto childNode = std::make_unique<DirectoryNode>();
        childNode->absolutePath = QDir::cleanPath(childDirectory.absoluteFilePath());
        childNode->displayText = childDirectory.fileName();
        childNode->parent = directoryNode;
        // Cache whether this child is expandable, but defer allocating its child nodes until it is expanded.
        childNode->containsChildDirectories = !findChildDirectories(childNode->absolutePath).isEmpty();
        directoryNode->children.push_back(std::move(childNode));
    }

    endInsertRows();
}

const std::vector<std::unique_ptr<ScanDirectoriesTreeModel::DirectoryNode>>&
ScanDirectoriesTreeModel::childrenOf(const DirectoryNode* parentNode) const
{
    // Qt represents the invisible root with an invalid QModelIndex, which nodeFromIndex() maps to null.
    return parentNode ? parentNode->children : rootDirectories_;
}

std::vector<std::unique_ptr<ScanDirectoriesTreeModel::DirectoryNode>>&
ScanDirectoriesTreeModel::childrenOf(DirectoryNode* parentNode)
{
    return parentNode ? parentNode->children : rootDirectories_;
}

ScanDirectoriesTreeModel::DirectoryNode* ScanDirectoriesTreeModel::nodeFromIndex(const QModelIndex& index)
{
    // internalPointer is non-owning; node ownership remains entirely within the unique_ptr tree.
    return index.isValid() ? static_cast<DirectoryNode*>(index.internalPointer()) : nullptr;
}

int ScanDirectoriesTreeModel::rowOfNode(const DirectoryNode* node) const
{
    const auto& siblings = childrenOf(node ? node->parent : nullptr);

    // Nodes do not cache row numbers because sibling collections are the source of truth for presentation order.
    for (std::size_t row = 0; row < siblings.size(); ++row)
    {
        if (siblings.at(row).get() == node)
        {
            return static_cast<int>(row);
        }
    }

    // A node owned by this model must be present among its parent's children; -1 exposes a broken invariant.
    return -1;
}
