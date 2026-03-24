
#include <QtConcurrent/QtConcurrent>
#include "pupilDetection.h"
#include "pupil-detection-methods/ElSe.h"
#include "pupil-detection-methods/ExCuSe.h"
#include "pupil-detection-methods/PuRe.h"
#include "pupil-detection-methods/Starburst.h"
#include "pupil-detection-methods/Swirski3D.h"
#include "pupil-detection-methods/PuReST.h"
#include "pupil-detection-methods/Swirski2D.h"
#include "devices/stereoCamera.h"
#include "devices/fileCamera.h"

#include <fstream>
#include <cmath>


// QTs event signal eventloop queues images for us, we run this pupildetection worker in a extra thread and every call to newImage is queued automatically
// This may however queue a large number of images if the processing speed is slow, increasing the memory potentially until it is full and the application is killed
// We dont have access to the eventloop to handle this, so its up to the user for now to not use a rate too high

void PupilDetection::populateWithMethods(std::vector<PupilDetectionMethod*> &vec) {
    vec.push_back(new ElSe());
    vec.push_back(new ExCuSe());
    vec.push_back(new PuRe());
    vec.push_back(new PuReST());
    vec.push_back(new Starburst());
    vec.push_back(new Swirski2D());

    // TODO finish integrating Swirski3D
    //double focal_length = 0.5;
    //pupilDetectionMethods.push_back(new Swirski3D(new PuReST(), focal_length, 5, 0.5));
}

// Creates a new pupil detection worker which include all pupil detection algorithms
// Should be run on a seperate thread
PupilDetection::PupilDetection(QMutex *imageMutex, QWaitCondition *imagePublished, QWaitCondition *imageProcessed, QObject *parent) :
        QObject(parent),
        camera(nullptr),
        frameCounter(new FrameRateCounter(parent)),
        useOutlineConfidence(true),
        useROIPreProcessing(false),
        useImageUndistort(false),
        usePupilUndistort(false),
        trackingOn(false),
        calibrated(false),
    //showROI(true),
    //showPupilCenter(false),
    autoParamEnabled(false),
        currentConfigLabel("Default"),
        currentProcMode(ProcMode::UNDETERMINED),
        ROIsingleImageOnePupil(),
        ROIsingleImageTwoPupilR(),
        ROIsingleImageTwoPupilL(),
        ROIstereoImageOnePupilM(),
        ROIstereoImageOnePupilS(),
        ROIstereoImageTwoPupilRM(),
        ROIstereoImageTwoPupilRS(),
        ROIstereoImageTwoPupilLM(),
        ROIstereoImageTwoPupilLS(),
        ROImirrImageOnePupil1(),
        ROImirrImageOnePupil2(),
        imageMutex(imageMutex),
        imagePublished(imagePublished),
        imageProcessed(imageProcessed)
    {

    drawDelay = 33; // ~30fps

    // we initialize the algorithms here one time and save them in a list, the created objects are then changed through index change of the list
    populateWithMethods(pupilDetectionMethods1);
    populateWithMethods(pupilDetectionMethods2);
    populateWithMethods(pupilDetectionMethods3);
    populateWithMethods(pupilDetectionMethods4);

    // Default algorithm PuRe
    pupilDetectionIndex = 2;

    // Processing speed frame counter
    connect(frameCounter, SIGNAL(fps(double)), this, SIGNAL(fps(double)));
    assert( connect(this, SIGNAL(processedPupilData(quint64, int, std::vector<Pupil>)), frameCounter, SLOT(count())) );
    // DEV
    //frameCounter->setParentName("pupil detection");

    drawTimer.start();
    processingTimer.start();

    //configureCameraConnection();
}

PupilDetection::~PupilDetection() {
    brisque.release();
    //BRISQUEModel.release();
    //brisque = nullptr;
}

void PupilDetection::enableComputeBRISQUE(bool state) {
    if(state) {
        computeBRISQUEEnabled = true;

        if(!brisque || brisque.empty()) {
            // init BRISQUE
            QFile fileBRISQUEModel(BRISQUEModelFileNameRES);
            fileBRISQUEModel.open(QIODevice::ReadOnly | QIODevice::Text);
            cv::FileStorage fsBRISQUEModel(fileBRISQUEModel.readAll().constData(),
                                           cv::FileStorage::READ | cv::FileStorage::MEMORY);
            cv::Ptr<cv::ml::SVM> BRISQUEModel = cv::ml::SVM::create();
            BRISQUEModel->read(fsBRISQUEModel.getFirstTopLevelNode());
            //
            QFile fileBRISQUERange(BRISQUERangeFileNameRES);
            fileBRISQUERange.open(QIODevice::ReadOnly | QIODevice::Text);
            cv::FileStorage fsBRISQUERange(fileBRISQUERange.readAll().constData(),
                                           cv::FileStorage::READ | cv::FileStorage::MEMORY);
            cv::Mat BRISQUERange;
            fsBRISQUERange["range"] >> BRISQUERange;
            //
            brisque = cv::quality::QualityBRISQUE::create(BRISQUEModel, BRISQUERange);
            //brisque = cv::quality::QualityBRISQUE::create("./brisque_model_live.yml", "./brisque_range_live.yml");
        }
    } else {
        computeBRISQUEEnabled = false;
    }
}

// Attaches a camera to the pupil detection process
// Checks if the camera is calibrated and stereo or single, sets the detection mode accordingly
void PupilDetection::setCamera(Camera *m_camera) {

    if (camera != m_camera) {
        camera = m_camera;

        // This can happen upon camera disconnect, especially important upon main window closing when a camera was open
        // The m_camera is set to nullptr then, but the following code does not get executed because of this return;
        if(!m_camera)
            return;

        calibrated = false;

        if (camera->getType() == CameraImageType::LIVE_STEREO_CAMERA) {
            stereoCalibration = dynamic_cast<StereoCamera *>(camera)->getCameraCalibration();
            calibrated = static_cast<bool>(stereoCalibration->isCalibrated());
        } else if (camera->getType() == CameraImageType::STEREO_IMAGE_FILE) {
            stereoCalibration = dynamic_cast<FileCamera *>(camera)->getStereoCameraCalibration();
            calibrated = static_cast<bool>(stereoCalibration->isCalibrated());
        } else if (camera->getType() == CameraImageType::LIVE_SINGLE_CAMERA) {
            singleCalibration = dynamic_cast<SingleCamera *>(camera)->getCameraCalibration();
            calibrated = static_cast<bool>(singleCalibration->isCalibrated());
        } else if (camera->getType() == CameraImageType::SINGLE_IMAGE_FILE) {
            singleCalibration = dynamic_cast<FileCamera *>(camera)->getCameraCalibration();
            calibrated = static_cast<bool>(singleCalibration->isCalibrated());
        }
        else if (camera->getType() == CameraImageType::LIVE_SINGLE_WEBCAM) {
            singleCalibration = dynamic_cast<SingleWebcam *>(camera)->getCameraCalibration();
            calibrated = static_cast<bool>(singleCalibration->isCalibrated());
        }
        configureCameraConnection(true);
    }
}

void PupilDetection::startTracking() {

    // TODO: check if ROIs are set ?

    // BG: NOTE: maybe not here? But one algorithm is changed, we certainly need to re-parameter
    if(autoParamEnabled)
        performAutoParam();

    trackingOn = true;
    if(camera) {
        //configureCameraConnection();
        emit processingStarted();
    }
}

void PupilDetection::stopTracking() {

    if(camera && trackingOn) {
        trackingOn = false;

        emit processingFinished();
        imageProcessed->wakeAll();
        imagePublished->wakeAll();
        qDebug() << "Woke up imageProcessing";
    }
    qDebug() << "Woke up imageProcessing";
}

// Changes the applied pupil detection algorithm
// The change is performed by first disconnecting the signal to stop potential frames, switch the algorithm and connect them again
// Emits a signal to signal the algorithm changed
void PupilDetection::setAlgorithm(QString method) {

    configureCameraConnection(false);

    frameCounter->reset();

    int i = 0;
    for(auto pm: pupilDetectionMethods1) {
        //if(pm->title() == method.toStdString())
        //modified for tolower to care for when someone is setting this through an UDP command and had a case-typo
        if(QString::fromStdString(pm->title()).toLower() == method.toLower())
            pupilDetectionIndex = i;
        i++;
    }

    // NOTE: maybe not here? But one algorithm is changed, we certainly need to re-parameter
    if(autoParamEnabled)
        autoParamScheduled = true;

    emit algorithmChanged();

    configureCameraConnection(true);
}

