/* --------------------------------------------------------------------------------
 * A Qt-based 2D or 3D OpenGL viewer with mouse rotation, pan and zoom.
 *
 * Copyright 2018 Marcel Paz Goldschen-Ohm <marcel.goldschen@gmail.com>
 * -------------------------------------------------------------------------------- */

#ifndef __QtOpenGLViewer_H__
#define __QtOpenGLViewer_H__

#include <QColor>
#include <QFont>
#include <QObject>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>
#include <QVector3D>
#include <QOpenGLFunctions_4_5_Core>
//#include "../../remoteSetupModel.h"
#include "../QJSonModel/QJsonModel.hpp"

// DEV
#include <QOpenGLDebugLogger>

#ifdef DEBUG
#include <iostream>
#include <QDebug>
#endif

/* --------------------------------------------------------------------------------
 * Graph viewer UI.
 * -------------------------------------------------------------------------------- */


// TODO: ne is ez legyen maga a class amiben a user interaction alapjai definiálva vannak,
//      ÉS a primitív rajzolás, hanem legyen egy ebből leszármazott osztály, és az csinálja az utóbbiakat,
//      pl setSetupModel és egyebek is abba mehetnének
// TODO: az egérrel a görgővel kattintás ("pan" funkció) valahogy elrontja a modellt onnantól kezdve, darabosan,
//      pattogva fog mozogni, még akkor is ha csak forgatjuk később, úgy marad. Valami kerekítési oka lehet, de nem
//      találom hol. Amikor a z tengellyel szemben áll a kamera akkor minimális a pattoágs,
//      és minél jobban el van forogva, annál nagyobb, ehhez lehet köze.
//      DE érdekes módon a camera téglatestje nem csinálja ezt, csak minden más

enum OpenGLPrimitiveType {
    //POINT = 0,
    SPHEROID = 1,
    CYLINDER = 2,
    CONE = 3,
    CUBOID = 4 //,
};

class QtOpenGLViewer : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
    Q_PROPERTY(bool Is3D READ is3D WRITE setIs3D NOTIFY optionsChanged)
    Q_PROPERTY(float MouseWheelSensitivity READ mouseWheelSensitivity WRITE setMouseWheelSensitivity)
    Q_PROPERTY(bool SwapMouseWheelZoomDirection READ swapMouseWheelZoomDirection WRITE setSwapMouseWheelZoomDirection)
    Q_PROPERTY(QColor BackgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY optionsChanged)
    Q_PROPERTY(QFont HudFont READ hudFont WRITE setHudFont NOTIFY optionsChanged)

private:

    static void startTransformed(GLfloat dimX = 1.0, GLfloat dimY = 1.0, GLfloat dimZ = 1.0, GLfloat locX = 0.0, GLfloat locY = 0.0, GLfloat locZ = 0.0, GLfloat rotX = 0.0, GLfloat rotY = 0.0, GLfloat rotZ = 0.0);
    static void endTransformed(GLfloat dimX = 1.0, GLfloat dimY = 1.0, GLfloat dimZ = 1.0, GLfloat locX = 0.0, GLfloat locY = 0.0, GLfloat locZ = 0.0, GLfloat rotX = 0.0, GLfloat rotY = 0.0, GLfloat rotZ = 0.0);

