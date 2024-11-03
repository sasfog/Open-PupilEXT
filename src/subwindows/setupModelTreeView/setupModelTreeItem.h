
#pragma once

#include <memory> // unique_ptr

#include <QVariant>
#include <QList>

class SetupModelTreeItem {

public:
    explicit SetupModelTreeItem(QVariantList data, SetupModelTreeItem *parentItem = nullptr);
    //explicit SetupModelTreeItem(SetupModelTreeItem *parentItem = nullptr);

    void appendChild(std::unique_ptr<SetupModelTreeItem> &&child);

    SetupModelTreeItem *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    SetupModelTreeItem *parentItem();

private:
    std::vector<std::unique_ptr<SetupModelTreeItem>> m_childItems;
    QVariantList m_itemData;
    SetupModelTreeItem *m_parentItem;

};