// Slot callback for receiving new single camera images
// Performs the processing/pupil detection
// Emits the pupil detection result as a signal, as well as processed images with plotted pupil contours
// Depending on the configuration, performs undistortion on the images or pupil detections
void PupilDetection::onNewSingleImageForOnePupil(const CameraImage &cimg) {

    // Can sometimes weirdly happen, when closing camera. TODO
    if(!camera)
        return;

    if (synchronised) {
        const QMutexLocker locker(imageMutex);
        onNewSingleImageForOnePupilImpl(cimg);
//        qDebug() << "pupilDetection locking";
  //      qDebug() << "pupilDetection image processed, unlocking";
        imagePublished->wakeAll();
        if (trackingOn) {
//            qDebug() << "Locking image processing";
            imageProcessed->wait(imageMutex);
        }
    }
    else {
        onNewSingleImageForOnePupilImpl(cimg);
    }
}

void PupilDetection::onNewSingleImageForOnePupilImpl(const CameraImage &image) {

    // Processing fps restriction not working correctly, timers overhead seem to break timing, left out for now
    //qDebug()<<cimg->filename;
    if (!trackingOn) {
        //qDebug()<<"Single: onNewSingleImageForOnePupil: Tracking is stopped but receiving signals."; // GB: changed text
        //qDebug() << image->frameNumber;
        // TODO: also emit one in case the file camera was PAUSED !
        if ((drawTimer.elapsed() > drawDelay) ||
            (camera->getType() == SINGLE_IMAGE_FILE && (
                    (!static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getLastCommissionedFrameNumber() == image.frameNumber) ||
                    (static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getNumImagesTotal()-1 == image.frameNumber)
            ) ) ) {
            emit processedImageLowFPS(image);
        }
        return;
    }

    cv::Mat bwFrame = image.img;

    // Undistorting the whole image is rather slow (~4ms on our test system), use contour point undistort instead (>~1ms)
    if(!usePupilUndistort && useImageUndistort) {
        //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        bwFrame = singleCalibration->undistortImage(image.img);
        //qDebug()<< std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count() / 1000.0 ;
    }

    cv::Rect roi = cv::Rect(0, 0, bwFrame.cols, bwFrame.rows);

    if(useROIPreProcessing && !ROIsingleImageOnePupil.empty() && roi != ROIsingleImageOnePupil && ROIsingleImageOnePupil.width<=bwFrame.cols && ROIsingleImageOnePupil.height<=bwFrame.rows) {
        roi = ROIsingleImageOnePupil;
        bwFrame = bwFrame(ROIsingleImageOnePupil);
    } else if(autoParamEnabled && autoParamScheduled)
        ROIsingleImageOnePupil = roi;

    if(autoParamEnabled && autoParamScheduled) {
        performAutoParam();
        autoParamScheduled = false;
    }

    // TODO: REMOVE. ALREADY DONE BEFORE
    if (bwFrame.channels() > 1) {
        cv::cvtColor(bwFrame, bwFrame, cv::COLOR_BGR2GRAY);
    }

    Pupil pupil = Pupil();

    // Pupil detection
    try {
        if(useOutlineConfidence) {

            //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            pupilDetectionMethods1[pupilDetectionIndex]->runWithConfidence(bwFrame, pupil);
            //runtimeHistory.push_back(std::make_pair(cimg->timestamp, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()));
        } else {
            //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            pupilDetectionMethods1[pupilDetectionIndex]->run(bwFrame, pupil);
            //runtimeHistory.push_back(std::make_pair(cimg->timestamp, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()));
        }
    } catch (...) {
        pupil.clear();
    }

    // Shift the pupil center position to be in the coordinate of the whole image instead of the ROI
    if(useROIPreProcessing) {
        pupil.shift(roi.tl());
    }

    // Undistort the pupil contour points to get an undistorted pupil size
    if(usePupilUndistort && !useImageUndistort) {
        //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        pupil.undistortedDiameter = singleCalibration->undistortPupilDiameter(pupil);
        //qDebug()<< std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count() / 1000.0 ;
    } else if(!usePupilUndistort && useImageUndistort) {
        pupil.undistortedDiameter = pupil.diameter();
    }

    pupil.algorithmName = pupilDetectionMethods1[pupilDetectionIndex]->title();

    if(computeBRISQUEEnabled) {
        pupil.BRISQUEFullImage = brisque->compute(image.img)[0];
        pupil.BRISQUEPDROI = brisque->compute(bwFrame)[0];
        //pupil.BRISQUEPDInternal = ...;
        //std::cout << "BRISQUE SCORE: " << score[0] << std::endl;
    }

    std::vector<Pupil> Pupils;
    Pupils.push_back(pupil);

    // Drawing of pupil detections on the image is only performed at ~30fps
    // NOTE: It is important to not only check for drawDelay, but care for the special case,
    // when the last signals from an image playback arrive before another drawDelay is happened,
    // to not make the playback stuck just near the end. That is why we check for frame number too
    if ((drawTimer.elapsed() > drawDelay) ||
        (camera->getType() == SINGLE_IMAGE_FILE && (
            (!static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getLastCommissionedFrameNumber() == image.frameNumber) ||
            (static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getNumImagesTotal()-1 == image.frameNumber)
            ) ) ) {

        drawTimer.start();

        const CameraImage &mimg = image;
        mimg.img = image.img.clone();

        if(!usePupilUndistort && useImageUndistort) {
            mimg.img = singleCalibration->undistortImage(image.img);
        }
        // not necessary to copy twice
        //else {
        //    mimg->img = cimg->img.clone();
        //}ú
        /*

        // moved code to singleCameraView code
        if (mimg->img.channels() == 1) {
            cv::cvtColor(mimg->img, mimg->img, cv::COLOR_GRAY2BGR);
        }

        //if(showROI)
            cv::rectangle(mimg->img, roi, cv::Scalar(255, 0, 255 ), 3);

        if(pupil.valid(-2.0)) {
            cv::ellipse(mimg->img, pupil, cv::Scalar( 0, 200, 255 ), 1); //BG: it was 0,0,255
            //if(showPupilCenter)
                cv::circle(mimg->img, pupil.center, 1, CV_RGB(255,0,0),3);
        } else {
            cv::putText(mimg->img, "NO PUPIL FOUND", cv::Point(static_cast<int>(0.25 * mimg->img.cols),
                                                              static_cast<int>(0.25 * mimg->img.rows)), cv::FONT_HERSHEY_PLAIN, 4, cv::Scalar(255, 0, 255), 4);
        }
        emit processedImageLowFPS(mimg);
        */

        std::vector<cv::Rect> ROIs;
        if(useROIPreProcessing) {
            ROIs.push_back(ROIsingleImageOnePupil);
        } else {
            ROIs.push_back(roi);
            //ROIs.push_back(cv::Rect(0,0, bwFrame.size().width, bwFrame.size().height));
        }
        emit processedImageLowFPS(mimg, currentProcMode, ROIs, Pupils);

        // to inform imagePlaybackControlDialog about the just processed image
        if(camera->getType() == SINGLE_IMAGE_FILE) {
            emit processedImageLowFPS(mimg);
//            qDebug() << image.frameNumber;
        }
        //qDebug() << "frameNumber left pupilDetection: " << cimg->frameNumber;
        emit processedPupilDataLowFPS(image.timestamp, currentProcMode, Pupils);
    }

    emit processedPupilData(image.timestamp, currentProcMode, Pupils);
}

// Slot callback for receiving new single camera images that contain two pupils/eyes
// Performs the processing/pupil detection
// Emits the pupil detection result as a signal, as well as processed images with plotted pupil contours
// Depending on the configuration, performs undistortion on the images or pupil detections
void PupilDetection::onNewSingleImageForTwoPupil(const CameraImage &cimg) {

    // Can sometimes weirdly happen, when closing camera. TODO
    if(!camera)
        return;

    if (synchronised) {
        const QMutexLocker locker(imageMutex);
        onNewSingleImageForTwoPupilImpl(cimg);
//        qDebug() << "pupilDetection locking";
        //      qDebug() << "pupilDetection image processed, unlocking";
        imagePublished->wakeAll();
        if (trackingOn) {
//            qDebug() << "Locking image processing";
            imageProcessed->wait(imageMutex);
        }
    }
    else {
        onNewSingleImageForTwoPupilImpl(cimg);
    }

}

