#pragma once

/**
    @author Moritz Lode, Gabor Benyei

    Note: Uses bits of code from Pylon samples, and Aravis samples (https://github.com/AravisProject/aravis-c-examples/)
*/

#include <QtCore/QObject>
#include <opencv2/core/mat.hpp>
#include <QDateTime>
#include "camera.h"

#ifdef USE_PYLON

#include <pylon/PylonImage.h>
#include <pylon/ImageEventHandler.h>
#include <pylon/PylonIncludes.h>

using namespace Pylon;

/**
    Image event handler, gets called at each new image received from a single camera (Basler camera)

    setTimeSynchronization(): set the camera and system times which are used to sync the camera timestamps, usually only a single time at camera initialisation

    onNewGrabResult(): signal send at each new image, distributing the images from the camera
*/
class SingleCameraImageEventHandler : public QObject, public CImageEventHandler {
Q_OBJECT

public:

    explicit SingleCameraImageEventHandler(QObject* parent=0);

    ~SingleCameraImageEventHandler() override;

    void setTimeSynchronization(uint64 cameraTime, uint64 systemTime);

    void OnImagesSkipped( CInstantCamera& camera, size_t countOfSkippedImages) override;
    void OnImageGrabbed( CInstantCamera& camera, const CGrabResultPtr& ptrGrabResult) override;

    void setTickFreq(quint64 _tickFreq);

private:

    uint64 cameraTime;
    uint64 systemTime;

    CImageFormatConverter formatConverter;
    CPylonImage pylonImage;

    // Needed for GigE
    bool needsTickConversion = false;
    uint64 tickFreq = 1;

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

/**
    Image event handler, gets called at each new image received from a single camera (Aravis camera)
*/
class SingleCameraImageEventHandler : public QObject {
Q_OBJECT

public:

    explicit SingleCameraImageEventHandler(QObject* parent=0);

    ~SingleCameraImageEventHandler() override;

    void setTimeSynchronization(uint64 cameraTime, uint64 systemTime);

    static void stream_callback(void *user_data, ArvStreamCallbackType type, ArvBuffer *buffer);

    // TODO: ASK TISA, ki lehet e szervezni ezt nemstatikus másik methodba ami már erre a példányra szól, és közben nincs semmi overheadje
    //void OnImageGrabbed(uint64 timestamp, ArvBuffer *buffer);

private:

    uint64 cameraTime;
    uint64 systemTime;

signals:

    void onNewGrabResult(CameraImage grabResult);
    void imagesSkipped();

};

#endif
