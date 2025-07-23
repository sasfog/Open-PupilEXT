#pragma once

/**
    @author Gabor Benyei
*/

// has to happen, because aravis includes glib-2.0, and there the definition "signals" is clashing with the Qt definition
//#undef signals
//#define QT_NO_SIGNALS_SLOTS_KEYWORDS 1
#undef signals
#include <arv.h>
#include <arvbuffer.h>
#define signals Q_SIGNALS
//Q_SIGNALS

#include <QtCore/QObject>
#include <opencv2/core/mat.hpp>
#include <QDateTime>
#include "camera.h"

typedef struct {
	ArvStream *stream;
	int counter;
	gboolean done;
} ArvStreamCallbackData;

/**
    Image event handler, gets called at each new image received from a single camera (Aravis camera)
*/
class SingleAravisCameraImageEventHandler : public QObject {
Q_OBJECT

public:

    explicit SingleAravisCameraImageEventHandler(QObject* parent=0);

    ~SingleAravisCameraImageEventHandler() override;

    void setTimeSynchronization(uint64 cameraTime, uint64 systemTime);

    static void stream_callback (void *user_data, ArvStreamCallbackType type, ArvBuffer *buffer);

//    void OnImagesSkipped( CInstantCamera& camera, size_t countOfSkippedImages) override;
//    void OnImageGrabbed( CInstantCamera& camera, const CGrabResultPtr& ptrGrabResult) override;

private:

    uint64 cameraTime;
    uint64 systemTime;

//    CImageFormatConverter formatConverter;
//    CPylonImage pylonImage;

signals:
//Q_SIGNALS:

//    void onNewGrabResult(CameraImage grabResult);
//    void imagesSkipped();

};