void PupilDetection::onNewSingleImageForTwoPupilImpl(const CameraImage &cimg) {

    if (!trackingOn) {
//        qDebug() << cimg.frameNumber;
        // TODO: also emit one in case the file camera was PAUSED !
        if ((drawTimer.elapsed() > drawDelay) ||
            (camera->getType() == SINGLE_IMAGE_FILE && (
                    (!static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getLastCommissionedFrameNumber() == cimg.frameNumber) ||
                    (static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getNumImagesTotal()-1 == cimg.frameNumber)
            ) ) ) {
            emit processedImageLowFPS(cimg);
        }
        return;
    }

    cv::Mat bwFrameA;
    cv::Mat bwFrameB;

    // Undistorting the whole image is rather slow (~4ms on our test system), use contour point undistort instead (>~1ms)
    if(!usePupilUndistort && useImageUndistort) {
        //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        bwFrameA = singleCalibration->undistortImage(cimg.img);
        //qDebug()<< std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count() / 1000.0 ;
    } else {
        bwFrameA = cimg.img;
    }
    bwFrameB = bwFrameA;

    // BG: NOTE: by default we only use the left and right halves of the input image
    cv::Rect roiA = cv::Rect(0, 0, (int)std::floor(cimg.img.cols/2)-1, cimg.img.rows);
    cv::Rect roiB = cv::Rect((int)std::ceil(cimg.img.cols/2)+1, 0, cimg.img.cols, cimg.img.rows);

    if(useROIPreProcessing && !ROIsingleImageTwoPupilR.empty() && roiA != ROIsingleImageTwoPupilR && ROIsingleImageTwoPupilR.width <= bwFrameA.cols && ROIsingleImageTwoPupilR.height <= bwFrameA.rows) {
        roiA = ROIsingleImageTwoPupilR;
        bwFrameA = bwFrameA(ROIsingleImageTwoPupilR);
    } else if(autoParamEnabled && autoParamScheduled)
        ROIsingleImageTwoPupilR = roiA;

    if(useROIPreProcessing && !ROIsingleImageTwoPupilL.empty() && roiB != ROIsingleImageTwoPupilL && ROIsingleImageTwoPupilL.width <= bwFrameB.cols && ROIsingleImageTwoPupilL.height <= bwFrameB.rows) {
        roiB = ROIsingleImageTwoPupilL;
        bwFrameB = bwFrameB(ROIsingleImageTwoPupilL);
    } else if(autoParamEnabled && autoParamScheduled)
        ROIsingleImageTwoPupilL = roiB;

    if(autoParamEnabled && autoParamScheduled) {
        performAutoParam();
        autoParamScheduled = false;
    }

    if (cimg.img.channels() > 1) {
        cv::cvtColor(bwFrameA, bwFrameA, cv::COLOR_BGR2GRAY);
        cv::cvtColor(bwFrameB, bwFrameB, cv::COLOR_BGR2GRAY);
    }

    // We execute pupil detection for main and secondary images concurrently using treads, we execute both in separate threads, then wait till both are finished
    QFutureSynchronizer<Pupil> synchronizer;
    Pupil pupilA;
    Pupil pupilB;

    try {
        //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        if(useOutlineConfidence) {

            // calibrationSuccess = QtConcurrent::run([this]{ return CameraCalibration::calibrate(); }); // Qt6 compatible
            // //calibrationSuccess = QtConcurrent::run(this, &CameraCalibration::calibrate); // GB NOTE: this line was the Qt5 compatible version

            // Qt6
            auto pdm1 = pupilDetectionMethods1[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm1, bwFrameA] { return pdm1->runWithConfidence(bwFrameA); }) );
            auto pdm2 = pupilDetectionMethods2[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm2, bwFrameB] { return pdm2->runWithConfidence(bwFrameB); }) );

            // Qt5
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods1[pupilDetectionIndex], &PupilDetectionMethod::runWithConfidence, bwFrameA)); // GB NOTE: this line was the Qt5 compatible version
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods2[pupilDetectionIndex], &PupilDetectionMethod::runWithConfidence, bwFrameB)); // GB NOTE: this line was the Qt5 compatible version
        } else {

            // Qt6
            auto pdm1 = pupilDetectionMethods1[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm1, bwFrameA] { return pdm1->run(bwFrameA); }) );
            auto pdm2 = pupilDetectionMethods2[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm2, bwFrameB] { return pdm2->run(bwFrameB); }) );

            // Qt5
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods1[pupilDetectionIndex], &PupilDetectionMethod::run, bwFrameA)); // GB NOTE: this line was the Qt5 compatible version
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods2[pupilDetectionIndex], &PupilDetectionMethod::run, bwFrameB)); // GB NOTE: this line was the Qt5 compatible version

        }
        synchronizer.waitForFinished();
        // Unhandled exceptions in the QtConcurrent::run function are thrown at the result() call
        pupilA = synchronizer.futures().at(0).result();
        pupilB = synchronizer.futures().at(1).result();
    } catch (...) {
        pupilA.clear();
        pupilB.clear();
    }

    // Shift the pupil position back to the original image coordinates instead of ROI
    if(useROIPreProcessing) {
        pupilA.shift(roiA.tl());
        pupilB.shift(roiB.tl());
    }

    if(usePupilUndistort && !useImageUndistort) {
        pupilA.undistortedDiameter = singleCalibration->undistortPupilDiameter(pupilA);
        pupilB.undistortedDiameter = singleCalibration->undistortPupilDiameter(pupilB);
    } else if(!usePupilUndistort && useImageUndistort) {
        pupilA.undistortedDiameter = pupilA.diameter();
        pupilB.undistortedDiameter = pupilB.diameter();
    }

    pupilA.algorithmName = pupilDetectionMethods1[pupilDetectionIndex]->title();
    pupilB.algorithmName = pupilA.algorithmName;

    // TODO: ? Implement basic pythagorean px-mm mapping

    if(computeBRISQUEEnabled) {
        pupilA.BRISQUEFullImage = brisque->compute(cimg.img)[0];
        pupilA.BRISQUEPDROI = brisque->compute(bwFrameA)[0];
        pupilB.BRISQUEFullImage = pupilA.BRISQUEFullImage;
        pupilB.BRISQUEPDROI = brisque->compute(bwFrameB)[0];
        //pupil.BRISQUEPDInternal = ...;
        // std::cout << "BRISQUE SCORE: " << score[0] << std::endl;
    }

    std::vector<Pupil> Pupils;
    Pupils.push_back(pupilA);
    Pupils.push_back(pupilB);

    // NOTE: It is important to not only check for drawDelay, but care for the special case,
    // when the last signals from an image playback arrive before another drawDelay is happened,
    // to not make the playback stuck just near the end. That is why we check for frame number too
    if ((drawTimer.elapsed() > drawDelay) ||
        (camera->getType() == SINGLE_IMAGE_FILE && (
                (!static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getLastCommissionedFrameNumber() == cimg.frameNumber) ||
                (static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getNumImagesTotal()-1 == cimg.frameNumber)
        ) ) ) {
        // NOTE: need to emit the processed image and data also when the user hits pause or stop, and we are waiting there for the last read image to arrive processed

        drawTimer.start();

        const CameraImage &mimg = cimg;
//        mimg->img = cimg->img.clone();
// //        mimg->imgB = cimg->img.clone(); // would be the same

        if(!usePupilUndistort && useImageUndistort) {
            mimg.img = singleCalibration->undistortImage(cimg.img);
        } else {
            mimg.img = cimg.img.clone();
        }

        std::vector<cv::Rect> ROIs;
        if(useROIPreProcessing) {
            ROIs.push_back(ROIsingleImageTwoPupilR);
            ROIs.push_back(ROIsingleImageTwoPupilL);
        } else {
            ROIs.push_back(roiA);
            ROIs.push_back(roiB);
            // ROIs.push_back(cv::Rect(0, 0, bwFrameA.size().width, bwFrameA.size().height));
            // ROIs.push_back(cv::Rect((int)std::ceil(cimg->img.cols/2)+1, 0, bwFrameB.size().width, bwFrameB.size().height));
        }
        emit processedImageLowFPS(mimg, currentProcMode, ROIs, Pupils);

        // to inform imagePlaybackControlDialog about the just processed image
        if(camera->getType() == SINGLE_IMAGE_FILE)
                emit processedImageLowFPS(mimg);

        emit processedPupilDataLowFPS(cimg.timestamp, currentProcMode, Pupils);
    }

    emit processedPupilData(cimg.timestamp, currentProcMode, Pupils);

}

// Slot callback for receiving new stereo camera images
// Performs the processing/pupil detection
// Emits the pupil detection result as a signal, as well as processed images with plotted pupil contours
// Depending on the configuration, performs undistortion on the pupil detections
void PupilDetection::onNewStereoImageForOnePupil(const CameraImage &simg) {

    // Can sometimes weirdly happen, when closing camera. TODO
    if(!camera)
        return;

    if (synchronised) {
        const QMutexLocker locker(imageMutex);
        onNewStereoImageForOnePupilImpl(simg);
//        qDebug() << "pupilDetection locking";
        //      qDebug() << "pupilDetection image processed, unlocking";
        imagePublished->wakeAll();
        if (trackingOn) {
//            qDebug() << "Locking image processing";
            imageProcessed->wait(imageMutex);
        }
    }
    else {
        onNewStereoImageForOnePupilImpl(simg);
    }

}


