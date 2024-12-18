
#pragma once

#include <QtCore/QObject>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include "pupil-detection-methods/Pupil.h"

#include <QtMath>

//#include "subwindows/QtOpenGlViewer/QtOpenGlViewer.h"

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

class Component : public QObject {
Q_OBJECT
public:
    explicit inline Component(QObject *parent = 0) : QObject(parent) {};

    virtual SetupModelComponentType getType() = 0;

    // Component *parentComponent;
    // char id;

    // dimensions of the model box
    float dimX = 1.0f; // mm
    float dimY = 1.0f; // mm
    float dimZ = 1.0f; // mm

    // in world coordinates
    float locX = 0.0f; // mm
    float locY = 0.0f; // mm
    float locZ = 0.0f; // mm
    float rotX = 0.0f; // rad
    float rotY = 0.0f; // rad
    float rotZ = 0.0f; // rad

    // NOTE: if a component is disabled, it will not be taken into any calculations.
    // If it is a vital component, then the program will show that it cannot currently calculate geometries
    // until the user sets properties properly and enables the element again
    // TODO?: this could be a method, which upon call, checks if all needed component parameters are set
    bool enabled = true;

    //virtual void drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) = 0;

    QVector<ComponentDataCitation> componentDataCitations;

    QVector<Component*> components;

    virtual void enable() {enabled = true;};
    virtual void disable() {enabled = false;};
};

// A unit is made of components but it does not show in 3D space in the model view widget
// TODO: should there be different derived classes for illuminatorUnit, cameraUnit, etc. with own get methods that do calculations?
class Unit : public QObject {
Q_OBJECT
public:
    explicit inline Unit(QObject *parent = 0) : QObject(parent) {};

    // NOTE: if a component is disabled, it will not be taken into any calculations.
    // If it is a vital component, then the program will show that it cannot currently calculate geometries
    // until the user sets properties properly and enables the element again
    // TODO?: this could be a method, which upon call, checks if all needed component parameters are set
    bool enabled = true;

    //QVector<ComponentDataCitation> componentDataCitations;

    QVector<Component*> components;

    virtual void enable() {enabled = true;};
    virtual void disable() {enabled = false;};
};

// possible derived classes: CameraUnit, IlluminatorUnit, ScreenUnit, Head

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CameraComponent : public Component {
Q_OBJECT
public:
    explicit CameraComponent(QObject *parent = 0) {};

    SetupModelComponentType getType() override { return CAMERA; };

    // Component *parentComponent;
    // char id;

    //
    //void drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCuboidAt(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ); };

    //- cameraIdentity // string // Main or Secondary
    QString componentVendor;
    QString componentType;
    QString componentProductFamily;
    QString componentSerialNumber;
    float flangeDistance; // i.e. sensor to lens mount distance. Actually this is a feature of mount type, e.g. C-mount is 17.526 mm, BUT s-mount has no fixed flange distance
    QString lensMountType; // c-mount / cs-mount / s-mount (= M12)
    QString dataInterfaceType; // GigE / USB3 / CoaXPress
    float operatingCurrent; // A // This is a typical value. Maximum can be 10-15% larger
    float operatingVoltage; // V
    QVector<CameraConnectorPin> cameraConnectorPins;
    QString cameraConnectorType; // e.g. M8 6-PIN female, A-coded, IEC 61076-2-104
    float defaultFramerate;

    // - components
    //						// sensor could be a component...
    //						// mount could be a component
};

class LensComponent : public Component {
Q_OBJECT
public:
    explicit LensComponent(QObject *parent = 0) {};

    SetupModelComponentType getType() override { return LENS; };

    // Component *parentComponent;
    // char id;

    //
    //void drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCylinderAt(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ); };

    //
    QString componentVendor;
    QString componentType;
    //QString componentProductFamily;
    //QString componentSerialNumber;
    QString suggestedSensorSizeRating; // E.g. 1/2"
    bool isVarifocal = false;
    float focalDistanceMin; // mm
    float focalDistanceMax; // mm
    float focalDistanceActual; // mm // --------------------------------
    float fValueMin;
    float fValueMax;
    float fValueActual; // ---------------------------------
    float sholuderToFirstSurfaceDistance; // mm
    float outerDiameter; // mm // when there are no adjustment screws attached
    float frontFilterDiameter; // mm
};

