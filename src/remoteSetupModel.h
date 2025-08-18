
#pragma once

#include <QtCore/QObject>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QVector3D>
#include "pupil-detection-methods/Pupil.h"
#include "./subwindows/QJsonModel/QJsonModel.hpp"
#include "subwindows/QtOpenGLViewer/QtOpenGLViewer.h"

#include <QtMath>

#include <QSettings>
#include <QCoreApplication>



enum cameraConnectorPinPurpose {
    PIN_INPUT,
    PIN_OUTPUT,
    PIN_GPIO,
    PIN_GROUND,
    PIN_RESERVED // = NC = not connected
};

struct CameraConnectorPin {
    char physicalPinNumber = 0; // 0 is invalid by default
    char softwareLineNumber = 0; // 0 is invalid by default
    cameraConnectorPinPurpose purpose = PIN_RESERVED;
    bool isOptoCoupled = false;
};

struct ComponentDataCitation {
    QString link;
    QString lastAccessed;
};

/*
enum SetupModelComponentType {
    CAMERA,
    SENSOR,
    LENS,
    ILLUMINATOR,
    FILTER,
    SCREEN,
    CVTARGET,
    EYEBALL
};
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO:    - REMOVE dimX, dimY, dimZ, and add a getBoundingBox() virtual method instead
//          - access each components special dimensions through getters or properties, e.g. lens diameter, not using dimX, dimY...
//          - use 3D data type instead of 3 separate variables

class RemoteSetupModel : public QObject {
Q_OBJECT
public:
    explicit RemoteSetupModel(QString jsonFile, QtOpenGLViewer* GLViewer, QObject *parent = 0) : QObject(parent){
        bool success = rep->load(jsonFile);
        if(!success) {
            //success = rep->load(":/default.json"); // TODO, in resources
            // TODO !!
        }

        if(!success)
            throw std::runtime_error("json not found");
        //linkDataRSM();

        rep->makeKeysFriendly();
        //rep->modelReset();

        linkedGLViewer = GLViewer;

        mapGUITreeTo3DView();
    };
    ~RemoteSetupModel() override {};

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    QJsonModel* rep = new QJsonModel;
    //QList<QJsonTreeItem*> GLViewRepresentedItems = nullptr;

    QtOpenGLViewer* linkedGLViewer = nullptr;


    // TODO: beautify this thing. Perhaps knowing "ProcMode procMode" would be needed
    // Returns the first matching occurence
    QJsonTreeItem* findTreeItem(const QString &scope, const QString &key) {

        // Get all items that have at least one child, that has
        // the key (starting with the QString in the key) we specified.
        auto allPossible = rep->findItemsByChildKey(key);

        // NOTE:
        // This level of search cannot directly happen inside the QJsonModel, so it is here instead.
        // The reason is that, in later to-be-added, more complicated hardware setups (more than one illuminator, etc)
        // it would be necessary to decide on more than one high-level properties to choose the right item in search.

        if(scope == "LEFT_EYE") {
            for (int i = 0; i < allPossible.size(); i++) {
                if (//allPossible[i]->key().startsWith(key) &&
                    allPossible[i]->/*parent()->*/key().startsWith("Eyeball") &&
                    allPossible[i]->/*parent()->*/hasChildWithKeyAndValue("AnatomicalPosition", "Left")) {
                    return allPossible[i];
                }
            }
        } else if(scope == "RIGHT_EYE") {
            for (int i = 0; i < allPossible.size(); i++) {
                if (//allPossible[i]->key().startsWith(key) &&
                    allPossible[i]->/*parent()->*/key().startsWith("Eyeball") &&
                    allPossible[i]->/*parent()->*/hasChildWithKeyAndValue("AnatomicalPosition", "Right")) {
                    return allPossible[i];
                }
            }
        } else if(scope == "CVTARGET") {
            for (int i = 0; i < allPossible.size(); i++) {
                if (//allPossible[i]->key().startsWith(key) &&
                    allPossible[i]->/*parent()->*/key().startsWith("CV Target")) {
                    return allPossible[i];
                }
            }
        } else if(scope == "HEAD") {
            for (int i = 0; i < allPossible.size(); i++) {
                if (//allPossible[i]->key().startsWith(key) &&
                    allPossible[i]->/*parent()->*/key().startsWith("Head")) {
                    return allPossible[i];
                }
            }
        } else if(scope == "CAMERA") {
            for (int i = 0; i < allPossible.size(); i++) {
                if (//allPossible[i]->key().startsWith(key) &&
                    allPossible[i]->/*parent()->*/key().startsWith("Camera")) {
                    return allPossible[i];
                }
            }
        } else if(scope == "ILLUMINATOR") {
            for (int i = 0; i < allPossible.size(); i++) {
                if (//allPossible[i]->key().startsWith(key) &&
                    allPossible[i]->/*parent()->*/key().startsWith("Illuminator")) {
                    return allPossible[i];
                }
            }
        }
        return nullptr;
    }

    bool subscribeInbound(QObject *sender, const char *method, QJsonTreeItem* item, Qt::ConnectionType type = Qt::AutoConnection) {
        if(!item)
            return false;
        return connect(sender, method, item, SLOT(setValue(QVariant)), type);
    };
    bool subscribeOutbound(QObject *receiver, const char *method, QJsonTreeItem* item, Qt::ConnectionType type = Qt::AutoConnection) {
        if(!item)
            return false;
        return connect(item, SIGNAL(valueChanged(QVariant)), receiver, method, type);
    };
    bool subscribeInbound(QObject *sender, const char *method, const QString &scope, const QString &key, Qt::ConnectionType type = Qt::AutoConnection) {
        QJsonTreeItem* item = findTreeItem(scope, key);
        if(!item)
            return false;
        return connect(sender, method, item, SLOT(setValue(QVariant)), type);
    };
    bool subscribeOutbound(QObject *receiver, const char *method, const QString &scope, const QString &key, Qt::ConnectionType type = Qt::AutoConnection) {
        QJsonTreeItem* item = findTreeItem(scope, key);
        if(!item)
            return false;
        return connect(item, SIGNAL(valueChanged(QVariant)), receiver, method, type);
    };
    //
    bool unsubscribeInbound(QObject *sender, const char *method, QJsonTreeItem* item) {
        if(!item)
            return false;
        return connect(sender, method, item, SLOT(setValue(QVariant)));
    };

    bool unsubscribeOutbound(QObject *receiver, const char *method, QJsonTreeItem* item) {
        if(!item)
            return false;
        return disconnect(item, SIGNAL(valueChanged(QVariant)), receiver, method);
    };
    bool unsubscribeInbound(QObject *sender, const char *method, const QString &scope, const QString &key) {
        QJsonTreeItem* item = findTreeItem(scope, key);
        if(!item)
            return false;
        return disconnect(sender, method, item, SLOT(setValue(QVariant)));
    };
    bool unsubscribeOutbound(QObject *receiver, const char *method, const QString &scope, const QString &key) {
        QJsonTreeItem* item = findTreeItem(scope, key);
        if(!item)
            return false;
        return disconnect(item, SIGNAL(valueChanged(QVariant)), receiver, method);
    };
    //bool subscribeBothWays ...

    QList<QJsonTreeItem*> findItemsByChildKey(const QString &key) {
        return rep->findItemsByChildKey(key);
    }
    QList<QJsonTreeItem*> findItemsByKey(const QString &key) {
        return rep->findItemsByKey(key);
    }
    QList<QJsonTreeItem*> findItemsByKeyAndParentKey(const QString &key, const QString &parentKey) {
        return rep->findItemsByKeyAndParentKey(key, parentKey);
    }

    void mapGUITreeTo3DView() {

        // get everything which has a dimension, so needs to be shown in the 3D view
        QList<QJsonTreeItem*> items3D = rep->findItemsByChildKey("Dim");
        QJsonTreeItem* p;

        for (int i = 0; i < items3D.size(); i++) {
            p = items3D[i]->parent();

            qDebug() << p->key();
            qDebug() << p->childrenWithKey("Dim")[0]->value();
            qDebug() << p->childrenWithKey("Loc")[0]->value();
            qDebug() << p->childrenWithKey("Rot")[0]->value();
            qDebug() << "------------";

            if (p->key().startsWith("Eyeball")) {
                linkedGLViewer->addToScene({SPHEROID, p->childrenWithKey("Dim")[0], p->childrenWithKey("Loc")[0], p->childrenWithKey("Rot")[0], false });
                //connect(p, SIGNAL(valueChanged(QJsonTreeItem)), receiver, method, type);
                //connect(highlightGeom(QJsonTreeItem* p)
            } else if (p->key().startsWith("CV Target")) {
                linkedGLViewer->addToScene({CYLINDER, p->childrenWithKey("Dim")[0], p->childrenWithKey("Loc")[0], p->childrenWithKey("Rot")[0], false });
            } else if (p->key().startsWith("Camera")) {
                linkedGLViewer->addToScene({CUBOID, p->childrenWithKey("Dim")[0], p->childrenWithKey("Loc")[0], p->childrenWithKey("Rot")[0], false });
            } else if (p->key().startsWith("Sensor")) {
                linkedGLViewer->addToScene({CUBOID, p->childrenWithKey("Dim")[0], p->childrenWithKey("Loc")[0], p->childrenWithKey("Rot")[0], false });
            } else if (p->key().startsWith("Lens")) {
                linkedGLViewer->addToScene({CYLINDER, p->childrenWithKey("Dim")[0], p->childrenWithKey("Loc")[0], p->childrenWithKey("Rot")[0], false });
            } else if (p->key().startsWith("Filter")) {
                linkedGLViewer->addToScene({CYLINDER, p->childrenWithKey("Dim")[0], p->childrenWithKey("Loc")[0], p->childrenWithKey("Rot")[0], false });
            } else if (p->key().startsWith("Illuminator")) {
                linkedGLViewer->addToScene({CUBOID, p->childrenWithKey("Dim")[0], p->childrenWithKey("Loc")[0], p->childrenWithKey("Rot")[0], false });
            } else if (p->key().startsWith("Screen")) {
                linkedGLViewer->addToScene({CUBOID, p->childrenWithKey("Dim")[0], p->childrenWithKey("Loc")[0], p->childrenWithKey("Rot")[0], false });
            }
        }

        // TODO: LINK ALL DIM, LOC, ROT item value changes to trigger GLView refresh and treeview/model refresh
        // connect(selectionModel, SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)), this, SLOT(mySelectionChanged(const QItemSelection&,const QItemSelection&)));

    }

    /*
    void resetModelRSM() {
        // TODO
    };

    bool isInitializedRSM() {
        return (!cameraUnits.isEmpty() && !illuminatorUnits.isEmpty() && !screenUnits.isEmpty() && !heads.isEmpty());
    };
     */

    bool isValidRSM() {
        // TODO: iteratively check if the model is "valid" so no impossible values exist, and can be used to start gaze tracking with
        return true;
    };

    // TODO: ASAP 2025.7.29.
