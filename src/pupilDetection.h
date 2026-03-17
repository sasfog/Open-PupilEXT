#pragma once

/**
    @author Moritz Lode, Gabor Benyei, Attila Boncser
*/

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QRect>
#include "devices/camera.h"
#include "pupil-detection-methods/PupilDetectionMethod.h"
#include "devices/singleCamera.h"
#include "stereoCameraCalibration.h"
#include "devices/singleWebcam.h"

#include <opencv2/quality/qualitybrisque.hpp>

Q_DECLARE_METATYPE(Pupil)
Q_DECLARE_METATYPE(cv::Rect)
Q_DECLARE_METATYPE(std::vector<Pupil>)
Q_DECLARE_METATYPE(std::vector<cv::Rect>)

/**
    Enum for different camera image processing modes. These require separate ROI configurations
*/
enum ProcMode {
    UNDETERMINED = 0,
    SINGLE_IMAGE_ONE_PUPIL = 1,
    SINGLE_IMAGE_TWO_PUPIL = 2,
    STEREO_IMAGE_ONE_PUPIL = 3,
    STEREO_IMAGE_TWO_PUPIL = 4 //,
    // MIRR_IMAGE_ONE_PUPIL = 3
};


enum PupilVecIdx {
    SINGLE_IMAGE_ONE_PUPIL_MAIN = 0,
    SINGLE_IMAGE_TWO_PUPIL_R = 0,
    SINGLE_IMAGE_TWO_PUPIL_L = 1,
    STEREO_IMAGE_ONE_PUPIL_MAIN = 0,
    STEREO_IMAGE_ONE_PUPIL_SEC = 1,
    STEREO_IMAGE_TWO_PUPIL_R_MAIN = 0,
    STEREO_IMAGE_TWO_PUPIL_R_SEC = 1,
    STEREO_IMAGE_TWO_PUPIL_L_MAIN = 2,
    STEREO_IMAGE_TWO_PUPIL_L_SEC = 3,
    MIRR_IMAGE_ONE_PUPIL_MAIN = 0,
    MIRR_IMAGE_ONE_PUPIL_SEC = 1
};


class PupilDetection : public QObject {
    Q_OBJECT

public:

    explicit PupilDetection(QMutex *imageMutex, QWaitCondition *imagePublished, QWaitCondition *imageProcessed, QObject *parent = 0);
    ~PupilDetection() override;

    std::vector<PupilDetectionMethod*> getMethods() {
        return pupilDetectionMethods1;
    }

    QString getCurrentConfigLabel() {
        return currentConfigLabel;
    }

    void setCurrentConfigLabel(QString config) {
        currentConfigLabel = config;
    }

    // GB: getCurrentMethod() and its stereo pair were refactored and declaration moved a few lines down

    bool isOutlineConfidenceEnabled() {
        return useOutlineConfidence;
    }

    bool isROIPreProcessingEnabled() {
        return useROIPreProcessing;
    }

    void enableOutlineConfidence(bool value) {
        useOutlineConfidence = value;
    }

    bool isComputeBRISQUEEnabled() {
        return computeBRISQUEEnabled;
    }

    void enableROIPreProcessing(bool value) {
        useROIPreProcessing = value;

        // in order to inform any existing videoView that ROI is not used for pupil detection
        emit onROIPreprocessingChanged(value);
    }

    bool isPupilUndistortionEnabled() {
        return usePupilUndistort;
    }

    void enablePupilUndistortion(bool value) {
        usePupilUndistort = value;
    }

    bool isImageUndistortionEnabled() {
        return useImageUndistort;
    }

    void enableImageUndistortion(bool value) {
        useImageUndistort = value;
    }

    void setCamera(Camera *m_camera);

    bool hasCamera() {
        return camera != nullptr;
    }

    // all maximum of 4 threads, using 4 different pupilDetectionMethods variables thread-safely
    PupilDetectionMethod* getCurrentMethod1() {
        return pupilDetectionMethods1[pupilDetectionIndex];
    }
    PupilDetectionMethod* getMethod1(std::string method) {
        for(auto pm: pupilDetectionMethods1) {
            if(pm->title() == method)
                return pm;
        }
        return nullptr;
    }
    //
    PupilDetectionMethod* getCurrentMethod2() {
        return pupilDetectionMethods2[pupilDetectionIndex];
    }
    PupilDetectionMethod* getMethod2(std::string method) {
        for(auto pm: pupilDetectionMethods2) {
            if(pm->title() == method)
                return pm;
        }
        return nullptr;
    }

