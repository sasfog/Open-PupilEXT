#pragma once

/**
    @authors Moritz Lode, Bényei Gábor, Attila Boncser
*/

#include <QtCore/QObject>
#include "camera.h"
#include "../frameRateCounter.h"
#include "stereoCameraImageEventHandler.h"
#include "../stereoCameraCalibration.h"
#include "../cameraFrameRateCounter.h"


#ifdef USE_PYLON

#include "cameraConfigurationEventHandler.h"
#include "hardwareTriggerConfiguration.h"

#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCameraArray.h>
#include <pylon/TlFactory.h>
//#include <pylon/PylonIncludes.h>
#include <pylon/gige/GigETransportLayer.h>

using namespace Pylon;
using namespace Basler_UniversalCameraParams;

/**
    StereoCamera represents a stereo camera (two Basler cameras), main camera and secondary camera

    The camera settings of the main camera are used for the secondary camera

    getFriendlyNames(): return the Basler device names for both cameras in a vector list
    attachCameras(): defines the two camera devices by device names
    open(): opens the two cameras and starts grabbing, IMPORTANT: cameras need to be opened before starting the hardware trigger to guarantee sync acquisition

    getCameraCalibration(): return a stereo calibration object for the two cameras

    loadMainFromFile(): load camera configuration for the main camera from file
    saveMainToFile(): save camera configuration for the main camera  to file

    CAUTION: all camera settings are set to both cameras, default values are taken from the main camera and applied to the secondary camera

signals:
    fps(double fps): frames per second of the file camera playback
    framecount(int framecount): current framecount of the file camera playback

*/
class StereoCamera : public Camera {
    Q_OBJECT

public:

    explicit StereoCamera(QObject *parent= 0);
    //explicit StereoCamera(const QString &friendlyNameMain, const QString &friendlyNameSecondary, QObject* parent=0);

    ~StereoCamera() override;

    bool isOpen() override;
    void close() override;
    CameraImageType getType() override;

    void startGrabbing() override;
    void stopGrabbing() override;

    std::vector<QString> getFriendlyNames();

    void autoGainOnce();
    void autoExposureOnce();

    int getExposureTimeValue();
    int getExposureTimeMin();
    int getExposureTimeMax();

    bool isEnabledAcquisitionFrameRate();
    bool isAcquisitionFrameRateAvailable();
    bool isEmulated();
    double getResultingFrameRateValue() override;

    // AFAIK it is always supported by Basler cameras.
    // Prepping is already done properly in the corresponding method performing this auto function.
    static bool isAutoGainAvailable() { return true; };
    static bool isAutoExposureAvailable() { return true; };

    bool isBinningAvailable();

    int getAcquisitionFPSValue();
    int getAcquisitionFPSMin();
    int getAcquisitionFPSMax();

    double getGainValue();
    double getGainMin();
    double getGainMax();

    void attachCameras(const QString &friendlyNameMain, const QString &friendlyNameSecondary);
    void open(bool enableHardwareTrigger);

    QString getLineSource();

    StereoCameraCalibration *getCameraCalibration();
    QString getCalibrationFilename();

    void loadMainFromFile(const QString &filename);
    //void loadSecondaryFromFile(const QString &filename); // removed this as stereo camera configuration is only set by main and secondary is adapted
    void saveMainToFile(const QString &filename);
    //void saveSecondaryToFile(const QString &filename);

    int getImageROIwidth() override; 
    int getImageROIheight() override; 
    int getImageROIoffsetX() override; 
    int getImageROIoffsetXInc() override;
    int getImageROIoffsetY() override;
    int getImageROIoffsetYInc() override;
    int getImageROIwidthMax() override; // both setImageROI and setImageResize depends on this
    int getImageROIwidthInc() override;
    int getImageROIheightMax() override; // both setImageROI and setImageResize depends on this
    int getImageROIheightInc() override;
    QRectF getImageROI() override;
    int getBinningVal();
    int getBinningMax();
    std::vector<double> getTemperatures();

    bool isTemperatureReadingSupported() override;

    bool isGrabbing() override;

private:

    // We only let Pylon type input from within the class. Calls from outside are only to use friendly names with serial number
    //  This is a preparatory step to later enable easier implementation of a general genicam camera wrapper
//    explicit StereoCamera(const CDeviceInfo &diMain, const CDeviceInfo &diSecondary, QObject* parent=0);
    void attachCameras(const CDeviceInfo &diMain, const CDeviceInfo &diSecondary);

    QDir settingsDirectory;

    uint64 cameraMainTime;
    uint64 cameraSecondaryTime;
    uint64 systemTime;

    QString lineSource;

    CBaslerUniversalInstantCameraArray cameras;
    StereoCameraImageEventHandler *cameraImageEventHandler = nullptr;
    CameraConfigurationEventHandler *cameraConfigurationEventHandler0 = nullptr;
    CameraConfigurationEventHandler *cameraConfigurationEventHandler1 = nullptr;
    HardwareTriggerConfiguration* hardwareTriggerConfiguration0 = nullptr;
    HardwareTriggerConfiguration* hardwareTriggerConfiguration1 = nullptr;
    CameraFrameRateCounter *frameCounter;

    StereoCameraCalibration *cameraCalibration;
    QThread *calibrationThread;

    void synchronizeTime();
    void loadCalibrationFile();
    void genericExceptionOccured(const GenericException &e);

    void enableSensorLevelBinningIfPossible();

    void safelyCloseCameras();

public slots:

    void setGainValue(double value);
    void setExposureTimeValue(int value);
    void setLineSource(QString value);
    void enableAcquisitionFrameRate(bool enabled);
    void setAcquisitionFPSValue(int value);
    void resynchronizeTime();

    bool setBinningVal(int value);
    bool setImageROIwidth(int width);
    bool setImageROIheight(int height);
    bool setImageROIoffsetX(int offsetX);
    bool setImageROIoffsetY(int offsetY);

    //bool setImageROIwidthEmu(int width);
    //bool setImageROIheightEmu(int height);
    //bool setImageROIoffsetXEmu(int offsetX);
    //bool setImageROIoffsetYEmu(int offsetY);

signals:

    void fps(double fps);
    void framecount(int framecount);
    void cameraDeviceRemoved();
    void imagesSkipped();

};

#else

// NOTE: has to happen, because aravis includes glib-2.0, and there
//  the definition "signals" is clashing with the Qt definition
#undef signals
#include <arv.h>
#define signals Q_SIGNALS

class StereoCamera : public Camera {
Q_OBJECT

public:

    explicit StereoCamera(QObject *parent= 0);
    //explicit StereoCamera(const QString &friendlyNameMain, const QString &friendlyNameSecondary, QObject* parent=0);

    ~StereoCamera() override;

    bool isOpen() override;
    void close() override;
    CameraImageType getType() override;

    void startGrabbing() override;
    void stopGrabbing() override;

    std::vector<QString> getFriendlyNames();
    // TODO: getfullnames?
    // TODO: getdeviceids?

    void autoGainOnce();
    void autoExposureOnce();

    int getExposureTimeValue();
    int getExposureTimeMin();
    int getExposureTimeMax();

    bool isEnabledAcquisitionFrameRate();
    bool isEmulated();
    double getResultingFrameRateValue() override;

    // TODO DEV !!!
    bool isAutoGainAvailable();
    bool isAutoExposureAvailable();

    bool isBinningAvailable();
    // TODO END

    int getAcquisitionFPSValue();
    int getAcquisitionFPSMin();
    int getAcquisitionFPSMax();

    double getGainValue();
    double getGainMin();
    double getGainMax();

    void attachCameras(const QString &friendlyNameMain, const QString &friendlyNameSecondary);
    void open(bool enableHardwareTrigger);

    QString getLineSource();

    StereoCameraCalibration *getCameraCalibration();
    QString getCalibrationFilename();

    void loadMainFromFile(const QString &filename);
    void saveMainToFile(const QString &filename);

    int getImageROIwidth() override;
    int getImageROIheight() override;
    int getImageROIoffsetX() override;
    int getImageROIoffsetXInc() override;
    int getImageROIoffsetY() override;
    int getImageROIoffsetYInc() override;
    int getImageROIwidthMax() override; // both setImageROI and setImageResize depends on this
    int getImageROIwidthInc() override;
    int getImageROIheightMax() override; // both setImageROI and setImageResize depends on this
    int getImageROIheightInc() override;
    QRectF getImageROI() override;
    int getBinningVal();
    int getBinningMax();
    std::vector<double> getTemperatures();

    bool isTemperatureReadingSupported() override;

    bool isGrabbing() override;

    // TODO
    void wrappedErrorOccured(GError *error);
    // TODO
    void manualResetDevice() {}; // needed for GigE devices, that can get stuck in an error state sometimes

private:

    void attachCameras(const ArvDevice &diMain, const ArvDevice &diSecondary);

    QDir settingsDirectory;

    uint64 cameraMainTime;
    uint64 cameraSecondaryTime;
    uint64 systemTime;

    QString lineSource;

    bool isGrabbingV = false;

    //CBaslerUniversalInstantCameraArray cameras;
    std::vector<ArvCamera*> cameras;
    StereoCameraImageEventHandler *cameraImageEventHandler = nullptr;
//    CameraConfigurationEventHandler *cameraConfigurationEventHandler0 = nullptr;
//    CameraConfigurationEventHandler *cameraConfigurationEventHandler1 = nullptr;
//    HardwareTriggerConfiguration* hardwareTriggerConfiguration0 = nullptr;
//    HardwareTriggerConfiguration* hardwareTriggerConfiguration1 = nullptr;
    ArvStreamCallbackData callbackData;

    void resizeStreamBuffer();

    CameraFrameRateCounter *frameCounter;
    StereoCameraCalibration *cameraCalibration;
    QThread *calibrationThread;

    void synchronizeTime();
    void loadCalibrationFile();

    void enableSensorLevelBinningIfPossible();

    void genericExceptionOccured(const std::exception &e, const GError &lastAravisError);
    void genericExceptionOccured(const std::exception &e, bool deviceRemoved = false);

    // TODO
//    void safelyCloseCameras();

public slots:

    void setGainValue(double value);
    void setExposureTimeValue(int value);
    void setLineSource(QString value);
    void enableAcquisitionFrameRate(bool enabled);
    void setAcquisitionFPSValue(int value);
    void resynchronizeTime();

    bool setBinningVal(int value);
    bool setImageROIwidth(int width);
    bool setImageROIheight(int height);
    bool setImageROIoffsetX(int offsetX);
    bool setImageROIoffsetY(int offsetY);

    //bool setImageROIwidthEmu(int width);
    //bool setImageROIheightEmu(int height);
    //bool setImageROIoffsetXEmu(int offsetX);
    //bool setImageROIoffsetYEmu(int offsetY);

signals:

    void fps(double fps);
    void framecount(int framecount);
    void cameraDeviceRemoved();
    void imagesSkipped();

    void deviceWasReset();
    void manualDeviceResetNecessary();

};

#endif