/*
    // TODO: MOVE TO GEOMETRY
    // TODO: the IPD should be constant, so there is no actual need for a function like this,
    //      although could be used for checking, as we will sometimes update eye locations
    float getInterPupillaryDistance() {
        if(heads.size() < 1)
            return -1.0f;

        std::vector<EyeballComponent*> eyes;

        for(int i=0; i<heads[0]->components.size(); i++) {
            if(heads[0]->components[i]->getType() == EYEBALL) {
                eyes.push_back(dynamic_cast<EyeballComponent *>(heads[0]->components[i]));
            }
        }
        if(eyes.size() < 2)
            return -1.0f;

        return hypot(hypot(eyes[0]->loc.x()-eyes[1]->loc.x(),eyes[0]->loc.y()-eyes[1]->loc.y()),eyes[0]->loc.z()-eyes[1]->loc.z());
    };
    */

    //struct QEPoint {short x = 0; float y = 0;};
/*
signals:

    // When: 1, structure changes, e.g. the distance between the illuminator and camera OR 2, some component property (e.g. focal length) changes
    void staticGeometryChanged();
    // When e.g. the head position is changed
    void dynamicGeometryChanged();
*/




/*
    // Get dim and loc
    std::tuple<QVector3D, QVector3D> dummy_getScreenParams() {



        return std::tuple<QVector3D, QVector3D> { QVector3D(0,0,0), QVector3D(0,0,0) };

        return std::tuple<QVector3D, QVector3D> {
                setupModel->screenUnits[0]->components[0]->dim,
                setupModel->screenUnits[0]->components[0]->loc
        };
    };

    // Get loc
    QVector3D dummy_getCamMidLoc() {
        if(!setupModel || !setupModel->isInitialized() || !setupModel->isValid() || setupModel->cameraUnits.empty() || setupModel->cameraUnits[0]->components.empty() )
            return QVector3D(0,0,0);

        QVector3D avgLoc = setupModel->cameraUnits[0]->components[0]->loc;
        for( int i = 1; setupModel->cameraUnits.size() < 1; i++ ) {
            avgLoc = (avgLoc + setupModel->cameraUnits[0]->components[0]->loc) / 2.0f;
        }

        return avgLoc;
    };

    float dummy_getHeadScreenDist() {
        if(!setupModel || !setupModel->isInitialized() || !setupModel->isValid() ||
           setupModel->screenUnits.empty() || setupModel->screenUnits[0]->components.empty() ||
           setupModel->heads.empty() || setupModel->heads[0]->components.empty()   )
            return 0.0f;

        QVector3D screenCenter =
                ((setupModel->screenUnits[0]->components[0]->dim / 2.0f) +
                 setupModel->screenUnits[0]->components[0]->loc);
        QVector3D firstRandomheadComponentCenter =
                ((setupModel->heads[0]->components[0]->dim / 2.0f) +
                 setupModel->heads[0]->components[0]->loc);
        return abs( screenCenter.distanceToPoint( firstRandomheadComponentCenter ) );
    };
*/