void PupilDetection::onNewStereoImageForOnePupilImpl(const CameraImage &simg) {

    // at the moment, the images are not undistorted completely but only the major axis points are undistorted after detection for absolute unit conversion
    // This creates a discrepancy between the undistorted pixel size and the physical measure, as a fix, undistortedDiamter can be calculated using useUndistort

    if (!trackingOn) {
//        qDebug() << simg.frameNumber;
        // TODO: also emit one in case the file camera was PAUSED !
        if ((drawTimer.elapsed() > drawDelay) ||
            (camera->getType() == STEREO_IMAGE_FILE && (
                    (!static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getLastCommissionedFrameNumber() == simg.frameNumber) ||
                    (static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getNumImagesTotal()-1 == simg.frameNumber)
            ) ) ) {
            emit processedImageLowFPS(simg);
        }
        return;
    }

    cv::Mat bwFrameM = simg.img;
    cv::Mat bwFrameS = simg.imgS;
    cv::Rect roiM = cv::Rect(0, 0, simg.img.cols, simg.img.rows);
    cv::Rect roiS = cv::Rect(0, 0, simg.img.cols, simg.img.rows);

    // GB: like this the global ROI variables can inform performAutoParam() about ROI sizes
    if(useROIPreProcessing && !ROIstereoImageOnePupilM.empty() && roiM != ROIstereoImageOnePupilM && ROIstereoImageOnePupilM.width <= bwFrameM.cols && ROIstereoImageOnePupilM.height <= bwFrameM.rows) {
        roiM = ROIstereoImageOnePupilM;
        bwFrameM = bwFrameM(ROIstereoImageOnePupilM);
    } else if(autoParamEnabled && autoParamScheduled)
        ROIstereoImageOnePupilM = roiM;

    if(useROIPreProcessing && !ROIstereoImageOnePupilS.empty() && roiS != ROIstereoImageOnePupilS && ROIstereoImageOnePupilS.width <= bwFrameS.cols && ROIstereoImageOnePupilS.height <= bwFrameS.rows) {
        roiS = ROIstereoImageOnePupilS;
        bwFrameS = bwFrameS(ROIstereoImageOnePupilS);
    } else if(autoParamEnabled && autoParamScheduled)
        ROIstereoImageOnePupilS = roiS;

    if(autoParamEnabled && autoParamScheduled) {
        performAutoParam();
        autoParamScheduled = false;
    }

    if (simg.img.channels() > 1) {
        cv::cvtColor(bwFrameM, bwFrameM, cv::COLOR_BGR2GRAY);
        cv::cvtColor(bwFrameS, bwFrameS, cv::COLOR_BGR2GRAY);
    }

    // We execute pupil detection for main and secondary images concurrently using treads, we execute both in separate threads, then wait till both are finished
    QFutureSynchronizer<Pupil> synchronizer;
    Pupil pupilM;
    Pupil pupilS;

    try {
        //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        if(useOutlineConfidence) {

            // Qt6
            auto pdm1 = pupilDetectionMethods1[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm1, bwFrameM] { return pdm1->runWithConfidence(bwFrameM); }) );
            auto pdm2 = pupilDetectionMethods2[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm2, bwFrameS] { return pdm2->runWithConfidence(bwFrameS); }) );

            // Qt5
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods1[pupilDetectionIndex], &PupilDetectionMethod::runWithConfidence, bwFrameM));
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods2[pupilDetectionIndex], &PupilDetectionMethod::runWithConfidence, bwFrameS));
        } else {

            // Qt6
            auto pdm1 = pupilDetectionMethods1[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm1, bwFrameM] { return pdm1->run(bwFrameM); }) );
            auto pdm2 = pupilDetectionMethods2[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm2, bwFrameS] { return pdm2->run(bwFrameS); }) );

            // Qt5
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods1[pupilDetectionIndex], &PupilDetectionMethod::run, bwFrameM));
            //.addFuture(QtConcurrent::run(pupilDetectionMethods2[pupilDetectionIndex], &PupilDetectionMethod::run, bwFrameS));
        }
        synchronizer.waitForFinished();
        // Unhandled exceptions in the QtConcurrent::run function are thrown at the result() call
        pupilM = synchronizer.futures().at(0).result();
        pupilS = synchronizer.futures().at(1).result();
    } catch (...) {
        pupilM.clear();
        pupilS.clear();
    }
    //runtimeHistory.push_back(std::make_pair(simg->timestamp, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()));

    // Shift the pupil position back to the original image coordinates instead of ROI
    if(useROIPreProcessing) {
        pupilM.shift(roiM.tl());
        pupilS.shift(roiS.tl());
    }

    if(usePupilUndistort && !useImageUndistort) {
        //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        std::pair<double, double> diameters = stereoCalibration->undistortPupilDiameters(pupilM, pupilS);
        pupilM.undistortedDiameter = diameters.first;
        pupilS.undistortedDiameter = diameters.second;
        //qDebug()<< std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count() / 1000.0 ;
    } else if(!usePupilUndistort && useImageUndistort) {
        pupilM.undistortedDiameter = pupilM.diameter();
        pupilS.undistortedDiameter = pupilM.diameter(); // NOTE: is it okay to just accept the undistorted diameter of main for the sec too?
    }

    pupilM.algorithmName = pupilDetectionMethods1[pupilDetectionIndex]->title();
    pupilS.algorithmName = pupilM.algorithmName;

    // If both pupil detections are valid and the camera is calibrated, we can perform unit conversion to absolute measure
    if(pupilM.valid(-2.0) && pupilS.valid(-2.0) && calibrated) {
        //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

        Pupil rotPupilM(pupilM);
        rotPupilM.angle += 360-rotPupilM.angle;
        Pupil rotPupilS(pupilS);
        rotPupilS.angle += 360-rotPupilS.angle;

        // convert pupil detection from pixel into mm through stereo calibration
        // Select the top left and top right corner of the bounding rects of the pupils as the points we triangulate
        cv::Point2f pointsArrM[4]; // rotatedRect points in order: bottomLeft, topLeft, topRight, bottomRight
        rotPupilM.points(pointsArrM);
        int secondPoint = rotPupilM.size.width > rotPupilM.size.height ? 2 : 0; // if the pupil major axis is horizontal use topLeft and topRight, else topLeft and bottomLeft
        //cv::line(mimg->img,pointsArrM[1], pointsArrM[secondPoint], cv::Scalar(255, 0, 0));

        cv::Point2f pointsArrS[4];
        rotPupilS.points(pointsArrS);
        //cv::line(mimg->imgS,pointsArrS[1], pointsArrS[secondPoint], cv::Scalar(255, 0, 0));

        std::vector<cv::Point2f> pointsM{pointsArrM[1], pointsArrM[secondPoint]}, pointsS{pointsArrS[1], pointsArrS[secondPoint]};
        std::vector<cv::Point3f> worldPoints = stereoCalibration->convertPointsTo3D(pointsM, pointsS);

        pupilM.physicalDiameter = static_cast<float>(cv::norm(worldPoints[0] - worldPoints[1]));
        pupilS.physicalDiameter = pupilM.physicalDiameter;
        //runtimeHistory.push_back(std::make_pair(simg->timestamp, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()));
    }

    if(computeBRISQUEEnabled) {
        pupilM.BRISQUEFullImage = brisque->compute(simg.img)[0];
        pupilM.BRISQUEPDROI = brisque->compute(bwFrameM)[0];
        pupilS.BRISQUEFullImage = brisque->compute(simg.imgS)[0];
        pupilS.BRISQUEPDROI = brisque->compute(bwFrameS)[0];
        //pupil.BRISQUEPDInternal = ...;
        //std::cout << "BRISQUE SCORE: " << score[0] << std::endl;
    }

    std::vector<Pupil> Pupils;
    Pupils.push_back(pupilM);
    Pupils.push_back(pupilS);

    // NOTE: It is important to not only check for drawDelay, but care for the special case,
    // when the last signals from an image playback arrive before another drawDelay is happened,
    // to not make the playback stuck just near the end. That is why we check for frame number too
    if ((drawTimer.elapsed() > drawDelay) ||
        (camera->getType() == STEREO_IMAGE_FILE && (
                (!static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getLastCommissionedFrameNumber() == simg.frameNumber) ||
                (static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getNumImagesTotal()-1 == simg.frameNumber)
        ) ) ) {

        drawTimer.start();
        const CameraImage &mimg = simg;
        mimg.img = simg.img.clone();
        mimg.imgS = simg.imgS.clone();

        std::vector<cv::Rect> ROIs;
        if(useROIPreProcessing) {
            ROIs.push_back(ROIstereoImageOnePupilM);
            ROIs.push_back(ROIstereoImageOnePupilS);
        } else {
            ROIs.push_back(roiM);
            ROIs.push_back(roiS);
            //ROIs.push_back(cv::Rect(0,0, bwFrameM.size().width, bwFrameM.size().height));
            //ROIs.push_back(cv::Rect(0,0, bwFrameS.size().width, bwFrameS.size().height));
        }
        emit processedImageLowFPS(mimg, currentProcMode, ROIs, Pupils);

        // to inform imagePlaybackControlDialog about the just processed image->
        if(camera->getType() == STEREO_IMAGE_FILE)
                emit processedImageLowFPS(mimg);

        emit processedPupilDataLowFPS(simg.timestamp, currentProcMode, Pupils);
    }

    emit processedPupilData(simg.timestamp, currentProcMode, Pupils);
}
// Slot callback for receiving new stereo camera images, associated with two viewpoints, both looking at both eyes
// Performs the processing/pupil detection
// Emits the pupil detection result as a signal, as well as processed images with plotted pupil contours
// Depending on the configuration, performs undistortion on the pupil detections
void PupilDetection::onNewStereoImageForTwoPupil(const CameraImage &simg) {

    // Can sometimes weirdly happen, when closing camera. TODO
    if(!camera)
        return;

    if (synchronised) {
        const QMutexLocker locker(imageMutex);
        onNewStereoImageForTwoPupilImpl(simg);
//        qDebug() << "pupilDetection locking";
        //      qDebug() << "pupilDetection image processed, unlocking";
        imagePublished->wakeAll();
        if (trackingOn) {
//            qDebug() << "Locking image processing";
            imageProcessed->wait(imageMutex);
        }
    }
    else {
        onNewStereoImageForTwoPupilImpl(simg);
    }
}

