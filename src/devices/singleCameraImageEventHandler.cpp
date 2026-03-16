
#include "singleCameraImageEventHandler.h"

#include <QDebug>

#ifdef USE_PYLON

SingleCameraImageEventHandler::SingleCameraImageEventHandler(QObject* parent) : QObject(parent), cameraTime(0), systemTime(0) {
    // Set the image mode to 8 bit grayscale
    formatConverter.OutputPixelFormat = Pylon::PixelType_Mono8;
}

SingleCameraImageEventHandler::~SingleCameraImageEventHandler() {

}

// Event handler when images are skipped by the camera
// BG: NOTE: According to Basler docs this will only ever get called when grabStrategy is set to LatestImageOnly or LatestImages, which is never the case for us ..(?)
// https://zh.docs.baslerweb.com/pylonapi/cpp/class_pylon_1_1_c_basler_universal_image_event_handler#function-onimagesskipped
void SingleCameraImageEventHandler::OnImagesSkipped(CInstantCamera& camera, size_t countOfSkippedImages) {
    std::cout << "OnImagesSkipped event for device " << camera.GetDeviceInfo().GetModelName() << std::endl;
    std::cout << countOfSkippedImages  << " images have been skipped." << std::endl;
    std::cout << std::endl;

    emit imagesSkipped();
}

// Event handler when a new image was grabbed from the camera
// Receives the grab result pointer which contains the grabbed image and meta information
// After a successful image grab, the image timestamp is converted to system time using the previously acquired starting timestamps
// The image is converted to a 8 bit grayscale cv::Mat format and bundled with meta information in a CameraImage object
// Emits signal onNewGrabResult containing the newly grabbed CameraImage
void SingleCameraImageEventHandler::OnImageGrabbed(CInstantCamera& camera, const CGrabResultPtr& ptrGrabResult) {
    //std::cout << "OnImageGrabbed event for device " << camera.GetDeviceInfo().GetModelName() << std::endl;

    // Image grabbed successfully?
    if (ptrGrabResult->GrabSucceeded()) {
        //std::cout << "SizeX: " << ptrGrabResult->GetWidth() << std::endl;
        //std::cout << "SizeY: " << ptrGrabResult->GetHeight() << std::endl;

        //// DEV DEE DEMO LUND
        //if (dummyFrameLimiterFrameCount < 2) {
        //    dummyFrameLimiterFrameCount++;
        //    return;
        //} else {
        //    dummyFrameLimiterFrameCount = 0;
        //}

        uint64 timeStamp = ptrGrabResult->GetTimeStamp();
        // cameraTime describes the acquisition start in camera time, systemTime the acquisition start in system time
        timeStamp = ((timeStamp-cameraTime) / 1000000) + systemTime;

        //quint64 chrono_time  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        //qDebug() <<
        //    QString("chrono time = ") << QString::number(chrono_time) <<
        //    QString("; acq timestamp = ") << QString::number(timeStamp) <<
        //    QString("; diff = ") << QString::number((int64)chrono_time-(int64)timeStamp) ; // túlcsordul ha másikból vonunk ki.. nagyobból kell

        formatConverter.Convert(pylonImage, ptrGrabResult);
        cv::Mat img = cv::Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC1, (uint8_t *) pylonImage.GetBuffer());

        CameraImage result;
        result.type = CameraImageType::LIVE_SINGLE_CAMERA;
        result.img = img.clone();
        result.timestamp = timeStamp;

        emit onNewGrabResult(result);
    } else {
        std::cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << std::endl;

        std::cout << " second try " << ptrGrabResult->GetErrorDescription() << std::endl;

        // If there is faulty connection /hw. interface problem
        if(ptrGrabResult->GetErrorCode() == 3791651083 || // Error code for "The image stream is out of sync."
            ptrGrabResult->GetErrorCode() == 31 || // Error code for device not functioning
            QString::fromStdString(ptrGrabResult->GetErrorDescription().c_str()).contains("sync")) {
            emit imagesSkipped();
        }
    }
}

// For conversion of the camera timestamp to system time, a tuple of timestamps from the start of the image acquisition is used
void SingleCameraImageEventHandler::setTimeSynchronization(uint64 m_cameraTime, uint64 m_systemTime) {

    // cameraTime describes the acquisition start in camera time, systemTime the acquisition start in system time
    cameraTime = m_cameraTime;
    systemTime = m_systemTime;
}

#else

SingleCameraImageEventHandler::SingleCameraImageEventHandler(QObject* parent) : QObject(parent), cameraTime(0), systemTime(0) {
    // Set the image mode to 8 bit grayscale
//    formatConverter.OutputPixelFormat = Pylon::PixelType_Mono8;
}

SingleCameraImageEventHandler::~SingleCameraImageEventHandler() {

}

// For conversion of the camera timestamp to system time, a tuple of timestamps from the start of the image acquisition is used
void SingleCameraImageEventHandler::setTimeSynchronization(uint64 m_cameraTime, uint64 m_systemTime) {

    // cameraTime describes the acquisition start in camera time, systemTime the acquisition start in system time
    cameraTime = m_cameraTime;
    systemTime = m_systemTime;
}

