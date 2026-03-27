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

#include <queue>
#include <unordered_map>

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
        else
            return {'X'};
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
        else
            return {'M'}; // ?
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

    ////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////

    // Pupil data Tracked Time Window
    quint64 pupilTTWRefitLC;
    int pupilTTWRefitDelayMs = 2*1000;
    float pupilTTWCriterion_confidence = 0.8;
    float pupilTTWCriterion_outlineConfidence = 0.8;
    float pupilTTWCriterion_axisRatio = 2.0; // = MAJOR / MINOR ratio
    quint64 pupilTTWTimeWindowMs = 1*1000;
//    float pupilTTWExpectedFPS = 50;
    std::vector<std::vector<cv::Point2f>> pupilTTW_centers;
    std::vector<std::vector<float>> pupilTTW_dias; // TODO: seat MA and ma into a Point2F or such, so we could easily stash it and fit width & height of ROI easily
    std::vector<std::vector<quint64>> pupilTTWTimestamps;
    bool ROIeyeFitScheduled = false;

    QVector<QRectF> pupilTTW_suggestedROIs = {QRectF(), QRectF(), QRectF(), QRectF()};

    int pupilTTWminSamples = 5;

    float wfac_basic = 3.5f;
    float hfac_basic = 3.0f;
    float wfac_clueless = 5.0f;
    float hfac_clueless = 4.5f;

    bool pupilTTW_useConfidence = false;
    bool pupilTTW_useOutlineConfidence = false;

    /////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////

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
    void setROIstereoImageTwoPupilLM(QRectF roi);
    void setROIstereoImageTwoPupilLS(QRectF roi);
    void setROImirrImageOnePupil1(QRectF roi);
    void setROImirrImageOnePupil2(QRectF roi);

    void setSynchronised(bool synchronised);

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    void timeWindow_init(quint64 _pupilTTWTimeWindowMs, int _pupilTTWRefitDelayMs, int _pupilTTWExpectedFPS) {
        pupilTTWTimeWindowMs = _pupilTTWTimeWindowMs;
        pupilTTWRefitDelayMs = _pupilTTWRefitDelayMs;
    }
    /*
    void setTimeWindow_pupilTTW(quint64 _pupilTTWTimeWindowMs) {
        pupilTTWTimeWindowMs = _pupilTTWTimeWindowMs;
    }
    void setRefitDelay_pupilTTW(int _pupilTTWRefitDelayMs) {
        pupilTTWRefitDelayMs = _pupilTTWRefitDelayMs;
    }
    void setExpectedFPS_pupilTTW(int _pupilTTWExpectedFPS) {
        pupilTTWExpectedFPS = _pupilTTWExpectedFPS;
    }
    */

    float vecMeanF(std::vector<float> vec){
        return (std::accumulate(vec.begin(), vec.end(), 0.0) / vec.size());
    }

    cv::Point2f vecMeanP2F(std::vector<cv::Point2f> vec){
        return (std::accumulate(vec.begin(), vec.end(), cv::Point2f(0, 0)) * (1.0f / vec.size()));
    }

    void advanceTTW(int pdx){
        int nChecked = 0;
        int nAll = pupilTTW_centers[pdx].size();
        while(pupilTTW_centers[pdx].size() > 0 && nChecked < nAll) {
            if( pupilTTWTimestamps[pdx][pupilTTW_centers[pdx].size()-1] - pupilTTWTimestamps[pdx][0] > pupilTTWTimeWindowMs
                    ) {

                // pops. should be slow anyway
                if (!pupilTTW_centers[pdx].empty()) {
                    pupilTTW_centers[pdx].erase(pupilTTW_centers[pdx].begin());
                }
                if (!pupilTTW_dias[pdx].empty()) {
                    pupilTTW_dias[pdx].erase(pupilTTW_dias[pdx].begin());
                }
                if (!pupilTTWTimestamps[pdx].empty()) {
                    pupilTTWTimestamps[pdx].erase(pupilTTWTimestamps[pdx].begin());
                }
            }
            nChecked++;
        }
    }

    QRectF pupilTTW_suggestROI(cv::Point2f avg_center, float avg_dia) {
        float wfac, hfac;
        if(ROIeyeFitScheduled) {
            wfac = wfac_clueless;
            hfac = hfac_clueless;
            ROIeyeFitScheduled = false;
        } else {
            wfac = wfac_basic;
            hfac = hfac_basic;
        }
        return QRectF(
                avg_center.x - wfac/2.0*avg_dia,
                avg_center.y - hfac/2.0*avg_dia,
                wfac*avg_dia,
                hfac*avg_dia
        );
    }

    void updatePupilTTW(quint64 _timestamp, int _currentProcMode, const std::vector<Pupil> &_Pupils) {
        //processedPupilData(image.timestamp, currentProcMode, Pupils)

        // TODO: keep (not the center of pupil, but)
        //  the midpoint between center of pupil and center of eyeball,
        //  in/as the center of image

        // check criteria
        for(int zz = 0; zz < _Pupils.size(); zz++) {
//        qDebug() << "confidence" << _Pupils[zz].confidence; // NOT ALL ALGS HAVE CONFIDENCE.
//        qDebug() << "outline_confidence" << _Pupils[zz].outline_confidence; // ALSO MIGHT BE DISABLED
//            qDebug() << "axis ratio" << (_Pupils[zz].majorAxis() / _Pupils[zz].minorAxis());
            if (    ((pupilTTW_useConfidence && _Pupils[zz].confidence > pupilTTWCriterion_confidence) || !pupilTTW_useConfidence) &&
                    ((pupilTTW_useOutlineConfidence && _Pupils[zz].outline_confidence > pupilTTWCriterion_outlineConfidence) || !pupilTTW_useOutlineConfidence) //&&
                //(_Pupils[zz].majorAxis() / _Pupils[zz].minorAxis()) <= pupilTTWCriterion_axisRatio
                    ) {

                pupilTTW_centers[zz].push_back(_Pupils[zz].center);
                pupilTTW_dias[zz].push_back(_Pupils[zz].diameter());
                pupilTTWTimestamps[zz].push_back(_timestamp);
            }
        }

        for(int uu = 0; uu < _Pupils.size(); uu++) {
            if (_timestamp - pupilTTWRefitLC > pupilTTWRefitDelayMs) {
                advanceTTW(uu);
                pupilTTW_suggestedROIs[uu] = QRectF(); // "clear" it
                if (pupilTTW_centers[uu].size() < pupilTTWminSamples)
                    return;
                pupilTTW_suggestedROIs[uu] = pupilTTW_suggestROI(vecMeanP2F(pupilTTW_centers[uu]), vecMeanF(pupilTTW_dias[uu]));
            }
        }

        if(_timestamp - pupilTTWRefitLC > pupilTTWRefitDelayMs) {
            if (_currentProcMode == ProcMode::SINGLE_IMAGE_ONE_PUPIL) {
                if(!pupilTTW_suggestedROIs[PupilVecIdx::SINGLE_IMAGE_ONE_PUPIL_MAIN].isEmpty())
                    setROIsingleImageOnePupil(pupilTTW_suggestedROIs[PupilVecIdx::SINGLE_IMAGE_ONE_PUPIL_MAIN]);
            } else if (_currentProcMode == ProcMode::SINGLE_IMAGE_TWO_PUPIL) {
                if(!pupilTTW_suggestedROIs[PupilVecIdx::SINGLE_IMAGE_TWO_PUPIL_L].isEmpty())
                    setROIsingleImageTwoPupilL(pupilTTW_suggestedROIs[PupilVecIdx::SINGLE_IMAGE_TWO_PUPIL_L]);
                if(!pupilTTW_suggestedROIs[PupilVecIdx::SINGLE_IMAGE_TWO_PUPIL_R].isEmpty())
                    setROIsingleImageTwoPupilR(pupilTTW_suggestedROIs[PupilVecIdx::SINGLE_IMAGE_TWO_PUPIL_R]);
            } else if (_currentProcMode == ProcMode::STEREO_IMAGE_ONE_PUPIL) {
                if(!pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_ONE_PUPIL_MAIN].isEmpty())
                    setROIstereoImageOnePupilM(pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_ONE_PUPIL_MAIN]);
                if(!pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_ONE_PUPIL_SEC].isEmpty())
                    setROIstereoImageOnePupilS(pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_ONE_PUPIL_SEC]);
            } else if (_currentProcMode == ProcMode::STEREO_IMAGE_TWO_PUPIL) {
                if(!pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_TWO_PUPIL_L_MAIN].isEmpty())
                    setROIstereoImageTwoPupilLM(pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_TWO_PUPIL_L_MAIN]);
                if(!pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_TWO_PUPIL_L_SEC].isEmpty())
                    setROIstereoImageTwoPupilLS(pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_TWO_PUPIL_L_SEC]);
                if(!pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_TWO_PUPIL_R_MAIN].isEmpty())
                    setROIstereoImageTwoPupilRM(pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_TWO_PUPIL_R_MAIN]);
                if(!pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_TWO_PUPIL_R_SEC].isEmpty())
                    setROIstereoImageTwoPupilRS(pupilTTW_suggestedROIs[PupilVecIdx::STEREO_IMAGE_TWO_PUPIL_R_SEC]);
            } else {
                qDebug() << "Could not determine pupilDetection proc mode or it is still undetermined";
            }

//            emit refitPupilROIs(_currentProcMode);
            pupilTTWRefitLC = _timestamp;
        }


    };
    void emptyPupilTTW () {
        // TODO
    }

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

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