void PupilDetection::onNewStereoImageForTwoPupilImpl(const CameraImage &simg) {

    // at the moment, the images are not undistorted completely but only the major axis points are undistorted after detection for absolute unit conversion
    // This creates a discrepancy between the undistorted pixel size and the physical measure, as a fix, undistortedDiamter can be calculated using useUndistort

    if (!trackingOn) {
//        qDebug() << simg.frameNumber;
        // TODO: also emit one in case the file camera was PAUSED !
        if ((drawTimer.elapsed() > drawDelay) ||
            (camera->getType() == STEREO_IMAGE_FILE && (
                    (!static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getLastCommissionedFrameNumber() == simg.frameNumber) ||
                    (static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getNumImagesTotal()-1 == simg.frameNumber)
            ) ) ) {
            emit processedImageLowFPS(simg);
        }
        return;
    }

    cv::Mat bwFrameRM = simg.img;
    cv::Mat bwFrameRS = simg.imgS;
    cv::Mat bwFrameLM = simg.img;
    cv::Mat bwFrameLS = simg.imgS;
    cv::Rect roiRM = cv::Rect(0, 0, simg.img.cols, simg.img.rows);
    cv::Rect roiRS = cv::Rect(0, 0, simg.img.cols, simg.img.rows);
    cv::Rect roiLM = cv::Rect(0, 0, simg.img.cols, simg.img.rows);
    cv::Rect roiLS = cv::Rect(0, 0, simg.img.cols, simg.img.rows);

    if(useROIPreProcessing && !ROIstereoImageTwoPupilRM.empty() && roiRM != ROIstereoImageTwoPupilRM && ROIstereoImageTwoPupilRM.width <= bwFrameRM.cols && ROIstereoImageTwoPupilRM.height <= bwFrameRM.rows) {
        roiRM = ROIstereoImageTwoPupilRM;
        bwFrameRM = bwFrameRM(ROIstereoImageTwoPupilRM);
    } else if(autoParamEnabled && autoParamScheduled)
        ROIstereoImageTwoPupilRM = roiRM;

    if(useROIPreProcessing && !ROIstereoImageTwoPupilRS.empty() && roiRS != ROIstereoImageTwoPupilRS && ROIstereoImageTwoPupilRS.width <= bwFrameRS.cols && ROIstereoImageTwoPupilRS.height <= bwFrameRS.rows) {
        roiRS = ROIstereoImageTwoPupilRS;
        bwFrameRS = bwFrameRS(ROIstereoImageTwoPupilRS);
    } else if(autoParamEnabled && autoParamScheduled)
        ROIstereoImageTwoPupilRS = roiRS;

    if(useROIPreProcessing && !ROIstereoImageTwoPupilLM.empty() && roiLM != ROIstereoImageTwoPupilLM && ROIstereoImageTwoPupilLM.width <= bwFrameLM.cols && ROIstereoImageTwoPupilLM.height <= bwFrameLM.rows) {
        roiLM = ROIstereoImageTwoPupilLM;
        bwFrameLM = bwFrameLM(ROIstereoImageTwoPupilLM);
    } else if(autoParamEnabled && autoParamScheduled)
        ROIstereoImageTwoPupilLM = roiLM;

    if(useROIPreProcessing && !ROIstereoImageTwoPupilLS.empty() && roiLS != ROIstereoImageTwoPupilLS && ROIstereoImageTwoPupilLS.width <= bwFrameLS.cols && ROIstereoImageTwoPupilLS.height <= bwFrameLS.rows) {
        roiLS = ROIstereoImageTwoPupilLS;
        bwFrameLS = bwFrameLS(ROIstereoImageTwoPupilLS);
    } else if(autoParamEnabled && autoParamScheduled)
        ROIstereoImageTwoPupilLS = roiLS;

    if(autoParamEnabled && autoParamScheduled) {
        performAutoParam();
        autoParamScheduled = false;
    }

    if (bwFrameRM.channels() > 1) {
        cv::cvtColor(bwFrameRM, bwFrameRM, cv::COLOR_BGR2GRAY);
        cv::cvtColor(bwFrameLM, bwFrameLM, cv::COLOR_BGR2GRAY);
    }
    if (bwFrameRS.channels() > 1) {
        cv::cvtColor(bwFrameRS, bwFrameRS, cv::COLOR_BGR2GRAY);
        cv::cvtColor(bwFrameLS, bwFrameLS, cv::COLOR_BGR2GRAY);
    }

    // We execute pupil detection for main and secondary images concurrently using treads, we execute both in separate threads, then wait till both are finished
    QFutureSynchronizer<Pupil> synchronizer;
    Pupil pupilRM;
    Pupil pupilRS;
    Pupil pupilLM;
    Pupil pupilLS;

    try {
        //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        if(useOutlineConfidence) {

            // Qt6
            auto pdm1 = pupilDetectionMethods1[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm1, bwFrameRM] { return pdm1->runWithConfidence(bwFrameRM); }) );
            auto pdm2 = pupilDetectionMethods2[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm2, bwFrameRS] { return pdm2->runWithConfidence(bwFrameRS); }) );
            auto pdm3 = pupilDetectionMethods3[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm3, bwFrameLM] { return pdm3->runWithConfidence(bwFrameLM); }) );
            auto pdm4 = pupilDetectionMethods4[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm4, bwFrameLS] { return pdm4->runWithConfidence(bwFrameLS); }) );

            // Qt5
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods1[pupilDetectionIndex], &PupilDetectionMethod::runWithConfidence, bwFrameRM));
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods2[pupilDetectionIndex], &PupilDetectionMethod::runWithConfidence, bwFrameRS));
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods3[pupilDetectionIndex], &PupilDetectionMethod::runWithConfidence, bwFrameLM));
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods4[pupilDetectionIndex], &PupilDetectionMethod::runWithConfidence, bwFrameLS));
        } else {

            // Qt6
            auto pdm1 = pupilDetectionMethods1[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm1, bwFrameRM] { return pdm1->run(bwFrameRM); }) );
            auto pdm2 = pupilDetectionMethods2[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm2, bwFrameRS] { return pdm2->run(bwFrameRS); }) );
            auto pdm3 = pupilDetectionMethods3[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm3, bwFrameLM] { return pdm3->run(bwFrameLM); }) );
            auto pdm4 = pupilDetectionMethods4[pupilDetectionIndex];
            synchronizer.addFuture( QtConcurrent::run([pdm4, bwFrameLS] { return pdm4->run(bwFrameLS); }) );

            // Qt5
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods1[pupilDetectionIndex], &PupilDetectionMethod::run, bwFrameRM));
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods2[pupilDetectionIndex], &PupilDetectionMethod::run, bwFrameRS));
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods3[pupilDetectionIndex], &PupilDetectionMethod::run, bwFrameLM));
            //synchronizer.addFuture(QtConcurrent::run(pupilDetectionMethods4[pupilDetectionIndex], &PupilDetectionMethod::run, bwFrameLS));
        }
        synchronizer.waitForFinished();
        // Unhandled exceptions in the QtConcurrent::run function are thrown at the result() call
        pupilRM = synchronizer.futures().at(0).result();
        pupilRS = synchronizer.futures().at(1).result();
        pupilLM = synchronizer.futures().at(2).result();
        pupilLS = synchronizer.futures().at(3).result();
    } catch (...) {
        pupilRM.clear();
        pupilRS.clear();
        pupilLM.clear();
        pupilLS.clear();
    }
    //runtimeHistory.push_back(std::make_pair(simg->timestamp, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()));

    // Shift the pupil position back to the original image coordinates instead of ROI
    if(useROIPreProcessing) {
        pupilRM.shift(roiRM.tl());
        pupilRS.shift(roiRS.tl());
        pupilLM.shift(roiLM.tl());
        pupilLS.shift(roiLS.tl());
    }

    if(usePupilUndistort && !useImageUndistort) {
        //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        std::pair<double, double> diameters1 = stereoCalibration->undistortPupilDiameters(pupilRM, pupilLM);
        pupilRM.undistortedDiameter = diameters1.first;
        pupilLM.undistortedDiameter = diameters1.second;
        std::pair<double, double> diameters2 = stereoCalibration->undistortPupilDiameters(pupilRS, pupilLS);
        pupilRS.undistortedDiameter = diameters2.first;
        pupilLS.undistortedDiameter = diameters2.second;
        //qDebug()<< std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count() / 1000.0 ;
    } else if(!usePupilUndistort && useImageUndistort) {
        pupilRM.undistortedDiameter = pupilRM.diameter();
        pupilRS.undistortedDiameter = pupilRM.diameter();
        pupilLM.undistortedDiameter = pupilLM.diameter();
        pupilLS.undistortedDiameter = pupilLM.diameter();
    }

    pupilRM.algorithmName = pupilDetectionMethods1[pupilDetectionIndex]->title();
    pupilRS.algorithmName = pupilRM.algorithmName;
    pupilLM.algorithmName = pupilDetectionMethods1[pupilDetectionIndex]->title();
    pupilLS.algorithmName = pupilLM.algorithmName;

    // Eye A
    // If both pupil detections are valid and the camera is calibrated, we can perform unit conversion to absolute measure
    if(pupilRM.valid(-2.0) && pupilRS.valid(-2.0) && calibrated) {
        //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

        Pupil rotPupilRM(pupilRM);
        rotPupilRM.angle += 360 - rotPupilRM.angle;
        Pupil rotPupilRS(pupilRS);
        rotPupilRS.angle += 360 - rotPupilRS.angle;

        // convert pupil detection from pixel into mm through stereo calibration
        // Select the top left and top right corner of the bounding rects of the pupils as the points we triangulate
        cv::Point2f pointsArrRM[4]; // rotatedRect points in order: bottomLeft, topLeft, topRight, bottomRight
        rotPupilRM.points(pointsArrRM);
        int secondPointR = rotPupilRM.size.width > rotPupilRM.size.height ? 2 : 0; // if the pupil major axis is horizontal use topLeft and topRight, else topLeft and bottomLeft
        //cv::line(mimg->img,mainPointsArr[1], mainPointsArr[secondPoint], cv::Scalar(255, 0, 0));

        cv::Point2f pointsArrRS[4];
        rotPupilRS.points(pointsArrRS);
        //cv::line(mimg->imgS,secondaryPointsArr[1], secondaryPointsArr[secondPoint], cv::Scalar(255, 0, 0));

        std::vector<cv::Point2f> pointsRM{pointsArrRM[1], pointsArrRM[secondPointR]}, pointsRS{pointsArrRS[1], pointsArrRS[secondPointR]};
        std::vector<cv::Point3f> worldPointsR = stereoCalibration->convertPointsTo3D(pointsRM, pointsRS);

        pupilRM.physicalDiameter = static_cast<float>(cv::norm(worldPointsR[0] - worldPointsR[1]));
        pupilRS.physicalDiameter = pupilRM.physicalDiameter;
        //runtimeHistory.push_back(std::make_pair(simg->timestamp, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()));
    }

    // Eye B
    // If both pupil detections are valid and the camera is calibrated, we can perform unit conversion to absolute measure
    if(pupilLM.valid(-2.0) && pupilLS.valid(-2.0) && calibrated) {
        //std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

        Pupil rotPupilLM(pupilLM);
        rotPupilLM.angle += 360 - rotPupilLM.angle;
        Pupil rotPupilLS(pupilLS);
        rotPupilLS.angle += 360 - rotPupilLS.angle;

        // convert pupil detection from pixel into mm through stereo calibration
        // Select the top left and top right corner of the bounding rects of the pupils as the points we triangulate
        cv::Point2f pointsArrLM[4]; // rotatedRect points in order: bottomLeft, topLeft, topRight, bottomRight
        rotPupilLM.points(pointsArrLM);
        int secondPointL = rotPupilLM.size.width > rotPupilLM.size.height ? 2 : 0; // if the pupil major axis is horizontal use topLeft and topRight, else topLeft and bottomLeft
        //cv::line(mimg->img,mainPointsArr[1], mainPointsArr[secondPoint], cv::Scalar(255, 0, 0));

        cv::Point2f pointsArrLS[4];
        rotPupilLS.points(pointsArrLS);
        //cv::line(mimg->imgS,secondaryPointsArr[1], secondaryPointsArr[secondPoint], cv::Scalar(255, 0, 0));

        std::vector<cv::Point2f> pointsLM{pointsArrLM[1], pointsArrLM[secondPointL]}, pointsLS{pointsArrLS[1], pointsArrLS[secondPointL]};
        std::vector<cv::Point3f> worldPointsL = stereoCalibration->convertPointsTo3D(pointsLM, pointsLS);

        pupilLM.physicalDiameter = static_cast<float>(cv::norm(worldPointsL[0] - worldPointsL[1]));
        pupilLS.physicalDiameter = pupilLM.physicalDiameter;
        //runtimeHistory.push_back(std::make_pair(simg->timestamp, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()));
    }

    if(computeBRISQUEEnabled) {
        pupilRM.BRISQUEFullImage = brisque->compute(simg.img)[0];
        pupilRM.BRISQUEPDROI = brisque->compute(bwFrameRM)[0];
        pupilRS.BRISQUEFullImage = brisque->compute(simg.imgS)[0];
        pupilRS.BRISQUEPDROI = brisque->compute(bwFrameRS)[0];
        pupilLM.BRISQUEFullImage = brisque->compute(simg.img)[0];
        pupilLM.BRISQUEPDROI = brisque->compute(bwFrameLM)[0];
        pupilLS.BRISQUEFullImage = brisque->compute(simg.imgS)[0];
        pupilLS.BRISQUEPDROI = brisque->compute(bwFrameLS)[0];
        //pupil.BRISQUEPDInternal = ...;
        //std::cout << "BRISQUE SCORE: " << score[0] << std::endl;
    }

    std::vector<Pupil> Pupils;
    Pupils.push_back(pupilRM);
    Pupils.push_back(pupilRS);
    Pupils.push_back(pupilLM);
    Pupils.push_back(pupilLS);

    // NOTE: It is important to not only check for drawDelay, but care for the special case,
    // when the last signals from an image playback arrive before another drawDelay is happened,
    // to not make the playback stuck just near the end. That is why we check for frame number too
    if ((drawTimer.elapsed() > drawDelay) ||
        (camera->getType() == STEREO_IMAGE_FILE && (
                (!static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getLastCommissionedFrameNumber() == simg.frameNumber) ||
                (static_cast<FileCamera*>(camera)->isPlaying() && static_cast<FileCamera*>(camera)->getNumImagesTotal()-1 == simg.frameNumber)
        ) ) ) {

        drawTimer.start();
        const CameraImage &mimg = simg;
        mimg.img = simg.img.clone();
        mimg.imgS = simg.imgS.clone();

        std::vector<cv::Rect> ROIs;
        if(useROIPreProcessing) {
            ROIs.push_back(ROIstereoImageTwoPupilRM);
            ROIs.push_back(ROIstereoImageTwoPupilRS);
            ROIs.push_back(ROIstereoImageTwoPupilLM);
            ROIs.push_back(ROIstereoImageTwoPupilLS);
        } else {
            ROIs.push_back(roiRM);
            ROIs.push_back(roiRS);
            ROIs.push_back(roiLM);
            ROIs.push_back(roiLS);

        }
        emit processedImageLowFPS(mimg, currentProcMode, ROIs, Pupils);

        // to inform imagePlaybackControlDialog about the just processed image
        if(camera->getType() == STEREO_IMAGE_FILE)
                emit processedImageLowFPS(mimg);

        emit processedPupilDataLowFPS(simg.timestamp, currentProcMode, Pupils);
    }

    emit processedPupilData(simg.timestamp, currentProcMode, Pupils);
}

