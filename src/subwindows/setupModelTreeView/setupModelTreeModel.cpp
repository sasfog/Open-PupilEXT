
#include "setupModelTreeModel.h"
#include "setupModelTreeItem.h"

#include <QStringList>

//using namespace Qt::Literals::StringLiterals;

SetupModelTreeModel::SetupModelTreeModel(RemoteSetupModel *remoteSetupModel, QObject *parent)
        : QAbstractItemModel(parent)
        , rootItem(std::make_unique<SetupModelTreeItem>(QVariantList{tr("Title"), tr("Summary")}))
        //, rootItem(std::make_unique<SetupModelTreeItem>())
        {
    setupModelData(remoteSetupModel, rootItem.get());
}

SetupModelTreeModel::~SetupModelTreeModel() = default;

int SetupModelTreeModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return static_cast<SetupModelTreeItem*>(parent.internalPointer())->columnCount();
    return rootItem->columnCount();
}

QVariant SetupModelTreeModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole)
        return {};

    const auto *item = static_cast<const SetupModelTreeItem*>(index.internalPointer());
    return item->data(index.column());
}

Qt::ItemFlags SetupModelTreeModel::flags(const QModelIndex &index) const {
    return index.isValid()
           ? QAbstractItemModel::flags(index) : Qt::ItemFlags(Qt::NoItemFlags);
}

QVariant SetupModelTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    return orientation == Qt::Horizontal && role == Qt::DisplayRole
           ? rootItem->data(section) : QVariant{};
}

QModelIndex SetupModelTreeModel::index(int row, int column, const QModelIndex &parent) const {
    if (!hasIndex(row, column, parent))
        return {};

    SetupModelTreeItem *parentItem = parent.isValid()
                           ? static_cast<SetupModelTreeItem*>(parent.internalPointer())
                           : rootItem.get();

    if (auto *childItem = parentItem->child(row))
        return createIndex(row, column, childItem);
    return {};
}

QModelIndex SetupModelTreeModel::parent(const QModelIndex &index) const {
    if (!index.isValid())
        return {};

    auto *childItem = static_cast<SetupModelTreeItem*>(index.internalPointer());
    SetupModelTreeItem *parentItem = childItem->parentItem();

    return parentItem != rootItem.get()
           ? createIndex(parentItem->row(), 0, parentItem) : QModelIndex{};
}

int SetupModelTreeModel::rowCount(const QModelIndex &parent) const {
    if (parent.column() > 0)
        return 0;

    const SetupModelTreeItem *parentItem = parent.isValid()
                                 ? static_cast<const SetupModelTreeItem*>(parent.internalPointer())
                                 : rootItem.get();

    return parentItem->childCount();
}

void SetupModelTreeModel::setupModelData(RemoteSetupModel *remoteSetupModel, SetupModelTreeItem *parent) {
    struct ParentIndentation
    {
        SetupModelTreeItem *parent;
        qsizetype indentation;
    };

    /*
    QList<ParentIndentation> state{{parent, 0}};

    for (const auto &line : lines) {
        qsizetype position = 0;
        for ( ; position < line.length() && line.at(position).isSpace(); ++position) {
        }

        const QStringView lineData = line.sliced(position).trimmed();
        if (!lineData.isEmpty()) {
            // Read the column data from the rest of the line.
            const auto columnStrings = lineData.split(u'\t', Qt::SkipEmptyParts);
            QVariantList columnData;
            columnData.reserve(columnStrings.count());
            for (const auto &columnString : columnStrings)
                columnData << columnString.toString();

            if (position > state.constLast().indentation) {
                // The last child of the current parent is now the new parent
                // unless the current parent has no children.
                auto *lastParent = state.constLast().parent;
                if (lastParent->childCount() > 0)
                    state.append({lastParent->child(lastParent->childCount() - 1), position});
            } else {
                while (position < state.constLast().indentation && !state.isEmpty())
                    state.removeLast();
            }

            // Append a new item to the current parent's list of children.
            auto *lastParent = state.constLast().parent;
            lastParent->appendChild(std::make_unique<SetupModelTreeItem>(columnData, lastParent));
        }
    }
     */
}
