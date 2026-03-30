#pragma once

/**
    @authors Moritz Lode, Gabor Benyei, Attila Boncser
*/

#include <QtCore/QObject>
#include "singleCameraImageEventHandler.h"
#include "camera.h"
#include "../frameRateCounter.h"
#include "../cameraCalibration.h"
#include "../cameraFrameRateCounter.h"

#ifdef USE_PYLON

#include "cameraConfigurationEventHandler.h"
#include "hardwareTriggerConfiguration.h"

#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/TlFactory.h>
//#include <pylon/PylonIncludes.h>
#include <pylon/gige/GigETransportLayer.h>

using namespace Pylon;
using namespace Basler_UniversalCameraParams;

/**
    SingleCamera represents a single camera (Basler camera)

    getFriendlyName(): return the Basler device name in a friendly formatting
    getFullName(): return the full Basler device name
    getCameraCalibration(): return the camera's calibration object

    loadFromFile(): load camera configuration from file
    saveToFile(): save camera configuration to file

    Camera configuration is set through various member functions

signals:
    fps(double fps): frames per second of the file camera playback
    framecount(int framecount): current framecount of the file camera playback

*/
class SingleCamera : public Camera {
Q_OBJECT

public:

    explicit SingleCamera(const QString &friendlyName, QObject* parent=0);

    ~SingleCamera() override;

    QSize fullSensorResolution;
    QString getFriendlyName();
    QString getFullName();
    QString getDeviceID();

    bool isOpen() override;
    void close() override;
    CameraImageType getType() override;

    void startGrabbing() override;
    void stopGrabbing() override;

    void autoGainOnce();
    void autoExposureOnce();

    int getExposureTimeValue();
    int getExposureTimeMin();
    int getExposureTimeMax();

    bool isEnabledAcquisitionFrameRate(); // ResultingFrameRate
    bool isAcquisitionFrameRateAvailableForSWT();
    bool isAcquisitionFrameRateAvailableForHWT();
    bool isEmulated();
    double getResultingFrameRateValue() override; // ResultingFrameRate

    bool isAutoGainAvailable();
    bool isAutoExposureAvailable();

    bool isBinningAvailable();

    int getAcquisitionFPSValue();
    int getAcquisitionFPSMin();
    int getAcquisitionFPSMax();

    double getGainValue();
    double getGainMin();
    double getGainMax();

    QString getLineSource();
    bool isHardwareTriggerAvailable();
    bool isHardwareTriggerEnabled();

    CameraCalibration* getCameraCalibration();
    QString getCalibrationFilename();

    void loadFromFile(const QString &filename);
    void saveToFile(const QString &filename);

    int getImageROIwidth() override;
    int getImageROIheight() override;
    int getImageROIoffsetX() override;
    int getImageROIoffsetXInc() override;
    int getImageROIoffsetY() override;
    int getImageROIoffsetYInc() override;
    QRectF getImageROI() override;
    int getImageROIwidthMax() override; // both setImageROI and setImageResize depends on this
    int getImageROIwidthInc() override;
    int getImageROIheightMax() override; // both setImageROI and setImageResize depends on this
    int getImageROIheightInc() override;
    int getBinningVal();
    int getBinningMax();
    double getTemperature();
    bool isGrabbing() override;

    QSize getFullSensorResolution() override;
    int checkExposureTimeIfCompletedAuto();
    double checkGainIfCompletedAuto();

    bool isTemperatureReadingSupported() override;

private:

    QDir settingsDirectory;
    QSettings *applicationSettings;

    uint64 cameraTime;
    uint64 systemTime;

    bool hardwareTriggerEnabled;
    QString lineSource;

    void determineFullSensorResolution();
    CBaslerUniversalInstantCamera camera;
    SingleCameraImageEventHandler *cameraImageEventHandler;

    CameraConfigurationEventHandler *cameraConfigurationEventHandler = nullptr;
    HardwareTriggerConfiguration *hardwareTriggerConfiguration = nullptr;
    CAcquireContinuousConfiguration *softwareTriggerConfiguration = nullptr;
    CameraFrameRateCounter *frameCounter;
    CameraCalibration *cameraCalibration;
    QThread *calibrationThread;

    void synchronizeTime();
    void loadCalibrationFile();
    void genericExceptionOccured(const GenericException &e);
    void stdExceptionOccured(const std::exception &e);

    void enableSensorLevelBinningIfPossible();

public slots:

    void setGainValue(double value);
    void setExposureTimeValue(int value);
    void setLineSource(QString value);
    void enableAcquisitionFrameRate(bool enabled);
    void setAcquisitionFPSValue(int value);
    void enableHardwareTrigger(bool state);

    bool setBinningVal(int value);
    bool setImageROIwidth(int width);
    bool setImageROIheight(int height);
    bool setImageROIoffsetX(int offsetX);
    bool setImageROIoffsetY(int offsetY);

signals:

    void fps(double fps);
    void framecount(int framecount);
    void cameraDeviceRemoved();
    void imagesSkipped();

    // TODO: implement to Pylon too
    void deviceWasReset();
    void manualDeviceResetNecessary();

};

#else

// NOTE: has to happen, because aravis includes glib-2.0, and there
//  the definition "signals" is clashing with the Qt definition
#undef signals
#include <arv.h>
#define signals Q_SIGNALS