// GB: I found this function like this, and did not bother it
template <typename T> void PupilDetection::writeVectorCSV(std::vector<std::pair<uint64_t , T>> data, const std::string &header, const std::string &filename) {
    // Debug helper function
    std::ofstream file(filename);

    if(file.is_open()) {

        file << header ;

        for(typename std::vector<std::pair<uint64_t , T>>::iterator it = data.begin(); it != data.end(); ++it) {
            file << std::get<0>((*it)) << "," << std::get<1>((*it)) ;
        }

        file.close();
    } else {
        std::cerr<<"Failed to open file : "<<filename;
    }
}

// When the config changes, emit a signal to inform others of the current settings config i.e. subject configuration
void PupilDetection::setConfigLabel(QString config) {
    currentConfigLabel = config;
        if (config =="Automatic Parametrization")
        setAutoParamSettingsEnabled(true);
    else
        setAutoParamSettingsEnabled(false);

    emit configChanged(config);
}

// TODO: use only this everywhere, and remove mainwondow's trackingOn bool
bool PupilDetection::isTrackingOn() {
    return trackingOn;
}

QRect PupilDetection::getROIsingleImageOnePupil() {
    return QRect(ROIsingleImageOnePupil.x, ROIsingleImageOnePupil.y, ROIsingleImageOnePupil.width, ROIsingleImageOnePupil.height);
}
QRect PupilDetection::getROIsingleImageTwoPupilR() {
    return QRect(ROIsingleImageTwoPupilR.x, ROIsingleImageTwoPupilR.y, ROIsingleImageTwoPupilR.width, ROIsingleImageTwoPupilR.height);
}
QRect PupilDetection::getROIsingleImageTwoPupilL() {
    return QRect(ROIsingleImageTwoPupilL.x, ROIsingleImageTwoPupilL.y, ROIsingleImageTwoPupilL.width, ROIsingleImageTwoPupilL.height);
}
QRect PupilDetection::getROIstereoImageOnePupilM() {
    return QRect(ROIstereoImageOnePupilM.x, ROIstereoImageOnePupilM.y, ROIstereoImageOnePupilM.width, ROIstereoImageOnePupilM.height);
}
QRect PupilDetection::getROIstereoImageOnePupilS() {
    return QRect(ROIstereoImageOnePupilS.x, ROIstereoImageOnePupilS.y, ROIstereoImageOnePupilS.width, ROIstereoImageOnePupilS.height);
}
QRect PupilDetection::getROIstereoImageTwoPupilRM() {
    return QRect(ROIstereoImageTwoPupilRM.x, ROIstereoImageTwoPupilRM.y, ROIstereoImageTwoPupilRM.width, ROIstereoImageTwoPupilRM.height);
}
QRect PupilDetection::getROIstereoImageTwoPupilRS() {
    return QRect(ROIstereoImageTwoPupilRS.x, ROIstereoImageTwoPupilRS.y, ROIstereoImageTwoPupilRS.width, ROIstereoImageTwoPupilRS.height);
}
QRect PupilDetection::getROIstereoImageTwoPupilLM() {
    return QRect(ROIstereoImageTwoPupilLM.x, ROIstereoImageTwoPupilLM.y, ROIstereoImageTwoPupilLM.width, ROIstereoImageTwoPupilLM.height);
}
QRect PupilDetection::getROIstereoImageTwoPupilLS() {
    return QRect(ROIstereoImageTwoPupilLS.x, ROIstereoImageTwoPupilLS.y, ROIstereoImageTwoPupilLS.width, ROIstereoImageTwoPupilLS.height);
}
QRect PupilDetection::getROImirrImageOnePupil1() {
    return QRect(ROImirrImageOnePupil1.x, ROImirrImageOnePupil1.y, ROImirrImageOnePupil1.width, ROImirrImageOnePupil1.height);
}
QRect PupilDetection::getROImirrImageOnePupil2() {
    return QRect(ROImirrImageOnePupil2.x, ROImirrImageOnePupil2.y, ROImirrImageOnePupil2.width, ROImirrImageOnePupil2.height);
}

