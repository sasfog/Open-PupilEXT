#pragma once

/**
    @authors Gabor Benyei
*/

// has to happen, because aravis includes glib-2.0, and there the definition "signals" is clashing with the Qt definition
//#undef signals
#undef signals
//#define QT_NO_SIGNALS_SLOTS_KEYWORDS 1
#include <arv.h>
#define signals Q_SIGNALS
//Q_SIGNALS

#include <QtCore/QObject>
#include "singleAravisCameraImageEventHandler.h"
//#include "cameraConfigurationEventHandler.h"
#include "camera.h"
#include "../frameRateCounter.h"
#include "../cameraCalibration.h"
#include "../cameraFrameRateCounter.h"
//#include "hardwareTriggerConfiguration.h"
#include <QDir>

// TODO:
//  IMPORTANT: There is a very weird bug that can arise here. Including and using cameraCalibration
//  and frameRateCounter and cameraFrameRateCounter and hardwareTriggerConfiguration, will somehow
//  cause a linker error down the line. Linker will wail to find "GCBase_MDd_VC141_v3_1_Basler_pylon_v3.lib"
//  to link against. It is not surprising because that library does not exist, as pylon only comes in
//  release version for linking. But this desperate trying of the linker to find the debug version
//  will arise when the said includes are included. The reason might be that some definition is not found
//  in the release version, and the linker will automatically assume to continue with looking for the debug
//  version. It even happens in the add_executable call of cmake, however the error itself only materializes
//  during the target_link_libraries call later.
//   Experienced on windows 64 build system, or windows 64 target system,
//   using MSVC 2019, on CLion with the vcpkg toolchain file configured.

/**
    SingleCamera represents a single camera (Aravis camera)

*/
class SingleAravisCamera : public Camera {
Q_OBJECT

public:

    explicit SingleAravisCamera(const QString &friendlyName, QObject* parent=0);

    ~SingleAravisCamera() override;

    /*
    QString getFriendlyName();
    QString getFullName();
    QString getDeviceID();
     */

    // DEV EMPTY METHODS
    bool isOpen() override {return true;};
    void close() override {};
    CameraImageType getType() override {return CameraImageType::LIVE_SINGLE_CAMERA;};

    void startGrabbing() override {};
    void stopGrabbing() override {};

    /*
    void autoGainOnce();
    void autoExposureOnce();

    int getExposureTimeValue();
    int getExposureTimeMin();
    int getExposureTimeMax();

    bool isEnabledAcquisitionFrameRate(); // ResultingFrameRate
    bool isEmulated();
    double getResultingFrameRateValue(); // ResultingFrameRate

    int getAcquisitionFPSValue();
    int getAcquisitionFPSMin();
    int getAcquisitionFPSMax();

    double getGainValue();
    double getGainMin();
    double getGainMax();

    std::string getLineSource();
    bool isHardwareTriggerEnabled();
*/
    CameraCalibration* getCameraCalibration();
    /*
    QString getCalibrationFilename();

    void loadFromFile(const std::string &filename);
    void saveToFile(const std::string &filename);

    int getImageROIwidth() override; 
    int getImageROIheight() override; 
    int getImageROIoffsetX() override; 
    int getImageROIoffsetY() override; 
    QRectF getImageROI() override;
    int getImageROIwidthMax() override; // both setImageROI and setImageResize depends on this
    int getImageROIheightMax() override; // both setImageROI and setImageResize depends on this
    int getBinningVal();
    double getTemperature();
     */
    bool isGrabbing() override {return true;}; //DEV

    void getTEST();

private:

    QDir settingsDirectory;

    uint64 cameraTime;
    uint64 systemTime;

    bool hardwareTriggerEnabled;
    std::string lineSource;

    ArvCamera *camera;
    SingleAravisCameraImageEventHandler *cameraImageEventHandler;

    /*
    CameraConfigurationEventHandler *cameraConfigurationEventHandler = nullptr;
    HardwareTriggerConfiguration *hardwareTriggerConfiguration = nullptr;
    CAcquireContinuousConfiguration *softwareTriggerConfiguration = nullptr;
     */
    CameraFrameRateCounter *frameCounter;
    CameraCalibration *cameraCalibration;
    QThread *calibrationThread;

    // DEV EMPTY METHODS
    void synchronizeTime() {};
    void loadCalibrationFile() {};
    /*
    void genericExceptionOccured(const GenericException &e);
     */

public slots:

    /*
    void setGainValue(double value);
    void setExposureTimeValue(int value);
    void setLineSource(std::string value);
    void enableAcquisitionFrameRate(bool enabled);
    void setAcquisitionFPSValue(int value);
    void enableHardwareTrigger(bool state);

    bool setBinningVal(int value);
    bool setImageROIwidth(int width);
    bool setImageROIheight(int height);
    bool setImageROIoffsetX(int offsetX);
    bool setImageROIoffsetY(int offsetY);
     */

signals:
//Q_SIGNALS:

    void fps(double fps);
    void framecount(int framecount);
    void cameraDeviceRemoved();
    void imagesSkipped();

};