class SensorComponent : public Component {
Q_OBJECT
public:
    explicit SensorComponent(QObject *parent = 0) {};

    SetupModelComponentType getType() override { return SENSOR; };

    // Component *parentComponent;
    // char id;

    //
    //void drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCuboidAt(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ); };

    //
    QString componentVendor;
    QString componentType;
    //QString componentProductFamily;
    //QString componentSerialNumber;

    QString shutterType; // Rolling or Global (but it should be Global anyway)
    bool isMonochrome;
    QString sensorTechnology;
    unsigned short resolutionX; // px
    unsigned short resolutionY; // px
    float resolution() { return resolutionX*resolutionY/1000000.0f; }; // MP
    QString format; // E.g. 1/2" (but can be calculated from sensorSizeX and sensorSizeY and sensorPixelSize) // TODO: compute on demand
    float effectiveDiagonal() { return qSqrt(qPow(effectiveSizeX(),2)+qPow(effectiveSizeY(),2)); }; // mm
    float pixelSize; // um
    float effectiveSizeX() { return resolutionX*pixelSize/1000.0f; }; // mm
    float effectiveSizeY() { return resolutionY*pixelSize/1000.0f; }; // mm
    float aspectRatio() { return resolutionX/resolutionY; }; // E.g. 3/4 (but can be calculated from sensorSizeX and sensorSizeY)

    //    float sensorDarkNoise; // E
    //QEPoint sensorSensitivityCurve[]; // QEPoint {short, float} tömb
    //    float sensorSensitivityCurve[70]; // 400 nm-1100 nm, one sample per 10 nm, = 70 elements
    // quantum efficiency for each wavelength, can be used to calculate
    // (knowing the expected reflectancy of the target, and the amount of
    // controlled+external light, to determine the necessary expo and
    // gain values)
};

class FilterComponent : public Component {
Q_OBJECT
public:
    explicit FilterComponent(QObject *parent = 0) {};

    SetupModelComponentType getType() override { return FILTER; };

    // Component *parentComponent;
    // char id;

    //
    //void drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCylinderAt(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ); };

    //
    QString componentVendor;
    QString componentType;
    //QString componentProductFamily;
    //QString componentSerialNumber;
    QString mountingType; // screw-on, screw-in, embedded
    QString opticalBehaviour; // lowpass, highpass, bandpass
    QString principle; // absorption / interference
    float lowpassCuton; // nm // optional
    float highpassCutoff; // nm // optional
//    float transmissionCurve[70]; // 400 nm-1100 nm, one sample per 10 nm, = 70 elements // attenuation can be calculated
    // - attenuationBetween(lowEnd, highEnd)
    float filterDiameter; // mm
    // + thickness?
};

class IlluminatorComponent : public Component {
Q_OBJECT
public:
    explicit IlluminatorComponent(QObject *parent = 0) {};

    SetupModelComponentType getType() override { return ILLUMINATOR; };

    // Component *parentComponent;
    // char id;

    //
    //void drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCuboidAt(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ); };

    //
    QString atomicComponentVendor;
    QString atomicComponentType;
    QString componentVendor;
    QString componentType;
    //QString componentProductFamily;
    //QString componentSerialNumber;
    char numAtomicComponents; // how many LEDs it consists of
    // in theory we could calculate total current, etc using the propertied of an atomic illuminator
    // element (one LED), but it is not really good to rely on that, because it is not sure if they
    // are all in series, or on parallel blocks
    float operatingCurrentMin; // A
    float operatingCurrentMax; // A
    float operatingCurrentActual; // A // ------------------------------
    float operatingVoltageMin; // V
    float operatingVoltageMax; // V
    float operatingVoltageActual; // C // ------------------------------
    float operatingTemperatureMin; // C
    float operatingTemperatureMax; // C
    float operatingTemperatureActual; // C // ------------------------------
    char radiationAngle; // of the cone in which most light is emitted
    unsigned short emissionCentroid; // centroid wavelength, nm
//    float radiantFluxMin; // W // that can leave the component when warmed up, and used at the lowest "power" option (when driverIsVariable)
//    float radiantFluxMax; // W // that can leave the component when warmed up, and used at the highest "power" option (when driverIsVariable)
//    float radiantFluxActual; // W
//    float relativeSpectralEmissionCurve[70]; // x=lambda,nm; y=Irel,% // 400 nm-1100 nm, one sample per 10 nm, = 70 elements
    //float relativeRadiantFluxCurve[ccc]; // x=IF,A; y=rel flux
    //float radiationCurve[36]; // x=phi,deg; y=Irel,% // 0-90 deg, one sample per 2.5 deg, = 36 elements