void PupilDetection::setROIsingleImageOnePupil(QRectF roi) {
    if(!roi.isEmpty())
        ROIsingleImageOnePupil = cv::Rect(static_cast<int>(roi.topLeft().x()), static_cast<int>(roi.topLeft().y()), static_cast<int>(roi.width()), static_cast<int>(roi.height()));
}
void PupilDetection::setROIsingleImageTwoPupilR(QRectF roi) {
    if(!roi.isEmpty())
        ROIsingleImageTwoPupilR = cv::Rect(static_cast<int>(roi.topLeft().x()), static_cast<int>(roi.topLeft().y()), static_cast<int>(roi.width()), static_cast<int>(roi.height()));
}
void PupilDetection::setROIsingleImageTwoPupilL(QRectF roi) {
    if(!roi.isEmpty())
        ROIsingleImageTwoPupilL = cv::Rect(static_cast<int>(roi.topLeft().x()), static_cast<int>(roi.topLeft().y()), static_cast<int>(roi.width()), static_cast<int>(roi.height()));
}
void PupilDetection::setROIstereoImageOnePupilM(QRectF roi) {
    if(!roi.isEmpty())
        ROIstereoImageOnePupilM = cv::Rect(static_cast<int>(roi.topLeft().x()), static_cast<int>(roi.topLeft().y()), static_cast<int>(roi.width()), static_cast<int>(roi.height()));
}
void PupilDetection::setROIstereoImageOnePupilS(QRectF roi) {
    if(!roi.isEmpty())
        ROIstereoImageOnePupilS = cv::Rect(static_cast<int>(roi.topLeft().x()), static_cast<int>(roi.topLeft().y()), static_cast<int>(roi.width()), static_cast<int>(roi.height()));
}
void PupilDetection::setROIstereoImageTwoPupilRM(QRectF roi) {
    if(!roi.isEmpty())
        ROIstereoImageTwoPupilRM = cv::Rect(static_cast<int>(roi.topLeft().x()), static_cast<int>(roi.topLeft().y()), static_cast<int>(roi.width()), static_cast<int>(roi.height()));
}
void PupilDetection::setROIstereoImageTwoPupilRS(QRectF roi) {
    if(!roi.isEmpty())
        ROIstereoImageTwoPupilRS = cv::Rect(static_cast<int>(roi.topLeft().x()), static_cast<int>(roi.topLeft().y()), static_cast<int>(roi.width()), static_cast<int>(roi.height()));
}
void PupilDetection::setROIstereoImageTwoPupilLM(QRectF roi) {
    if(!roi.isEmpty())
        ROIstereoImageTwoPupilLM = cv::Rect(static_cast<int>(roi.topLeft().x()), static_cast<int>(roi.topLeft().y()), static_cast<int>(roi.width()), static_cast<int>(roi.height()));
}
void PupilDetection::setROIstereoImageTwoPupilLS(QRectF roi) {
    if(!roi.isEmpty())
        ROIstereoImageTwoPupilLS = cv::Rect(static_cast<int>(roi.topLeft().x()), static_cast<int>(roi.topLeft().y()), static_cast<int>(roi.width()), static_cast<int>(roi.height()));
}
void PupilDetection::setROImirrImageOnePupil1(QRectF roi) {
    if(!roi.isEmpty())
        ROImirrImageOnePupil1 = cv::Rect(static_cast<int>(roi.topLeft().x()), static_cast<int>(roi.topLeft().y()), static_cast<int>(roi.width()), static_cast<int>(roi.height()));
}
void PupilDetection::setROImirrImageOnePupil2(QRectF roi) {
    if(!roi.isEmpty())
        ROImirrImageOnePupil2 = cv::Rect(static_cast<int>(roi.topLeft().x()), static_cast<int>(roi.topLeft().y()), static_cast<int>(roi.width()), static_cast<int>(roi.height()));
}

ProcMode PupilDetection::getCurrentProcMode() {
    return (ProcMode)currentProcMode;
}

void PupilDetection::setCurrentProcMode(int val) {

    configureCameraConnection(true);

    currentProcMode = (ProcMode)val;

    configureCameraConnection(false);
}

void PupilDetection::setAutoParamEnabled(bool state) {
    autoParamEnabled = state;

    // GB TODO: do update here onse more for sure?
}

bool PupilDetection::isAutoParamSettingsEnabled(){
    return autoParamSettingsEnabled;
}

void PupilDetection::setAutoParamSettingsEnabled(bool enabled){
    autoParamSettingsEnabled = enabled;
}

void PupilDetection::setAutoParamPupSizePercent(float value) {
    autoParamPupSizePercent = value;
}

float PupilDetection::getAutoParamPupSizePercent() {
    return autoParamPupSizePercent;
}

void PupilDetection::setAutoParamScheduled(bool state) {
    autoParamScheduled = state;
}

