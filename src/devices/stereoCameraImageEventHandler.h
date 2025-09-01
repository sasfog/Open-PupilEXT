#pragma once

/**
    @author Moritz Lode, Gabor Benyei, Attila Boncser
*/


#include <QtCore/QObject>
#include <opencv2/core/mat.hpp>
#include <QtCore/QMutex>
#include "camera.h"

#ifdef USE_PYLON

#include <pylon/PylonImage.h>
#include <pylon/ImageEventHandler.h>
#include <pylon/PylonIncludes.h>

using namespace Pylon;

/**
    Image event handler for stereo camera, gets called for EACH of the stereo camera images, meaning it will get called two times for a single stereo-image

    CAUTION:
    To sync the images, the camera provided framecount is used and it is assumed that the framecount of both cameras matches due to sync acquisition start (See StereoCamera)
    Its important for stereo synchronization, that only after opening of the stereo camera (acquisition start) the hardware triggers are started

    setTimeSynchronization(): set the camera and system times which are used to sync the camera timestamps, usually only a single time at camera initialisation

    onNewGrabResult(): signal send at each new stereo image, distributing the new stereo images of both cameras
*/
class StereoCameraImageEventHandler : public QObject, public CImageEventHandler {
Q_OBJECT

public:

    explicit StereoCameraImageEventHandler(QObject* parent=0);

    ~StereoCameraImageEventHandler() override;

    void setTimeSynchronization(uint64_t m_mainCameraTime, uint64_t m_secondaryCameraTime, uint64_t m_systemTime);

    void OnImagesSkipped( CInstantCamera& camera, size_t countOfSkippedImages) override;
    void OnImageGrabbed( CInstantCamera& camera, const CGrabResultPtr& ptrGrabResult) override;

private:

    QMutex mutex;

    uint64_t cameraTime[2] = {0, 0};
    uint64_t systemTime;

    CImageFormatConverter formatConverter;
    CPylonImage pylonImage;

    CameraImage stereoImage;

signals:

    void onNewGrabResult(CameraImage grabResult);
    void imagesSkipped();

};

#else

#include "./arvStreamCallbackData.h"

// NOTE: has to happen, because aravis includes glib-2.0, and there
//  the definition "signals" is clashing with the Qt definition
#undef signals
#include <arv.h>
#include <arvbuffer.h>
#define signals Q_SIGNALS

class StereoCameraImageEventHandler : public QObject {
Q_OBJECT

public:

    explicit StereoCameraImageEventHandler(QObject* parent=0);

    ~StereoCameraImageEventHandler() override;

    void setTimeSynchronization(uint64 m_mainCameraTime, uint64 m_secondaryCameraTime, uint64 m_systemTime);

    static void stream_callback(void *user_data, ArvStreamCallbackType type, ArvBuffer *buffer);

    // TODO: ASK TISA, ki lehet e szervezni ezt nemstatikus másik methodba ami már erre a példányra szól, és közben nincs semmi overheadje
    //void OnImageGrabbed(uint64 timestamp, ArvBuffer *buffer);

    void dropAllPending();

private:

    QMutex mutex;

    uint64 cameraTime[2] = {0, 0};
    uint64 systemTime;

    CameraImage stereoImage;

signals:

    void onNewGrabResult(CameraImage grabResult);
    void imagesSkipped();

};

#endif
