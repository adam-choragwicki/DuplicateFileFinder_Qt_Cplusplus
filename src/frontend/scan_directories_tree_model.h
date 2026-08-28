#pragma once

#include <QAbstractItemModel>
#include <QFileInfo>

#include <memory>
#include <vector>

/// @brief Adapts selected scan roots and their browsable children to a directory tree view.
///
/// This is a Qt presentation model, distinct from the application's authoritative `Model`. Only the
/// supplied top-level paths are scan roots; child nodes are loaded lazily and exist solely for browsing.
class ScanDirectoriesTreeModel final : public QAbstractItemModel
{
public:
    /// Additional data that consumers can request without parsing user-facing display text.
    enum DataRole
    {
        /// Clean absolute path of the directory represented by an index.
        AbsolutePathRole = Qt::UserRole
    };

    explicit ScanDirectoriesTreeModel(QObject* parent = nullptr);
    ~ScanDirectoriesTreeModel() override;

    /// Creates an index for a root or a previously loaded child directory.
    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    /// Returns the index of a directory's parent, or an invalid index for top-level scan roots.
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    /// Returns the number of currently loaded children; unloaded descendants are advertised by hasChildren().
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    /// The tree exposes a single path column at every level.
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    /// Supplies directory text, icons, tooltips, and the custom absolute-path role.
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    /// Returns the display text for the sole horizontal column header.
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    /// Returns enabled and selectable flags for valid directory indexes, and no flags for invalid indexes.
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    /// Reports potential children before loading so the view can display an expansion control.
    [[nodiscard]] bool hasChildren(const QModelIndex& parent) const override;

    /// Replaces the scan roots, discards every lazily loaded descendant, and resets the model.
    void setRootDirectoryPaths(const QStringList& directoryPaths);
    /// Returns only the configured scan roots, in presentation order; browsed descendants are excluded.
    [[nodiscard]] QStringList getRootDirectoryPaths() const;
    /// Returns whether an index represents one of the configured top-level scan roots.
    [[nodiscard]] bool isRootDirectory(const QModelIndex& index) const;
    /// Discovers and inserts an expanded directory's immediate children, at most once per node.
    void populateChildren(const QModelIndex& directoryIndex);

private:
    /// Presentation node owned by its parent's `children` collection or by `rootDirectories_`.
    /// QModelIndex stores a non-owning pointer to this object in its internalPointer field.
    struct DirectoryNode
    {
        /// Normalized absolute path used for filesystem access and AbsolutePathRole.
        QString absolutePath;
        /// Root nodes show their full path; descendants show only their directory name.
        QString displayText;
        /// Non-owning link to the parent; nullptr identifies a scan root.
        DirectoryNode* parent{};
        /// Loaded child nodes. unique_ptr keeps node addresses stable when this vector reallocates.
        std::vector<std::unique_ptr<DirectoryNode>> children;
        /// Distinguishes an unexpanded node from a loaded node whose directory is genuinely empty.
        bool childrenLoaded{};
        /// Cached discovery hint used by hasChildren() before `children` has been populated.
        bool containsChildDirectories{};
    };

    /// Returns the immediate child directories to expose below the specified path.
    [[nodiscard]] static QFileInfoList findChildDirectories(const QString& directoryPath);
    /// Returns a node's child collection, treating a null node as the invisible root of the model.
    [[nodiscard]] const std::vector<std::unique_ptr<DirectoryNode>>& childrenOf(const DirectoryNode* parentNode) const;
    /// Mutable counterpart used while constructing the tree.
    [[nodiscard]] std::vector<std::unique_ptr<DirectoryNode>>& childrenOf(DirectoryNode* parentNode);
    /// Extracts the non-owning DirectoryNode pointer stored in a valid model index.
    [[nodiscard]] static DirectoryNode* nodeFromIndex(const QModelIndex& index);
    /// Finds a node's position among its siblings so parent() can construct the correct QModelIndex.
    [[nodiscard]] int rowOfNode(const DirectoryNode* node) const;

    /// Owns both the configured scan roots and, recursively, every loaded descendant.
    std::vector<std::unique_ptr<DirectoryNode>> rootDirectories_;
};