void PupilDetection::performAutoParam() {

    qDebug() << "autoParamEnabled = " << autoParamEnabled;

    if(!autoParamEnabled)
        return;

    std::vector<PupilDetectionMethod*> algInstances;

    std::vector<cv::Rect> rois;

    switch(currentProcMode) {
        case SINGLE_IMAGE_ONE_PUPIL:
            algInstances.push_back(getCurrentMethod1());
            rois.push_back(ROIsingleImageOnePupil);
            break;
        case SINGLE_IMAGE_TWO_PUPIL:
            algInstances.push_back(getCurrentMethod1());
            algInstances.push_back(getCurrentMethod2());
            rois.push_back(ROIsingleImageTwoPupilR);
            rois.push_back(ROIsingleImageTwoPupilL);
            break;
        case STEREO_IMAGE_ONE_PUPIL:
            algInstances.push_back(getCurrentMethod1());
            algInstances.push_back(getCurrentMethod2());
            rois.push_back(ROIstereoImageOnePupilM);
            rois.push_back(ROIstereoImageOnePupilS);
            break;
        case STEREO_IMAGE_TWO_PUPIL:
            algInstances.push_back(getCurrentMethod1());
            algInstances.push_back(getCurrentMethod2());
            algInstances.push_back(getCurrentMethod3());
            algInstances.push_back(getCurrentMethod4());
            rois.push_back(ROIstereoImageTwoPupilRM);
            rois.push_back(ROIstereoImageTwoPupilRS);
            rois.push_back(ROIstereoImageTwoPupilLM);
            rois.push_back(ROIstereoImageTwoPupilLS);
            break;
        default:
            return;
    }

    // useROIPreProcessing &&  (( NEMJÓ, MINDENKÉPP KELL LÉTEZŐ ROI))
    if((rois[0].width == 0 || rois[0].height == 0))
        return;

    // min 2 and max 8 mm means that the minimum is 1/4th of 8,
    // or in other words: 2 = 0.25 *8;
    float minToMaxDia = 0.25f;

    // NOTE: These are only DIAMETER values!
    float pupSizeFactorMin = (autoParamPupSizePercent/100.0f *minToMaxDia);
    if(pupSizeFactorMin < 0.01f)
        pupSizeFactorMin = 0.01f;
    float pupSizeFactorMax = (autoParamPupSizePercent/100.0f);

    for(size_t c=0; c<algInstances.size(); c++) {
        // for each algorithm instance, we perform the pupilDetectionMethod-type-specific automatic parametrization

        //std::string algName = pupilDetectionMethods1[pupilDetectionIndex]->title();
        float roiWidth = static_cast<float>( rois[c].width );
        float roiHeight = static_cast<float>( rois[c].height );

        float minDim = (roiWidth<=roiHeight) ? roiWidth : roiHeight;
        // bool isWidthTheMinDim = (roiWidth<=roiHeight) ? true : false;
        // bool isWidthTheMinDim = (roiWidth<=roiHeight) ? true : false;

        // now we get the RADIUS values
        float minRadius = pupSizeFactorMin*minDim /2.0f;
        float maxRadius = pupSizeFactorMax*minDim /2.0f;

        //qDebug() << "roiWidth = " << roiWidth;
        //qDebug() << "roiHeight =" << roiHeight;
        //qDebug() << "minDim =" << minDim;
        //qDebug() << "minRadius = " << minRadius;
        //qDebug() << "maxRadius =" << maxRadius;
        
        if(pupilDetectionIndex == 0) {
            // ELSE
            ElSe *alg = dynamic_cast<ElSe*>(algInstances[c]);

            // Note: Empirical correction
            maxRadius *= 1.1;

            alg->minAreaRatio = static_cast<float>( minRadius*minRadius*M_PI / (roiWidth*roiHeight) );
            alg->maxAreaRatio = static_cast<float>( maxRadius*maxRadius*M_PI / (roiWidth*roiHeight) );

            qDebug() << "Set AutoParam for algorithm ElSe, instance " << c;
            qDebug() << "minAreaRatio =" << alg->minAreaRatio;
            qDebug() << "maxAreaRatio =" << alg->maxAreaRatio;
                
        } else if(pupilDetectionIndex == 1) {
            // EXCUSE
            ExCuSe *alg = dynamic_cast<ExCuSe*>(algInstances[c]);

            alg->max_ellipse_radi = static_cast<int>(round(maxRadius));

            qDebug() << "Set AutoParam for algorithm ExCuSe, instance " << c;
            qDebug() << "max_ellipse_radi =" << alg->max_ellipse_radi;

        } if(pupilDetectionIndex == 2) {
            // PURE
            PuRe *alg = dynamic_cast<PuRe*>(algInstances[c]);

            // These are just a relative reference. User cannot set these to keep them constant
            // Anyway I guess a camera calibration that supports a precise px-to-mm mapping, could also be utilized here
            // Now we just back-calculate the roi width (= inter-canthi distance)

            alg->meanCanthiDistanceMM = (roiWidth / 2.0f) / maxRadius *8.0f; // THIS /2.0f is important. Image width is HALF of the canthi distance
            alg->maxPupilDiameterMM = 8.0f;
            alg->minPupilDiameterMM = 8.0f * minToMaxDia;

            qDebug() << "Set AutoParam for algorithm PuRe, instance " << c;
            qDebug() << "meanCanthiDistanceMM =" << alg->meanCanthiDistanceMM;
            qDebug() << "maxPupilDiameterMM =" << alg->maxPupilDiameterMM;
            qDebug() << "minPupilDiameterMM =" << alg->minPupilDiameterMM;

        } if(pupilDetectionIndex == 3) {
            // PUREST
            PuReST *alg = dynamic_cast<PuReST*>(algInstances[c]);

            alg->meanCanthiDistanceMM = (roiWidth / 2.0f) / maxRadius *8.0f; // THIS /2.0f is important. Image width is HALF of the canthi distance
            alg->maxPupilDiameterMM = 8.0f;
            alg->minPupilDiameterMM = 8.0f * minToMaxDia; //2.0f;

            qDebug() << "Set AutoParam for algorithm PuReSt, instance " << c;
            qDebug() << "meanCanthiDistanceMM =" << alg->meanCanthiDistanceMM;
            qDebug() << "maxPupilDiameterMM =" << alg->maxPupilDiameterMM;
            qDebug() << "minPupilDiameterMM =" << alg->minPupilDiameterMM;

        } if(pupilDetectionIndex == 4) {
            // STARBURST
            Starburst *alg = dynamic_cast<Starburst*>(algInstances[c]);

            //threshold of pupil edge points detection
            alg->edge_threshold = 18;
            // approx max size of the reflection relative to image height -> height/this
            alg->corneal_reflection_ratio_to_image_size = static_cast<int>( roiHeight / round((maxRadius* 0.2f)) );
            //corneal reflection search window size
            alg->crWindowSize = static_cast<int>( round(maxRadius * 2.0f) );

            qDebug() << "Set AutoParam for algorithm Starburst, instance " << c;
            qDebug() << "set now: edge_threshold =" << alg->edge_threshold;
            qDebug() << "set now: corneal_reflection_ratio_to_image_size =" << alg->corneal_reflection_ratio_to_image_size;
            qDebug() << "set now: crWindowSize =" << alg->crWindowSize;

        } if(pupilDetectionIndex == 5) {
            // SWIRSKI2D
            Swirski2D *alg = dynamic_cast<Swirski2D*>(algInstances[c]);

            alg->params.Radius_Min = static_cast<int>( round(minRadius) );
            alg->params.Radius_Max = static_cast<int>( round(maxRadius) );

            qDebug() << "Set AutoParam for algorithm Swirski2D, instance " << c;
            qDebug() << "set now: params.Radius_Min =" << alg->params.Radius_Min;
            qDebug() << "set now: params.Radius_Max =" << alg->params.Radius_Max;

        }

    }
}

void PupilDetection::setSynchronised(bool synchronised) {
    PupilDetection::synchronised = synchronised;
}

void PupilDetection::configureCameraConnection(bool connectOrDisconnect) {
    if(!camera)
        return;

    if(!connectOrDisconnect) {
        disconnect(camera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(onNewSingleImageForOnePupil(CameraImage)));
        disconnect(camera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(onNewSingleImageForTwoPupil(CameraImage)));
        disconnect(camera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(onNewStereoImageForOnePupil(CameraImage)));
        disconnect(camera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(onNewStereoImageForTwoPupil(CameraImage)));
//        disconnect(camera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(onNewMirrImageForOnePupil(CameraImage)));
    } else {
        if (currentProcMode == ProcMode::SINGLE_IMAGE_ONE_PUPIL) {
            connect(camera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(onNewSingleImageForOnePupil(CameraImage)));
        } else if (currentProcMode == ProcMode::SINGLE_IMAGE_TWO_PUPIL) {
            connect(camera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(onNewSingleImageForTwoPupil(CameraImage)));
        } else if (currentProcMode == ProcMode::STEREO_IMAGE_ONE_PUPIL) {
            connect(camera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(onNewStereoImageForOnePupil(CameraImage)));
        } else if (currentProcMode == ProcMode::STEREO_IMAGE_TWO_PUPIL) {
            connect(camera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(onNewStereoImageForTwoPupil(CameraImage)));
            // } else if(currentProcMode == ProcMode::MIRR_IMAGE_ONE_PUPIL) {
            //     connect(camera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(onNewMirrImageForOnePupil(CameraImage)));
        } else {
            qDebug() << "Could not determine pupilDetection proc mode or it is still undetermined";
        }
    }
}