    bool driverIsConstantCurrent; // constantCurrent, constantVoltage
    //QString driverTechnology; // Discrete, Analog, PWM/SMPS, ...
    bool driverIsVariable;

    // - components
    //						// atomic is lehetne ide egyenként
    //						// driver lehetne ide
};

class ScreenComponent : public Component {
Q_OBJECT
public:
    explicit ScreenComponent(QObject *parent = 0) {};

    SetupModelComponentType getType() override { return SCREEN; };

    // Component *parentComponent;
    // char id;

    //
    //void drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCuboidAt(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ); };

    //
    QString componentVendor;
    QString componentType;
    //QString componentProductFamily;
    //float curvatureX; // mm // radius, if the screen is bent in the X axis
    //float curvatureY; // mm // radius, if the screen is bent in the X axis
    unsigned int resolutionX; // px
    unsigned int resolutionY; // px
    float physicalSizeX; // mm
    float physicalSizeY; // mm

    // 1 DPMM = 25.4 DPI
    // 1 DPI = 1 PPI (?)

    // given that X and Y are using equally sized pixel sizes
    float DPMM() { return resolutionX/physicalSizeX; };
    float DPI() { return DPMM()/25.4f; };
    float inchSize() { return qSqrt(physicalSizeX*physicalSizeX + physicalSizeY*physicalSizeY)/25.4f; };
    //float frameRate; // Hz
    //float tiltAngle // deg // if the screen is not normal to the floor plane. Negative values mean tilted upwards /towards the screen facing the ceiling
    //float rotationAngle // deg // 0 and 90 means landscape and portrait
};

class CvTargetComponent : public Component {
Q_OBJECT
public:
    explicit CvTargetComponent(QObject *parent = 0) {};

    SetupModelComponentType getType() override { return CVTARGET; };

    // Component *parentComponent;
    // char id;

    //
    //void drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCylinderAt(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ); };

    //
    // TODO
    float outerRingDiameter; // mm
};

class EyeballComponent : public Component {
Q_OBJECT
public:
    explicit EyeballComponent(QObject *parent = 0) {};

    SetupModelComponentType getType() override { return EYEBALL; };

    // Component *parentComponent;
    // char id;

    //
    //void drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createSpheroidAt(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ); };

    //
    float eyeballDiameter;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO:    - REMOVE dimX, dimY, dimZ, and add a getBoundingBox() virtual method instead
//          - access each components special dimensions through getters or properties, e.g. lens diameter, not using dimX, dimY...
//          - use 3D data type instead of 3 separate variables

class RemoteSetupModel : public QObject {
Q_OBJECT
public:
    explicit RemoteSetupModel(QObject *parent = 0) : QObject(parent){};
    ~RemoteSetupModel() override {};

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    QVector<Unit*> cameraUnits;
    QVector<Unit*> illuminatorUnits;
    QVector<Unit*> screenUnits;
    QVector<Unit*> heads;

    void resetModel() {
        // TODO:
        cameraUnits.clear();
        illuminatorUnits.clear();
        screenUnits.clear();
        heads.clear();
    };

    bool isInitialized() {
        return (!cameraUnits.isEmpty() && !illuminatorUnits.isEmpty() && !screenUnits.isEmpty() && !heads.isEmpty());
    };

    bool isValid() {
        // TODO: iteratively check if the model is "valid" so no impossible values exist, and can be used to start gaze tracking with
        return true;
    };

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

        return hypot(hypot(eyes[0]->locX-eyes[1]->locX,eyes[0]->locY-eyes[1]->locY),eyes[0]->locZ-eyes[1]->locZ);
    };

    //struct QEPoint {short x = 0; float y = 0;};
/*
signals:

    // When: 1, structure changes, e.g. the distance between the illuminator and camera OR 2, some component property (e.g. focal length) changes
    void staticGeometryChanged();
    // When e.g. the head position is changed
    void dynamicGeometryChanged();
*/
};