public:

    struct GUIGeom {
        OpenGLPrimitiveType a;
        QJsonTreeItem *dimItem;
        QJsonTreeItem *locItem;
        QJsonTreeItem *rotItem;
        bool isHighlighted;
    };

    QVector<GUIGeom> _GUIGeoms;

    QtOpenGLViewer(QWidget *parent = NULL) : QOpenGLWidget(parent) {
        // TODO: in theory this is needed to let the qt gl debug logger initialize later.
        //      BUT if this code is executed, drawing is lost
        /*
        QSurfaceFormat format = this->format();
        format.setMajorVersion(4); // OpenGL version
        format.setMinorVersion(5);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setOption(QSurfaceFormat::DebugContext);
        setFormat(format);
        makeCurrent();
        */
    };
    virtual ~QtOpenGLViewer() {};

    /*
    enum PrimitiveType {
        CYLINDER, CONE, CUBE, // CUBOID,
    };

    struct Primitive {
        PrimitiveType primitiveType;
        std::vector<float> params;
    };

    class CreatePrimitive {
    public:
        static void createCylinder(float r = 5.0, float h = 2.0, float n = 25.0);
        static void createCone(float r = 5.0, float h = 2.0, float n = 25.0);
        static void createCube(GLfloat a = 1.0);
    };
     */

    void createCylinderAt(QVector3D dim, QVector3D loc, QVector3D rot, const QColor &color);
    void createCylinder(float r = 0.5, float h = 1.0, float n = 25.0, const QColor &color = QColor(255,255,0));
    void createCone(float r = 0.5, float h = 1.0, float n = 25.0, const QColor &color = QColor(255,255,0));
    void createConeAt(QVector3D dim, QVector3D loc, QVector3D rot, const QColor &color);
    void createCube(GLfloat a = 1.0, const QColor &color = QColor(255,255,0));
    void createCuboidAt(QVector3D dim, QVector3D loc, QVector3D rot, const QColor &color);

    void createSphere(float r = 0.5, int nParal = 10, int nMerid = 10, const QColor &color = QColor(255,255,0));
    void createSpheroidAt(QVector3D dim, QVector3D loc, QVector3D rot, const QColor &color);

    struct Camera {
        QVector3D eye = QVector3D(0, 0, 10);
        QVector3D center = QVector3D(0, 0, 0);
        QVector3D up = QVector3D(0, 1, 0);
        QVector3D view() { return center - eye; }
        void zoom(float viewDistance) { eye = center - view().normalized() * viewDistance; }
    } camera;
    
    bool is3D() const { return _is3D; }
    void setIs3D(bool b) { if(_is3D && !b) goToDefaultView(); _is3D = b; }
    
    float mouseWheelSensitivity() const { return _mouseWheelSensitivity; }
    void setMouseWheelSensitivity(float f) { _mouseWheelSensitivity = f > 1e-3 ? f : 1e-3; }
    
    bool swapMouseWheelZoomDirection() const { return _swapMouseWheelZoomDirection; }
    void setSwapMouseWheelZoomDirection(bool b) { _swapMouseWheelZoomDirection = b; }
    
    QColor backgroundColor() const { return _backgroundColor; }
    void setBackgroundColor(const QColor &color) { _backgroundColor = color; }
    
    QFont hudFont() const { return _hudFont; }
    void setHudFont(const QFont &font) { _hudFont = font; }
    
    // useful stuff
    static QVector3D screen2World(QVector3D screen, int *viewport, float *projection, float *modelview);
    static QVector3D world2Screen(QVector3D world, int *viewport, float *projection, float *modelview);
    static float intersectRayAndSphere(const QVector3D &rayOrigin, const QVector3D &rayDirection, const QVector3D &sphereCenter, float sphereRadius);
    static float intersectRayAndPlane(const QVector3D &rayOrigin, const QVector3D &rayDirection, const QVector3D &pointOnPlane, const QVector3D &planeNormal);
    void goToBillboard(const QVector3D &origin, const QVector3D &right);
    bool isLeftToRight(const QVector3D &vec);
    static float luminance(const QColor &color);
    static QColor colorWithMaxContrast(const QColor &color);
    void renderText(float x, float y, const QString &text, const QColor &color, const QFont &font);
    
    // drawing
    virtual void drawScene();
    virtual void drawHud(QPainter &painter);
    void drawAxes();
    
    // mouse selection
    virtual void selectObject(const QPointF &mousePosition);
    void getPickRay(const QPointF &mousePosition, QVector3D &origin, QVector3D &ray);
    QVector3D pickPointInPlane(const QPointF &mousePosition, const QVector3D &pointOnPlane, bool snapToUnitGrid = false);
    