    PupilDetectionMethod* getCurrentMethod3() {
        return pupilDetectionMethods3[pupilDetectionIndex];
    }
    PupilDetectionMethod* getMethod3(std::string method) {
        for(auto pm: pupilDetectionMethods3) {
            if(pm->title() == method)
                return pm;
        }
        return nullptr;
    }

    PupilDetectionMethod* getCurrentMethod4() {
        return pupilDetectionMethods4[pupilDetectionIndex];
    }
    PupilDetectionMethod* getMethod4(std::string method) {
        for(auto pm: pupilDetectionMethods4) {
            if(pm->title() == method)
                return pm;
        }
        return nullptr;
    }
    //
    bool isStereo() {
        if( camera &&
            (   camera->getType() == CameraImageType::LIVE_STEREO_CAMERA ||
                camera->getType() == CameraImageType::STEREO_IMAGE_FILE ) )
            return true;
        else
            return false;
    }
    bool hasOpenCamera() {
        if (camera)
            return camera->isOpen();
        return false;
    }

    void startTracking();
    void stopTracking();

    // TODO: MAKE MAP, UNIFY WITH ENUM STYLE OLD INDEX RESOLUTION
    const std::vector<QChar> getEyeIdentities() {
        //std::cout << "-------------- single eye identity: " << QString(singleEyeIdentity).toStdString() << std::endl;

        if(currentProcMode == SINGLE_IMAGE_ONE_PUPIL)
            return {singleEyeIdentity};
        else if(currentProcMode == SINGLE_IMAGE_TWO_PUPIL)
            return {'R','L'};
        else if(currentProcMode == STEREO_IMAGE_ONE_PUPIL)
            return {singleEyeIdentity,singleEyeIdentity};
        else if(currentProcMode == STEREO_IMAGE_TWO_PUPIL)
            return {'R','R','L','L'};
    };
    const std::vector<QChar> getCamIdentities() {
        if(currentProcMode == SINGLE_IMAGE_ONE_PUPIL)
            return {'M'};
        else if(currentProcMode == SINGLE_IMAGE_TWO_PUPIL)
            return {'M','M'};
        else if(currentProcMode == STEREO_IMAGE_ONE_PUPIL)
            return {'M','S'};
        else if(currentProcMode == STEREO_IMAGE_TWO_PUPIL)
            return {'M','S','M','S'};
    };
    void setSingleEyeIdentity(QChar identity) {
        singleEyeIdentity = identity;
        //std::cout << "-------------- set single eye identity to " << QString(identity).toStdString() << std::endl;
    };

private:

    Camera *camera;

    CameraCalibration *singleCalibration;
    StereoCameraCalibration *stereoCalibration;

    int pupilDetectionIndex;
    QString currentConfigLabel;

    FrameRateCounter *frameCounter;

    ProcMode currentProcMode;
    QChar singleEyeIdentity = 'X';

    std::vector<PupilDetectionMethod*> pupilDetectionMethods1;
    std::vector<PupilDetectionMethod*> pupilDetectionMethods2;
    std::vector<PupilDetectionMethod*> pupilDetectionMethods3;
    std::vector<PupilDetectionMethod*> pupilDetectionMethods4;

    QString BRISQUEModelFileNameRES = ":/3rdparty/models/BRISQUE/brisque_model_live.yml";
    QString BRISQUERangeFileNameRES = ":/3rdparty/models/BRISQUE/brisque_range_live.yml";
    cv::Ptr<cv::quality::QualityBRISQUE> brisque;

    // NOTE:
    // in case of e.g.: ROIstereoImageTwoPupilRM
    // the NUMBER in the end denotes the different VIEWPOINTS of the same pupil
    // the LETTER denotes different EYES
    cv::Rect ROIsingleImageOnePupil; // formerly cv::Rect ROI;
    cv::Rect ROIsingleImageTwoPupilR;
    cv::Rect ROIsingleImageTwoPupilL;
    cv::Rect ROIstereoImageOnePupilM; // formerly cv::Rect ROI;
    cv::Rect ROIstereoImageOnePupilS; // formerly cv::Rect ROISecondary;
    cv::Rect ROIstereoImageTwoPupilRM;
    cv::Rect ROIstereoImageTwoPupilRS;
    cv::Rect ROIstereoImageTwoPupilLM;
    cv::Rect ROIstereoImageTwoPupilLS;
    cv::Rect ROImirrImageOnePupil1;
    cv::Rect ROImirrImageOnePupil2;

    QElapsedTimer processingTimer;

    QElapsedTimer drawTimer;
    int drawDelay;

    QMutex mutex;
    QMutex *imageMutex;
    QWaitCondition *imageProcessed;
    QWaitCondition *imagePublished;

