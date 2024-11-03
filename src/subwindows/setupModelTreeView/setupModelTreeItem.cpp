
#include "setupModelTreeItem.h"


SetupModelTreeItem::SetupModelTreeItem(QVariantList data, SetupModelTreeItem *parent)
        : m_itemData(std::move(data)), m_parentItem(parent)
{}

void SetupModelTreeItem::appendChild(std::unique_ptr<SetupModelTreeItem> &&child)
{
    m_childItems.push_back(std::move(child));
}

SetupModelTreeItem *SetupModelTreeItem::child(int row)
{
    return row >= 0 && row < childCount() ? m_childItems.at(row).get() : nullptr;
}

int SetupModelTreeItem::childCount() const
{
    return int(m_childItems.size());
}

int SetupModelTreeItem::columnCount() const
{
    return int(m_itemData.count());
}

QVariant SetupModelTreeItem::data(int column) const
{
    return m_itemData.value(column);
}

SetupModelTreeItem *SetupModelTreeItem::parentItem()
{
    return m_parentItem;
}

int SetupModelTreeItem::row() const
{
    if (m_parentItem == nullptr)
        return 0;
    const auto it = std::find_if(m_parentItem->m_childItems.cbegin(), m_parentItem->m_childItems.cend(),
                                 [this](const std::unique_ptr<SetupModelTreeItem> &treeItem) {
                                     return treeItem.get() == this;
                                 });

    if (it != m_parentItem->m_childItems.cend())
        return std::distance(m_parentItem->m_childItems.cbegin(), it);
    Q_ASSERT(false); // should not happen
    return -1;
}

