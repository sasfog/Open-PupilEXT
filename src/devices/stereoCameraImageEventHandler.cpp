
#include <opencv2/core.hpp>
#include "stereoCameraImageEventHandler.h"
#include <QDebug>

#ifdef USE_PYLON

// Creates a new stereo image event handler for a StereoCamera
StereoCameraImageEventHandler::StereoCameraImageEventHandler(QObject* parent) : QObject(parent), systemTime(0), stereoImage() {

    formatConverter.OutputPixelFormat = Pylon::PixelType_Mono8;

    stereoImage.timestamp = 0;
    stereoImage.type = CameraImageType::LIVE_STEREO_CAMERA;
}

StereoCameraImageEventHandler::~StereoCameraImageEventHandler() {

}

// Event handler that is executed if for any of the two cameras in the stereo camera images were skipped
// If image skipping happens in one of the two cameras, one may assume that the images are not in sync
// anymore and the onImageGrabbed event handler may not be able produce any stereo images due to unsync framecount
// BG: NOTE: According to Basler docs this will only ever get called when grabStrategy is set to LatestImageOnly or LatestImages, which is never the case for us ..(?)
// https://zh.docs.baslerweb.com/pylonapi/cpp/class_pylon_1_1_c_basler_universal_image_event_handler#function-onimagesskipped
void StereoCameraImageEventHandler::OnImagesSkipped(CInstantCamera& camera, size_t countOfSkippedImages) {
    std::cout << "OnImagesSkipped event for device " << camera.GetDeviceInfo().GetModelName() << std::endl;
    std::cout << countOfSkippedImages  << " images have been skipped." << std::endl;
    std::cout << std::endl;

    emit imagesSkipped();
}

// Event handler that is executed for EACH image acquisition of EACH camera
// This means that for each hardware trigger signal to the two cameras in a StereoCamera, this handler is called two times
// In order to produce a single stereo camera image, combining the two camera acquisitions , OnImageGrabbed synchronized the
// image grab results based on their framenumber. For two consecutive(!) images with the same framenumber, a single new stereo
// camera image is produced and emitted through onNewGrabResult (containing both images).
// In case only a single image was acquired and its consecutive image is of a different framenumber, the first image is dropped.
void StereoCameraImageEventHandler::OnImageGrabbed(CInstantCamera& camera, const CGrabResultPtr& ptrGrabResult) {
    //std::cout << "OnImageGrabbed event for device " << ptrGrabResult->GetCameraContext() << std::endl;

    if (ptrGrabResult->GrabSucceeded()) {
        // As we modify the stereoImage in potentially different threads (Pylon image event handler) lock the mutex for the update
        mutex.lock();

        intptr_t cameraContextValue = ptrGrabResult->GetCameraContext();
        int64_t frameNumber = ptrGrabResult->GetImageNumber();

        uint64_t timeStamp = ptrGrabResult->GetTimeStamp();
        timeStamp = ((timeStamp-cameraTime[cameraContextValue]) + systemTime) / 1000000;

        //std::cout<< "Grabresult from camera" << cameraContextValue << ": frameNumber:  " << frameNumber << ", timestamp: " << timeStamp <<std::endl;

        formatConverter.Convert(pylonImage, ptrGrabResult);
        cv::Mat img(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC1, (uint8_t *) pylonImage.GetBuffer());

        // If the current image and its framenumber are part of the existing stereoImage, then the stereoImage is complete and gets emitted
        // To make sure stereo image consists of two images at the same time from both cameras, their framenumber is checked
        // We assume that when both cameras are started grabbing at the same time, the framenumbers should match (at each camera acquisition start, the framenumber is reset)
        // Combining images based on timestamps showed to be error prone as the time difference between the two images started to drift for unknown reasons

//        int diff = std::abs((int)(timeStamp - stereoImage.timestamp));
//        std::cout<<"Grabresult timestamp diff: "<<diff<<std::endl;

//        int diff2 = std::abs((int)(frameNumber - stereoImage.frameNumber));
//        std::cout<<"Grabresult frameNumber diff: "<<diff2<<std::endl;

        if (camera.GetDeviceInfo().GetModelName().find("Emu") != String_t::npos){
            stereoImage.timestamp = frameNumber; //+= frameNumber;
        }

        //std::cout << "Image frameNumber: " << stereoImage.frameNumber << std::endl;
        //std::cout << "Received frameNumber: " << frameNumber << " Device id: " << camera.GetDeviceInfo().GetDeviceGUID() <<  std::endl;
        if(stereoImage.frameNumber == frameNumber) {
            // If framenumber matches the already contained image in the stereo image this means the missing second images is now found
            // Cameracontextvalue describes the index of the camera in a basler camera array (main or secondary)
            if(cameraContextValue == 0) {
                stereoImage.img = img.clone();
            } else if(cameraContextValue == 1) {
                stereoImage.imgSecondary = img.clone();
            }
            //std::cout<< "Stereoimage complete: " << stereoImage.frameNumber << " " << stereoImage.timestamp <<std::endl;
            //std::cout<< "-------------------------------" <<std::endl;
            emit onNewGrabResult(stereoImage);
//            std::cout << "STEREO GRAB RESULT: " << stereoImage.frameNumber << " AT TIME: " << stereoImage.timestamp << std::endl;
        } else {
            // Else, we have a "new" stereo image, set the timestamp, image and wait for the second missing one, then emit
            stereoImage.timestamp = timeStamp;
            stereoImage.frameNumber = frameNumber;
            if(cameraContextValue == 0) {
                stereoImage.img = img.clone();
            } else if(cameraContextValue == 1) {
                stereoImage.imgSecondary = img.clone();
            }
        }
        mutex.unlock();
    } else {
        std::cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << std::endl;

        // If there is faulty connection /hw. interface problem
        if(ptrGrabResult->GetErrorCode() == 3791651083 || // Error code for "The image stream is out of sync."
           ptrGrabResult->GetErrorCode() == 31 || // Error code for device not functioning
           QString::fromStdString(ptrGrabResult->GetErrorDescription().c_str()).contains("sync")) {
            emit imagesSkipped();
        }
    }

}