// TODO NOTE: So we need this baked-in struct to function properly.. ?
void SingleCameraImageEventHandler::stream_callback(void *user_data, ArvStreamCallbackType type, ArvBuffer *buffer) {

    ArvStreamCallbackData *callbackData = (ArvStreamCallbackData *) user_data;

    /* This code is called from the stream receiving thread, which means all the time spent there is less time
     * available for the reception of incoming packets */

    // declarations need to happen here, out of switch-cases, due to compiler rule
    size_t buffer_size;
    cv::Mat img;
    CameraImage result;
    bool sc;
    quint64 timeStamp;
    //quint64 chrono_time;

    ArvBufferStatus bs;

    //guint n_buffers = arv_stream_get_n_buffers(callbackData->stream,);
    //qDebug() << "Total buffers: %u" << n_buffers;

    //qDebug() << "call";

    switch (type) {
        case ARV_STREAM_CALLBACK_TYPE_INIT:
            // //arv_make_thread_realtime(31);
            sc = arv_make_thread_high_priority(25);
            //qDebug() << "arv_make_thread_high_priority call returned: " << sc;
            qDebug() << "ARV_STREAM_CALLBACK_TYPE_INIT, Stream thread started";
            break;
        case ARV_STREAM_CALLBACK_TYPE_START_BUFFER:
            qDebug() << "ARV_STREAM_CALLBACK_TYPE_START_BUFFER, The first packet of a new frame was received";
            break;
        case ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE:
            // The buffer is received, successfully or not. It is already pushed in the output FIFO.
            // You could here signal the new buffer to another thread than the main one, and pull/push the
            // buffer from this another thread.
            // Or use the buffer here. We need to pull it, process it, then push it back for reuse by the
            // stream receiving thread

            //qDebug() << "ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE, The buffer is received, successfully or not";

            g_assert (buffer == arv_stream_pop_buffer(callbackData->stream));
            g_assert (buffer != NULL);

            bs = arv_buffer_get_status(buffer);
            switch(bs) {
                case ARV_BUFFER_STATUS_UNKNOWN:
                    qDebug() << "ARV_BUFFER_STATUS_UNKNOWN";
                    break;
                case ARV_BUFFER_STATUS_SUCCESS:
                    qDebug() << "ARV_BUFFER_STATUS_SUCCESS";
                    break;
                case ARV_BUFFER_STATUS_CLEARED:
                    qDebug() << "ARV_BUFFER_STATUS_CLEARED";
                    break;
                case ARV_BUFFER_STATUS_TIMEOUT:
                    qDebug() << "ARV_BUFFER_STATUS_TIMEOUT";
                    break;
                case ARV_BUFFER_STATUS_MISSING_PACKETS:
                    qDebug() << "ARV_BUFFER_STATUS_MISSING_PACKETS";
                    break;
                case ARV_BUFFER_STATUS_WRONG_PACKET_ID:
                    qDebug() << "ARV_BUFFER_STATUS_WRONG_PACKET_ID";
                    break;
                case ARV_BUFFER_STATUS_SIZE_MISMATCH:
                    qDebug() << "ARV_BUFFER_STATUS_SIZE_MISMATCH";
                    break;
                case ARV_BUFFER_STATUS_FILLING:
                    qDebug() << "ARV_BUFFER_STATUS_FILLING";
                    break;
                case ARV_BUFFER_STATUS_ABORTED:
                    qDebug() << "ARV_BUFFER_STATUS_ABORTED";
                    break;
                case ARV_BUFFER_STATUS_PAYLOAD_NOT_SUPPORTED:
                    qDebug() << "ARV_BUFFER_STATUS_PAYLOAD_NOT_SUPPORTED";
                    break;
                default:
                    qDebug() << bs;
            }

            if (arv_buffer_get_status(buffer) != ARV_BUFFER_STATUS_SUCCESS) {
                if(callbackData->aboutToStopGrabbing) {
                    qDebug() << "Grabbing is stopping. All pending frames are dropped.";
                } else {
                    qDebug() << "Image(s) skipped.";
                    emit ((SingleCameraImageEventHandler*)callbackData->emitter)->imagesSkipped();
                }
                // circulate buffer to keep receiving images
                arv_stream_push_buffer(callbackData->stream, buffer);
                //arv_stream_try_pop_buffer(callbackData->stream);
                return;
            }

            //qDebug() << "Acquired " << arv_buffer_get_image_width(buffer) << "x" << arv_buffer_get_image_height(buffer) << " buffer";
            img = cv::Mat(
                    cv::Size(arv_buffer_get_image_width(buffer), arv_buffer_get_image_height(buffer)),
                    CV_8U, (uint8_t *)arv_buffer_get_data(buffer, &buffer_size));
            result.img = img.clone(); // must be copied to keep data content

            // cameraTime describes the acquisition start in camera time, systemTime the acquisition start in system time
            timeStamp = arv_buffer_get_system_timestamp(buffer);
            timeStamp = ((timeStamp-((SingleCameraImageEventHandler*)callbackData->emitter)->cameraTime) / 1000000) + ((SingleCameraImageEventHandler*)callbackData->emitter)->systemTime;

            // Can come here as image is cloned
            arv_stream_push_buffer(callbackData->stream, buffer);
            callbackData->counter++;

            //chrono_time  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            //qDebug() <<
            //    QString("chrono time = ") << QString::number(chrono_time) <<
            //    QString("; acq timestamp = ") << QString::number(timeStamp) <<
            //    QString("; diff = ") << QString::number((int64)chrono_time-(int64)timeStamp) ; // túlcsordul ha másikból vonunk ki.. nagyobból kell

            result.type = CameraImageType::LIVE_SINGLE_CAMERA;
            result.timestamp = timeStamp;

            emit ((SingleCameraImageEventHandler*)callbackData->emitter)->onNewGrabResult(result);

            break;
        case ARV_STREAM_CALLBACK_TYPE_EXIT:
            qDebug() << "ARV_STREAM_CALLBACK_TYPE_EXIT, Stream thread ended";
            // TODO: TUTI KELL EZ?
            //arv_stream_push_buffer(callbackData->stream, buffer);
            break;
    }
}

#endif