/*
    void trivi_sensorChanged

    void trivi_setDefaultViews() {

        std::tuple<QVector3D, QVector3D> screenParams = dummy_getScreenParams();

        QVector3D camMidLoc = dummy_getCamMidLoc();

        float headScreenDist = dummy_getHeadScreenDist();

        switch(viewNumber) {
            case 1:
                // XY plane -- from front
                // just arbitrary values of Z yet
                camera.eye = QVector3D(
                        std::get<1>(screenParams).x() /2.0f,
                        std::get<1>(screenParams).y() /2.0f + 60.0f,
                        headScreenDist // yet arbitrary -- "zoom" dist
                );
                camera.center = QVector3D(
                        camera.eye.x(),
                        camera.eye.y(),
                        0
                );
                camera.up = QVector3D(0, 10, 0);
                break;
            case 2:
                // ZY plane -- from side
                camera.eye = QVector3D(
                        headScreenDist, // yet arbitrary -- "zoom" dist
                        std::get<1>(screenParams).y() /2.0f + 60.0f,
                        headScreenDist /2.0f
                );
                camera.center = QVector3D(
                        0,
                        camera.eye.y(),
                        camera.eye.z()
                );
                camera.up = QVector3D(0, 1, 0);
                break;
            case 3:
                // ZX plane -- from top
                camera.eye = QVector3D(
                        0,
                        headScreenDist *1.1f, // yet arbitrary -- "zoom" dist
                        headScreenDist /2.0f
                );
                camera.center = QVector3D(
                        camera.eye.x(),
                        0,
                        camera.eye.z()
                );
                camera.up = QVector3D(0, 0, -1);
                break;
            case 4:

                QVector3D imaginaryBorundaryBoxDims = {
                        std::get<1>(screenParams).x(),
                        std::get<1>(screenParams).y() *2.5f,
                        headScreenDist *0.9f // yet arbitrary
                };

                camera.eye = QVector3D(
                        qSqrt(3)* (headScreenDist *0.4f) + imaginaryBorundaryBoxDims.x() /2.0f,
                        qSqrt(3)* (headScreenDist *0.4f) + imaginaryBorundaryBoxDims.y() /2.0f,
                        qSqrt(3)* (headScreenDist *0.4f) + imaginaryBorundaryBoxDims.z() /2.0f
                );
                camera.center = QVector3D(
                        0 + imaginaryBorundaryBoxDims.x() /2.0f,
                        0 + imaginaryBorundaryBoxDims.y() /2.0f,
                        0 + imaginaryBorundaryBoxDims.z() /2.0f
                );
                camera.up = QVector3D(0, 1, 0);
                break;
        }
    };
*/
};
