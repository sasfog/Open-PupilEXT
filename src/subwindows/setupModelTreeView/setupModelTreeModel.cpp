
#include "setupModelTreeModel.h"
#include "setupModelTreeItem.h"

#include <QStringList>

//using namespace Qt::Literals::StringLiterals;

SetupModelTreeModel::SetupModelTreeModel(RemoteSetupModel *remoteSetupModel, QObject *parent)
        : QAbstractItemModel(parent)
        , rootItem(std::make_unique<SetupModelTreeItem>(QVariantList{tr("Variable"), tr("Value")}))
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


void SetupModelTreeModel::recurseAddComponent(Component *component, SetupModelTreeItem *parent) {

    QVariantList columnData;
    if(component->getType() == CAMERA) {
        auto componentA = dynamic_cast<CameraComponent*>(component);
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentVendor}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentProductFamily}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentSerialNumber}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->flangeDistance)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->lensMountType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->dataInterfaceType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->operatingCurrent)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->operatingVoltage)}), parent));
        //columnData.append(QVariant(componentA->cameraConnectorPins)); // TODO
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->cameraConnectorType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->defaultFramerate)}), parent));

    } else if(component->getType() == SENSOR) {
        auto componentA = dynamic_cast<SensorComponent*>(component);

        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentVendor}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->shutterType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->isMonochrome}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->sensorTechnology}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->resolutionX)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->resolutionY)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->resolution())}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->format}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->effectiveDiagonal())}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->pixelSize)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->effectiveSizeX())}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->effectiveSizeY())}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->aspectRatio())}), parent));

    } else if(component->getType() == LENS) {
        auto componentA = dynamic_cast<LensComponent*>(component);

        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentVendor}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->suggestedSensorSizeRating}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->isVarifocal}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->focalDistanceMin)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->focalDistanceMax)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->focalDistanceActual)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->fValueMin)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->fValueMax)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->fValueActual)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->sholuderToFirstSurfaceDistance)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->outerDiameter)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->frontFilterDiameter)}), parent));

    } else if(component->getType() == ILLUMINATOR) {
        auto componentA = dynamic_cast<IlluminatorComponent*>(component);

        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->atomicComponentVendor}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->atomicComponentType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentVendor}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->numAtomicComponents)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->operatingCurrentMin)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->operatingCurrentMax)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->operatingCurrentActual)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->operatingVoltageMin)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->operatingVoltageMax)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->operatingVoltageActual)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->operatingTemperatureMin)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->operatingTemperatureMax)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->operatingTemperatureActual)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->radiationAngle)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->emissionCentroid)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->driverIsConstantCurrent}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->driverIsVariable}), parent));

    } else if(component->getType() == SCREEN) {
        auto componentA = dynamic_cast<ScreenComponent*>(component);

        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentVendor}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->resolutionX)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->resolutionY)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->physicalSizeX)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->physicalSizeY)}), parent));

        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->DPMM())}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->DPI())}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->inchSize())}), parent));

    } else if(component->getType() == FILTER) {
        auto componentA = dynamic_cast<FilterComponent*>(component);

        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentVendor}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->componentType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->mountingType}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->opticalBehaviour}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({componentA->principle}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->lowpassCuton)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->highpassCutoff)}), parent));
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->filterDiameter)}), parent));

    } else if(component->getType() == CVTARGET) {
        auto componentA = dynamic_cast<CvTargetComponent*>(component);

        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->outerRingDiameter)}), parent));

    } else if(component->getType() == EYEBALL) {
        auto componentA = dynamic_cast<EyeballComponent*>(component);

        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({QString::number(componentA->eyeballDiameter)}), parent));

    }

    parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({"Sub-components"}), parent));
    auto branchPtr = parent->child(parent->childCount() - 1); // get last child
    for( auto subComponent : component->components ) {
        branchPtr->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({"COMPONENT"}), branchPtr));
        auto subComponentPtr = branchPtr->child(branchPtr->childCount() - 1); // get last child
        recurseAddComponent(subComponent, subComponentPtr);

        qDebug() << branchPtr << "_" << subComponentPtr;
    }

}


void SetupModelTreeModel::setupModelData(RemoteSetupModel *remoteSetupModel, SetupModelTreeItem *parent) {

    qDebug() << "HEHEHE";

    qsizetype currDepth = 0;

    for( auto cameraUnit : remoteSetupModel->cameraUnits ) {
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({"Camera units"}), parent));
        auto branchPtr = parent->child(parent->childCount() - 1); // get last child
        for( auto subComponent : cameraUnit->components ) {
            branchPtr->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({"COMPONENT"}), branchPtr));
            auto subComponentPtr = branchPtr->child(branchPtr->childCount() - 1); // get last child
            recurseAddComponent(subComponent, subComponentPtr);

            qDebug() << "cameraUnits_" << branchPtr << "_" << subComponentPtr;
        }
    }

    for( auto illuminatorUnit : remoteSetupModel->illuminatorUnits ) {
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({"Illuminator units"}), parent));
        auto branchPtr = parent->child(parent->childCount() - 1); // get last child
        for( auto subComponent : illuminatorUnit->components ) {
            branchPtr->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({"COMPONENT"}), branchPtr));
            auto subComponentPtr = branchPtr->child(branchPtr->childCount() - 1); // get last child
            recurseAddComponent(subComponent, subComponentPtr);

            qDebug() << "illuminatorUnits_" << branchPtr << "_" << subComponentPtr;
        }
    }

    for( auto screenUnit : remoteSetupModel->screenUnits ) {
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({"Screen units"}), parent));
        auto branchPtr = parent->child(parent->childCount() - 1); // get last child
        for( auto subComponent : screenUnit->components ) {
            branchPtr->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({"COMPONENT"}), branchPtr));
            auto subComponentPtr = branchPtr->child(branchPtr->childCount() - 1); // get last child
            recurseAddComponent(subComponent, subComponentPtr);

            qDebug() << "screenUnits_" << branchPtr << "_" << subComponentPtr;
        }
    }

    for( auto head : remoteSetupModel->heads ) {
        parent->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({"Heads"}), parent));
        auto branchPtr = parent->child(parent->childCount() - 1); // get last child
        for( auto subComponent : head->components ) {
            branchPtr->appendChild(std::make_unique<SetupModelTreeItem>(QVariantList({"COMPONENT"}), branchPtr));
            auto subComponentPtr = branchPtr->child(branchPtr->childCount() - 1); // get last child
            recurseAddComponent(subComponent, subComponentPtr);

            qDebug() << "heads_" << branchPtr << "_" << subComponentPtr;
        }
    }

    qDebug() << "HIHIHI";

}
