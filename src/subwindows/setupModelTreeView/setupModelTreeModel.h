
#pragma once

#include <memory> // unique_ptr

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

#include<QDebug>

#include "../../remoteSetupModel.h"
//#include "setupModelTreeItem.h"

class SetupModelTreeItem;

class SetupModelTreeModel : public QAbstractItemModel {
Q_OBJECT

public:
    Q_DISABLE_COPY_MOVE(SetupModelTreeModel)

    explicit SetupModelTreeModel(RemoteSetupModel *remoteSetupModel, QObject *parent = nullptr);
    ~SetupModelTreeModel() override;

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;

private:
    static void setupModelData(RemoteSetupModel *remoteSetupModel, SetupModelTreeItem *parent);

    std::unique_ptr<SetupModelTreeItem> rootItem;

    static void recurseAddComponent(Component *component, SetupModelTreeItem *parent);
};