// Set the timestamps of both cameras and the system time for a given point in time, used for converting between from cameratime to systemtime
void StereoCameraImageEventHandler::setTimeSynchronization(uint64_t m_mainCameraTime, uint64_t m_secondaryCameraTime, uint64_t m_systemTime) {

    cameraTime[0] = m_mainCameraTime;
    cameraTime[1] = m_secondaryCameraTime;

    systemTime = m_systemTime;
}

#else

StereoCameraImageEventHandler::StereoCameraImageEventHandler(QObject* parent) : QObject(parent), systemTime(0), stereoImage() {
    // Set the image mode to 8 bit grayscale
//    formatConverter.OutputPixelFormat = Pylon::PixelType_Mono8;

    stereoImage.timestamp = 0;
    stereoImage.type = CameraImageType::LIVE_STEREO_CAMERA;
}

StereoCameraImageEventHandler::~StereoCameraImageEventHandler() {

}

// For conversion of the camera timestamp to system time, a tuple of timestamps from the start of the image acquisition is used
void StereoCameraImageEventHandler::setTimeSynchronization(uint64 m_mainCameraTime, uint64 m_secondaryCameraTime, uint64 m_systemTime) {

    cameraTime[0] = m_mainCameraTime;
    cameraTime[1] = m_secondaryCameraTime;

    systemTime = m_systemTime;
}

// TODO NOTE: So we need this baked-in struct to function properly.. ?
void StereoCameraImageEventHandler::stream_callback(void *user_data, ArvStreamCallbackType type, ArvBuffer *buffer) {

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

    int cameraContextValue;
    int64 frameNumber;

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
                    emit ((StereoCameraImageEventHandler*)callbackData->emitter)->imagesSkipped();
                }
                // circulate buffer to keep receiving images
                arv_stream_push_buffer(callbackData->stream, buffer);
                //arv_stream_try_pop_buffer(callbackData->stream);
                return;
            }

            ///////////////////////////////////////

            // As we modify the stereoImage in potentially different threads (Pylon image event handler) lock the mutex for the update
            ((StereoCameraImageEventHandler*)callbackData->emitter)->mutex.lock();

            cameraContextValue = callbackData->cameraContext;
            frameNumber = arv_buffer_get_frame_id(buffer);

            // cameraTime describes the acquisition start in camera time, systemTime the acquisition start in system time
            timeStamp = arv_buffer_get_system_timestamp(buffer);
            timeStamp = ((timeStamp-((StereoCameraImageEventHandler*)callbackData->emitter)->cameraTime[cameraContextValue]) / 1000000) + ((StereoCameraImageEventHandler*)callbackData->emitter)->systemTime;

            //std::cout<< "Grabresult from camera" << cameraContextValue << ": frameNumber:  " << frameNumber << ", timestamp: " << timeStamp <<std::endl;

            //qDebug() << "Acquired " << arv_buffer_get_image_width(buffer) << "x" << arv_buffer_get_image_height(buffer) << " buffer";
            img = cv::Mat(
                    cv::Size(arv_buffer_get_image_width(buffer), arv_buffer_get_image_height(buffer)),
                    CV_8U, (uint8_t *)arv_buffer_get_data(buffer, &buffer_size));
            result.img = img.clone(); // must be copied to keep data content

            // If the current image and its framenumber are part of the existing stereoImage, then the stereoImage is complete and gets emitted
            // To make sure stereo image consists of two images at the same time from both cameras, their framenumber is checked
            // We assume that when both cameras are started grabbing at the same time, the framenumbers should match (at each camera acquisition start, the framenumber is reset)
            // Combining images based on timestamps showed to be error prone as the time difference between the two images started to drift for unknown reasons