    bool calibrated;
    bool trackingOn;
    bool useROIPreProcessing;
    bool useOutlineConfidence;
    bool computeBRISQUEEnabled;
    bool usePupilUndistort;
    bool useImageUndistort;
    //bool showROI;
    //bool showPupilCenter;

    std::vector<std::pair<uint64_t, long>> runtimeHistory;

    template<typename T> void writeVectorCSV(std::vector<std::pair<uint64_t , T>> data, const std::string &header, const std::string &filename);

    void populateWithMethods(std::vector<PupilDetectionMethod*> &vec);


    bool autoParamEnabled = false; // true as long as there is demand for autoParam. Also for informing other class instances through getter
    float autoParamPupSizePercent = 50;
    bool autoParamScheduled = false; // true only if a new performAutoParam() is necessary shortly. (need this, because performAutoParam cannot do anything when called without ROIs defined)
    
    bool autoParamSettingsEnabled = false; // True if pupil detection algorithm has Automatic Parametrization setting selected.

    bool synchronised = false; // True if both PupilDetection and Playback are running and synchronized.

    void performAutoParam();

    PupilDetectionMethod* getCurrentMethod(){
        return getCurrentMethod1();
    };

    void onNewSingleImageForOnePupilImpl(const CameraImage &image);
    void onNewSingleImageForTwoPupilImpl(const CameraImage &cimg);
    void onNewStereoImageForOnePupilImpl(const CameraImage &simg);
    void onNewStereoImageForTwoPupilImpl(const CameraImage &simg);

    void configureCameraConnection(bool connectOrDisconnect);

public slots:

    void enableComputeBRISQUE(bool state);

    void setAlgorithm(QString method);
    void setConfigLabel(QString config);

    void onNewSingleImageForOnePupil(const CameraImage &img);
    void onNewSingleImageForTwoPupil(const CameraImage &img);
    void onNewStereoImageForOnePupil(const CameraImage &simg);
    void onNewStereoImageForTwoPupil(const CameraImage &simg);

    void setAutoParamEnabled(bool state);
    void setAutoParamPupSizePercent(float value);
    bool isAutoParamSettingsEnabled();
    void setAutoParamSettingsEnabled(bool enabled);
    float getAutoParamPupSizePercent();
    //void performAutoParam();
    void setAutoParamScheduled(bool state);

    bool isTrackingOn();
    ProcMode getCurrentProcMode();
    void setCurrentProcMode(int val);
    
    QRect getROIsingleImageOnePupil();
    QRect getROIsingleImageTwoPupilR();
    QRect getROIsingleImageTwoPupilL();
    QRect getROIstereoImageOnePupilM();
    QRect getROIstereoImageOnePupilS();
    QRect getROIstereoImageTwoPupilRM();
    QRect getROIstereoImageTwoPupilRS();
    QRect getROIstereoImageTwoPupilLM();
    QRect getROIstereoImageTwoPupilLS();
    QRect getROImirrImageOnePupil1();
    QRect getROImirrImageOnePupil2();
    
    void setROIsingleImageOnePupil(QRectF roi); // formerly void setROI(QRectF roi);
    void setROIsingleImageTwoPupilR(QRectF roi);
    void setROIsingleImageTwoPupilL(QRectF roi);
    void setROIstereoImageOnePupilM(QRectF roi); // formerly void setROI(QRectF roi);
    void setROIstereoImageOnePupilS(QRectF roi); // formerly void setSecondaryROI(QRectF roi);
    void setROIstereoImageTwoPupilRM(QRectF roi);
    void setROIstereoImageTwoPupilRS(QRectF roi);
    void setROIstereoImageTwoPupilL1(QRectF roi);
    void setROIstereoImageTwoPupilL2(QRectF roi);
    void setROImirrImageOnePupil1(QRectF roi);
    void setROImirrImageOnePupil2(QRectF roi);

    void setSynchronised(bool synchronised);

signals:

    void processedImageLowFPS(CameraImage image);

//    void processedPlaybackImage(CameraImage mimg);
    void processedImageLowFPS(CameraImage mimg, int currentProcMode, std::vector<cv::Rect> ROIs, std::vector<Pupil> Pupils);
    void processedPupilData(quint64 timestamp, int currentProcMode, const std::vector<Pupil> &Pupils);
    void processedPupilDataLowFPS(quint64 timestamp, int currentProcMode, const std::vector<Pupil> &Pupils);

    void onROIPreprocessingChanged(bool state);

    void processingStarted();
    void processingFinished();

    void fps(double fps);
    void algorithmChanged();
    void configChanged(QString config);

};
