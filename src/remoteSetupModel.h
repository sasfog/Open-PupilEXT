
#pragma once

#include <QtCore/QObject>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include "pupil-detection-methods/Pupil.h"

#include "subwindows/qtOpenGlViewer/qtOpenGlViewer.h"

#include <QSettings>
#include <QCoreApplication>

class RemoteSetupModel : public QObject {
Q_OBJECT
public:
    explicit RemoteSetupModel(QObject *parent = 0) : QObject(parent){};
    ~RemoteSetupModel() override {};

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //struct QEPoint {short x = 0; float y = 0;};
/*
signals:

    // When: 1, structure changes, e.g. the distance between the illuminator and camera OR 2, some component property (e.g. focal length) changes
    void staticGeometryChanged();
    // When e.g. the head position is changed
    void dynamicGeometryChanged();
*/
};

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

class Component : public QObject {
Q_OBJECT
public:
    explicit inline Component(QObject *parent = 0) : QObject(parent){};

    // Component *parentComponent;
    // char id;

    // dimensions of the model box
    float dimX; // mm
    float dimY; // mm
    float dimZ; // mm

    // in world coordinates
    float locX; // mm
    float locY; // mm
    float locZ; // mm
    float rotX; // rad
    float rotY; // rad
    float rotZ; // rad

    // if a component is disabled, it will not be taken into any calculations.
    // If it is a vital component, then the program will show that it cannot currently calculate geometries
    // until the user sets properties properly and enables the element again
    bool enabled = true;

    virtual bool drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) = 0;

    QVector<ComponentDataCitation> componentDataCitations;

    QVector<Component*> components;

    virtual void enable() {enabled = true;};
    virtual void disable() {enabled = false;};
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class cameraComponent : public Component {
Q_OBJECT
public:
    explicit cameraComponent(QObject *parent = 0) {};

    // Component *parentComponent;
    // char id;

    //
    bool drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCube(dimX); };

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

class lensComponent : public Component {
Q_OBJECT
public:
    explicit lensComponent(QObject *parent = 0) {};

    // Component *parentComponent;
    // char id;

    //
    bool drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCube(dimX); };

    //
    QString componentVendor;
    QString componentType;
    //QString componentProductFamily;
    //QString componentSerialNumber;
    QString suggestedSensorSizeRating; // E.g. 1/2"
    bool isVarifocal;
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

class sensorComponent : public Component {
Q_OBJECT
public:
    explicit sensorComponent(QObject *parent = 0) {};

    // Component *parentComponent;
    // char id;

    //
    bool drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { /*qtOpenGlViewer->createRectangle(dimX,dimY);*/ };

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
    float resolution; // MP
    QString format; // E.g. 1/2" (but can be calculated from sensorSizeX and sensorSizeY and sensorPixelSize)
    float effectiveDiagonal; // mm
    float pixelSize; // um
    float effectiveSizeX; // mm
    float effectiveSizeY; // mm
    float aspectRatio; // E.g. 3/4 (but can be calculated from sensorSizeX and sensorSizeY)

    //    float sensorDarkNoise; // E
    //QEPoint sensorSensitivityCurve[]; // QEPoint {short, float} tömb
    //    float sensorSensitivityCurve[70]; // 400 nm-1100 nm, one sample per 10 nm, = 70 elements
    // quantum efficiency for each wavelength, can be used to calculate
    // (knowing the expected reflectancy of the target, and the amount of
    // controlled+external light, to determine the necessary expo and
    // gain values)
};

class filterComponent : public Component {
Q_OBJECT
public:
    explicit filterComponent(QObject *parent = 0) {};

    // Component *parentComponent;
    // char id;

    //
    bool drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCube(dimX); };

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
    float transmissionCurve[70]; // 400 nm-1100 nm, one sample per 10 nm, = 70 elements // attenuation can be calculated
    // - attenuationBetween(lowEnd, highEnd)
    float filterDiameter; // mm
};

class illuminatorComponent : public Component {
Q_OBJECT
public:
    explicit illuminatorComponent(QObject *parent = 0) {};

    // Component *parentComponent;
    // char id;

    //
    bool drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCube(dimX); };

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
    float radiantFluxMin; // W // that can leave the component when warmed up, and used at the lowest "power" option (when driverIsVariable)
    float radiantFluxMax; // W // that can leave the component when warmed up, and used at the highest "power" option (when driverIsVariable)
    float radiantFluxActual; // W
    float relativeSpectralEmissionCurve[70]; // x=lambda,nm; y=Irel,% // 400 nm-1100 nm, one sample per 10 nm, = 70 elements
    //float relativeRadiantFluxCurve[ccc]; // x=IF,A; y=rel flux
    //float radiationCurve[36]; // x=phi,deg; y=Irel,% // 0-90 deg, one sample per 2.5 deg, = 36 elements

    bool driverIsConstantCurrent; // constantCurrent, constantVoltage
    //QString driverTechnology; // Discrete, Analog, PWM/SMPS, ...
    bool driverIsVariable;

    // - components
    //						// atomic is lehetne ide egyenként
    //						// driver lehetne ide
};

class screenComponent : public Component {
Q_OBJECT
public:
    explicit screenComponent(QObject *parent = 0) {};

    // Component *parentComponent;
    // char id;

    //
    bool drawInGLWidget(QtOpenGLViewer *qtOpenGlViewer) { qtOpenGlViewer->createCube(dimX); };

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
    float DPMM;
    float DPI;
    //float frameRate; // Hz
    //float tiltAngle // deg // if the screen is not normal to the floor plane. Negative values mean tilted upwards /towards the screen facing the ceiling
    //float rotationAngle // deg // 0 and 90 means landscape and portrait
};