//        int diff = std::abs((int)(timeStamp - stereoImage.timestamp));
//        std::cout<<"Grabresult timestamp diff: "<<diff<<std::endl;

//        int diff2 = std::abs((int)(frameNumber - stereoImage.frameNumber));
//        std::cout<<"Grabresult frameNumber diff: "<<diff2<<std::endl;

            // TODO?
//            if (camera.GetDeviceInfo().GetModelName().find("Emu") != String_t::npos){
//                stereoImage.timestamp = frameNumber; //+= frameNumber;
//            }

            // TODO: this frameNumber base
            if(((StereoCameraImageEventHandler*)callbackData->emitter)->stereoImage.frameNumber == frameNumber) {
                // If framenumber matches the already contained image in the stereo image this means the missing second images is now found
                // Cameracontextvalue describes the index of the camera in a basler camera array (main or secondary)
                if(cameraContextValue == 0) {
                    ((StereoCameraImageEventHandler*)callbackData->emitter)->stereoImage.img = img.clone();
                } else if(cameraContextValue == 1) {
                    ((StereoCameraImageEventHandler*)callbackData->emitter)->stereoImage.imgSecondary = img.clone();
                }
                //std::cout<< "Stereoimage complete: " << stereoImage.frameNumber << " " << stereoImage.timestamp <<std::endl;
                //std::cout<< "-------------------------------" <<std::endl;
                emit ((StereoCameraImageEventHandler*)callbackData->emitter)->onNewGrabResult(((StereoCameraImageEventHandler*)callbackData->emitter)->stereoImage);
//            std::cout << "STEREO GRAB RESULT: " << stereoImage.frameNumber << " AT TIME: " << stereoImage.timestamp << std::endl;
            } else {
                // Else, we have a "new" stereo image, set the timestamp, image and wait for the second missing one, then emit
                ((StereoCameraImageEventHandler*)callbackData->emitter)->stereoImage.timestamp = timeStamp;
                ((StereoCameraImageEventHandler*)callbackData->emitter)->stereoImage.frameNumber = frameNumber;
                if(cameraContextValue == 0) {
                    ((StereoCameraImageEventHandler*)callbackData->emitter)->stereoImage.img = img.clone();
                } else if(cameraContextValue == 1) {
                    ((StereoCameraImageEventHandler*)callbackData->emitter)->stereoImage.imgSecondary = img.clone();
                }
            }
            ((StereoCameraImageEventHandler*)callbackData->emitter)->mutex.unlock();

            ///////////////////////////////////////

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

            emit ((StereoCameraImageEventHandler*)callbackData->emitter)->onNewGrabResult(result);

            break;
        case ARV_STREAM_CALLBACK_TYPE_EXIT:
            qDebug() << "ARV_STREAM_CALLBACK_TYPE_EXIT, Stream thread ended";
            // TODO: TUTI KELL EZ?
            //arv_stream_push_buffer(callbackData->stream, buffer);
            break;
    }
}

// TODO: do this properly, after having implemented a multiple-frame buffer stack instead of this one(x2) images buffer
void StereoCameraImageEventHandler::dropAllPending() {
    stereoImage.img = cv::Mat();
    stereoImage.imgSecondary = cv::Mat();
    stereoImage.timestamp = 0;
}

#endif