signals:
    void optionsChanged();
    void selectedObjectChanged(QObject*);
    
public slots:
    virtual void goToDefaultView(int viewNumber = 1);
    virtual void deleteSelectedObject();
    virtual void editSelectedObject(const QPoint &mousePosition);

    /*void onSelectionChange(const QItemSelection &selected, const QItemSelection &deselected) {
        //selected.indexes()[0]


        for(int i = 0; i < _GUIGeoms.size(); i++) {
            if( selected.contains(QAbstractItemModel::createIndex( _GUIGeoms[i].dimItem->parent()->row(), 0,  _GUIGeoms[i].dimItem->parent())) ) {
                _GUIGeoms[i].isHighlighted = true;
            } else {
                _GUIGeoms[i].isHighlighted = false;
            }
        }
        drawScene();
    }*/

    void highlightGeom(QJsonTreeItem* p) {
        for(int i = 0; i < _GUIGeoms.size(); i++) {
            if(_GUIGeoms[i].dimItem->parent() == p) {
                _GUIGeoms[i].isHighlighted = true;
            } else {
                _GUIGeoms[i].isHighlighted = false;
            }
        }
        drawScene();
    }

    void addToScene(GUIGeom geom) {
        _GUIGeoms.push_back(geom);
        qDebug() << "added geom " << geom.a << "; " << geom.dimItem->value() << "; " << geom.locItem->value() << "; " << geom.rotItem->value();
    };
    
protected:
    void initializeGL() Q_DECL_OVERRIDE;
    void resizeGL(int w, int h) Q_DECL_OVERRIDE;
    void paintGL() Q_DECL_OVERRIDE;
    
    virtual void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    virtual void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    virtual void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    virtual void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    virtual void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

    /*
    void qt_save_gl_state();
    void qt_restore_gl_state();
    void renderText(double x, double y, const QString text);
     */
    inline GLdouble project(GLdouble objx, GLdouble objy, GLdouble objz,
                                         const GLdouble model[16], const GLdouble proj[16],
                                         const GLdouble viewport[4],
                                         GLdouble * winx, GLdouble * winy, GLdouble * winz);
    void renderText(GLdouble objx, GLdouble objy, GLdouble objz, QString text, QColor color = Qt::yellow);
    inline void transformPoint(GLdouble out[4], const GLdouble m[16], const GLdouble in[4]);
    
protected:
    bool _is3D = true;
    float _mouseWheelSensitivity = 0.2f;
    bool _swapMouseWheelZoomDirection = false;
    QColor _backgroundColor = QColor(200, 200, 200);
    QFont _hudFont = QFont("Sans", 10, QFont::Normal);
    QPointF _mousePosition;
    QObject *_selectedObject = NULL;

    QColor _geomColorBasic = QColor(255, 255, 0); // yellow
    //QColor _geomColorHighlighted = QColor(200, 7, 240); // purple
    QColor _geomColorHighlighted = QColor(0, 255, 0); // green

    // TODO:
    //  when rotating, draw pivot point with text: "Pivot", like in Inventor
    //  Shift+mouse should rotate
    //  click (RMB or LMB) should select the item in treeview
    //  + when anything in treeview is selected, all rows that belong to the same parent,
    //      should be painted slightly green. also should work in dark mode, and 3D selectio should be green


    // TODO: GB ASAP. 2025.07.09.

    /*
    // Get dim and loc
    std::tuple<QVector3D, QVector3D> dummy_getScreenParams() {
        if(!setupModel || !setupModel->isInitialized() || !setupModel->isValid() || setupModel->screenUnits.empty() || setupModel->screenUnits[0]->components.empty() )
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

    //QOpenGLFunctions *f;
//    QOpenGLFunctions_4_5_Core *f;
};

#endif