#include <QtCore/QObject>
#include "singleCameraImageEventHandler.h"
#include "camera.h"
#include "../frameRateCounter.h"
#include "../cameraCalibration.h"
#include "../cameraFrameRateCounter.h"
#include <QDir>

// TODO:
//  IMPORTANT: There is a very weird bug that can arise here. Including and using hardwareTriggerConfiguration, will
//  cause a linker error down the line. Linker will wail to find "GCBase_MDd_VC141_v3_1_Basler_pylon_v3.lib"
//  to link against. It is not surprising because that library does not exist, as pylon only comes in
//  release version for linking. But this desperate trying of the linker to find the debug version
//  will arise when the said includes are included. The reason might be that some definition is not found
//  in the release version, and the linker will automatically assume to continue with looking for the debug
//  version. It even happens in the add_executable call of cmake, however the error itself only materializes
//  during the target_link_libraries call later.
//   Experienced on windows 64 build system, for windows 64 target system,
//   using MSVC 2019, on CLion with the vcpkg toolchain file configured.

// TODO: hardware trigger configuration, to accept input on line source chosen.
//  Similar to Basler hw.trigger conf. (see header)

/**
    SingleCamera represents a single camera (Aravis camera)
*/
class SingleCamera : public Camera {
Q_OBJECT

public:

    explicit SingleCamera(const QString &friendlyName, QObject* parent=0);

    ~SingleCamera() override;

    QSize fullSensorResolution;
    QString getFriendlyName();
    QString getFullName();
    QString getDeviceID();

    // TODO: DEV EMPTY METHODS
    bool isOpen() override;
    void close() override;
    CameraImageType getType() override;

    void startGrabbing() override;
    void stopGrabbing() override;


    void autoGainOnce();
    void autoExposureOnce();

    int getExposureTimeValue();
    int getExposureTimeMin();
    int getExposureTimeMax();

    bool isEnabledAcquisitionFrameRate(); // ResultingFrameRate
    bool isAcquisitionFrameRateAvailableForSWT();
    bool isAcquisitionFrameRateAvailableForHWT();
    bool isEmulated();
    double getResultingFrameRateValue() override; // ResultingFrameRate

    bool isAutoGainAvailable();
    bool isAutoExposureAvailable();

    bool isBinningAvailable();

    int getAcquisitionFPSValue();
    int getAcquisitionFPSMin();
    int getAcquisitionFPSMax();

    double getGainValue();
    double getGainMin();
    double getGainMax();

    QString getLineSource();
    bool isHardwareTriggerAvailable();
    bool isHardwareTriggerEnabled();

    CameraCalibration* getCameraCalibration();
    QString getCalibrationFilename();

    void loadFromFile(const QString &filename);
    void saveToFile(const QString &filename);

    int getImageROIwidth() override;
    int getImageROIheight() override;
    int getImageROIoffsetX() override;
    int getImageROIoffsetXInc() override;
    int getImageROIoffsetY() override;
    int getImageROIoffsetYInc() override;
    QRectF getImageROI() override;
    int getImageROIwidthMax() override; // both setImageROI and setImageResize depends on this
    int getImageROIwidthInc() override;
    int getImageROIheightMax() override; // both setImageROI and setImageResize depends on this
    int getImageROIheightInc() override;
    int getBinningVal();
    int getBinningMax();
    double getTemperature();

    QSize getFullSensorResolution() override;
    int checkExposureTimeIfCompletedAuto();
    double checkGainIfCompletedAuto();

    bool isTemperatureReadingSupported() override;

    bool isGrabbing() override;

    void wrappedErrorOccured(GError *error);
    // TODO
    void manualResetDevice() {}; // needed for GigE devices, that can get stuck in an error state sometimes


private:

    QDir settingsDirectory;
    QSettings *applicationSettings;
    int streamBufferSize = 100;

    uint64 cameraTime;
    uint64 systemTime;

    bool hardwareTriggerEnabled;
    QString lineSource;

    bool isGrabbingV = false;

    void determineFullSensorResolution();
    ArvCamera *camera;
    ArvStream *stream;
    SingleCameraImageEventHandler *cameraImageEventHandler;
    ArvStreamCallbackData callbackData;

    int lastUsedBinningVal = 1;

    void resizeStreamBuffer();

    CameraFrameRateCounter *frameCounter;
    CameraCalibration *cameraCalibration;
    QThread *calibrationThread;

    void synchronizeTime();
    void loadCalibrationFile();

    void enableSensorLevelBinningIfPossible();

    void genericExceptionOccured(const std::exception &e, const GError &lastAravisError);
    void genericExceptionOccured(const std::exception &e, bool deviceRemoved = false);


public slots:


    void setGainValue(double value);
    void setExposureTimeValue(int value);
    void setLineSource(QString value);
    void enableAcquisitionFrameRate(bool enabled);
    void setAcquisitionFPSValue(int value);
    void enableHardwareTrigger(bool state);

    bool setBinningVal(int value);
    bool setImageROIwidth(int width);
    bool setImageROIheight(int height);
    bool setImageROIoffsetX(int offsetX);
    bool setImageROIoffsetY(int offsetY);


signals:

    void fps(double fps);
    void framecount(int framecount);
    void cameraDeviceRemoved();
    void imagesSkipped();

    void deviceWasReset();
    void manualDeviceResetNecessary();

};

#endif


