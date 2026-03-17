#pragma once

/**
    @authors Moritz Lode, Gabor Benyei, Attila Boncser
*/

#include "subwindows/MCUSettingsDialog.h"
#include "subwindows/singleCameraSettingsDialog.h"
#include "devices/singleCamera.h"
#include "devices/stereoCamera.h"
#include "subwindows/pupilDetectionSettingsDialog.h"
#include "subwindows/setupGeometryDialog.h"
#include "subwindows/singleCameraView.h"
#include "data-io/dataWriter.h"
#include "data-io/imageWriter.h"
#include "data-io/recSectionExporter.h"
#include "subwindows/generalSettingsDialog.h"
#include "subwindows/subjectSelectionDialog.h"
#include "subwindows/stereoCameraSettingsDialog.h"
#include "subwindows/RestorableQMdiSubWindow.h"
#include "signalPubSubHandler.h"
#include <QMainWindow>
#include <QMdiSubWindow>
#include <QSettings>

#include "supportFunctions.h"
#include "metaSnapshotOrganizer.h"
#include "data-io/dataStreamer.h"
#include "devices/camTempMonitor.h"
#include "subwindows/imagePlaybackControlDialog.h"
#include "subwindows/remoteCCDialog.h"
#include "subwindows/streamingSettingsDialog.h"
#include "data-io/connPoolCOM.h"
#include "data-io/connPoolUDP.h"
#include "devices/singleWebcam.h"
#include "subwindows/singleWebcamSettingsDialog.h"
#include "subwindows/singleWebcamCalibrationView.h"
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include "recEventTracker.h"
#include "SVGIconColorAdjuster.h"
#include "playbackSynchroniser.h"
#include "pDataTypes.h"
#include "subwindows/sceneImageView.h"
#include "subwindows/gettingStartedWizard.h"
//#include <QtMultimedia/QCameraInfo>
#include "subwindows/openZipChoiceDialog.h"
#include "subwindows/threeChoiceDialog.h"
#include "subwindows/exportRecSectionDialog.h"
#include "adminPrivileges.h"

#ifdef USE_PYLON
#include <pylon/TlFactory.h>
//#include <pylon/PylonIncludes.h>
#include <pylon/gige/GigETransportLayer.h>
#endif


class MouseLeaveCatchFilter : public QObject {
Q_OBJECT
public:
    explicit MouseLeaveCatchFilter(QObject *parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (auto menu = qobject_cast<QMenu*>(watched)) {
            if (event->type() == QEvent::Leave) {
                // Ignore leave event to prevent hiding
                return true; // Block the event
            } else if (event->type() == QEvent::HoverLeave) {
                // Ignore leave event to prevent hiding
                return true; // Block the event
            } else if (event->type() == QEvent::FocusAboutToChange) {
                // Ignore leave event to prevent hiding
                return true; // Block the event
            } else if (event->type() == QEvent::FocusOut) {
                // Ignore leave event to prevent hiding
                return true; // Block the event
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

/**
    Main interface of the software

    Creates all GUI and processing objects and handles/connects their signal-slot connections

    Creates threads for concurrent processing for i.e. calibration and pupil detection
*/
class MainWindow : public QMainWindow {
    Q_OBJECT

public:

    static int const EXIT_CODE_REBOOT;

    MainWindow();
    ~MainWindow() override;

protected:

    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    //void dragMoveEvent(QDragMoveEvent* e) override; // likely not necessary
    void dropEvent(QDropEvent* e) override;

private:
 
    SignalPubSubHandler *signalPubSubHandler;

    QSettings *applicationSettings;
    QDir settingsDirectory;

    // Store separately these
    QString recentImageReadingDirectory;
    QString recentImageWritingDirectory;
    QString recentDataWritingDirectory;

    QString dataRecordingOutputTarget;
    QString imageRecordingOutputTarget;
    //QString imageDirectory;
    //QString recentPath;

    QMdiArea *mdiArea;
    QToolBar *toolBar;

    RestorableQMdiSubWindow *calibrationWindow; 
    RestorableQMdiSubWindow *cameraViewWindow;
    RestorableQMdiSubWindow *sharpnessWindow;
    RestorableQMdiSubWindow *dataTableWindow;
    RestorableQMdiSubWindow *sceneImageWindow;

    QIcon fileOpenIcon;
    QIcon exportRecSectionIcon;
    QIcon cameraSerialConnectionIcon;
    QIcon pupilDetectionSettingsIcon;
    QIcon setupGeometryIcon;
    QIcon remoteCCIcon;
    QIcon generalSettingsIcon;
    QIcon singleCameraIcon;
    QIcon stereoCameraIcon;
    QIcon cameraSettingsIcon1;
    QIcon cameraSettingsIcon2;
    QIcon calibrateIcon;
    QIcon sharpnessIcon;
    //QIcon subjectsIcon;
    QIcon outputDataFileIcon;
    QIcon streamingSettingsIcon;
    QIcon imagePlaybackControlIcon;
    QIcon dataTableIcon;
    QIcon sceneImageViewIcon;
    QIcon archiveIcon;
    QIcon videoFileIcon;

    QMenu *windowMenu;
    QMenu *cameraMenu;
    QMenu *singleCamerasMenu;
//    QMenu *openCVCamerasMenu;

    QAction *cameraViewAct;
    QAction *dataTableAct;
    QAction *sceneImageViewAct;

    QAction *cameraAct;
    QAction *cameraSettingsAct;
    QAction* cameraActDisconnectAct;
    QAction *trackAct;
    QAction *recordAct;
    QAction *calibrateAct;
    QAction *logFileAct;
//    QAction *outputDirectoryAct;
    QAction *imageRecordingOutputAct;
    QAction *recordImagesAct;

//    QAction *closeAct;
//    QAction *closeAllAct;
//    QAction *tileAct;
    QAction *cascadeAct;
    QAction *resetGeometryAct;

    QAction *nextAct;
    QAction *previousAct;
    QAction *windowMenuSeparatorAct;
    //QAction *subjectsAct;
    QAction *sharpnessAct;

    QLabel *serialStatusIcon;
    QLabel *hwTriggerStatusIcon;
    QLabel *warmedUpStatusIcon;
    QLabel *calibrationStatusIcon;
    //QLabel *subjectConfigurationLabel;
    QLabel *currentStatusMessageLabel;
    
    // TODO: Move trackingOn into class instance, and get rid of others, use nullptr check instead. better like that I think. Also
    bool trackingOn = false; // NOTE: also accessible in pupildetection now
    bool recordOn = false;
    bool recordImagesOn = false;
    //bool playImagesOn = false; // NOTE: from now can be checked via ImagePlaybackControlDialog
    bool hwTriggerOn = false;
    bool cameraPlaying = true;
    bool exportingRecSection = false;

    void loadIcons();
    void createActions();
    void createStatusBar();
    void readSettings();
    void writeSettings();

    QWidget* activeMdiChild() const;

#ifdef USE_PYLON
    Pylon::DeviceInfoList_t enumerateCameraDevices();
#else
    uint enumerateCameraDevices();
#endif

    Camera *selectedCamera;

    PupilDetection *pupilDetectionWorker;
    QThread *pupilDetectionThread;

    DataWriter *dataWriter;
    ImageWriter *imageWriter;
    RecSectionExporter *recSectionExporter;
    QThread *imageWriterThread;

    bool streamOn = false;

    // TODO: These are all dialogs that get opened DIRECTLY as a child of main window (not inside an MDI subwindow).
    //  Currently, on MacOS, they can be erroneously occluded by the main window once they lose focus.
    //  This is probably a Qt bug.
    // TODO: Also, always only one of them can exist at once, so it could be good to rethink their
    //  instantiation and memory management.
    MCUSettingsDialog *MCUSettingsDialogInst;
    PupilDetectionSettingsDialog *pupilDetectionSettingsDialog;
    SetupGeometryDialog *setupGeometryDialog;
    GeneralSettingsDialog *generalSettingsDialog;
    //SubjectSelectionDialog *subjectSelectionDialog;
    SingleCameraSettingsDialog *singleCameraSettingsDialog;
    StereoCameraSettingsDialog *stereoCameraSettingsDialog;
    RemoteCCDialog *remoteCCDialog;
    StreamingSettingsDialog *streamingSettingsDialog;
    SingleWebcamSettingsDialog *singleWebcamSettingsDialog;

    ImagePlaybackControlDialog *imagePlaybackControlDialog;

    // made these two global to be able to pass singlecameraview instance pointer to ...CameraSettingsDialog constructors:
    SingleCameraView *singleCameraChildWidget; 
    StereoCameraView *stereoCameraChildWidget;

    //QThread *tempMonitorThread;
    CamTempMonitor *camTempMonitor;
    RecEventTracker *recEventTracker;
    quint64 imageRecStartTimestamp;

    ConnPoolCOM *connPoolCOM;
    ConnPoolUDP *connPoolUDP;

    QSpinBox *webcamDeviceBox;

    QAction *fileOpenAct; // GB: made global to let it disable when image directory is already open
    QAction *exportRecSectionAct;
    QAction *toggleFullscreenAct;
    QAction *streamingSettingsAct;
    QAction *streamAct;

    QAction *forceResetTrialAct;
    QAction *manualIncTrialAct;
    QAction *forceResetMessageAct;

    QWidget *trialWidget;
    QLabel *currentTrialLabel;
    QFrame* trialWidgetLayoutSep;
    QWidget *messageWidget;
    QLabel *currentMessageLabel;
    QFrame* messageWidgetLayoutSep;
    QLabel *remoteStatusIcon;

    GettingStartedWizard* aboutWizard = nullptr;
    GettingStartedWizard* aboutAndUserGuideWizard = nullptr;
    GettingStartedWizard* userGuideWizard = nullptr;

    DataStreamer *dataStreamer;
    QMutex *imageMutex;
    QWaitCondition *imagePublished;
    QWaitCondition *imageProcessed;

    PlaybackSynchroniser *playbackSynchroniser;

    QMessageBox *imagesSkippedMsgBox = nullptr;
    QMessageBox *imageWriterFailedMsgBox = nullptr;
    QMessageBox *deviceWasResetMsgBox = nullptr;

    void loadCalibrationWindow();
    void loadSharpnessWindow();
    void loadDataTableWindow();
    void loadSceneImageWindow();

    void stopCamera();
    void startCamera();

    void resetStatus(bool isConnect);

    void openImageFileSource(QString imageSource, int subrecordingNumber);

    void setRecentImageReadingDirectory(QString path);
    void setRecentImageWritingDirectory(QString path);
    void setRecentDataWritingDirectory(QString path);

    void connectCameraPlaybackChangedSlots();

private slots:

    void onSerialConnect();
    void onSerialDisconnect();

    void onHwTriggerEnable();
    void onHwTriggerDisable();

    void onDeviceWarmupHasDeltaTimeData();
    void onDeviceWarmedUp();
    void onDeviceWarmUpReadingsInvalid();
    void onDeviceWarmUpReadingsUnavailable();
    void onDeviceWarmedUpReset();

    void onWebcamStartedToOpen();
    void onWebcamCouldNotBeOpened();
    void onWebcamSuccessfullyOpened();

    void onCameraCalibrationEnabled();
    void onCameraCalibrationDisabled();

    void onOpenImageRecordingClicked();
    void onExportRecSectionClicked();
    void onExportAllowedToEnd();

    void onCameraClick();
    void onImageRecordingOutputClick();
    void onCameraDisconnectClick();
    void onCameraSettingsClick();
    void onSingleCameraSettingsClick();
    void onStereoCameraSettingsClick();

    void onCalibrateClick();
    //void onSubjectsClick();

    void onTrackActClick();
    void updateRois(); // DEV
    void onRecordClick();

    void onGeneralSettingsChange();

    void onPupilDetectionProcModeChange(int);

    void cameraViewClick();

    void onRecordImageClick();

    void singleCameraSelected(QAction *action);
    void stereoCameraSelected();

    void onCreateGraphPlot(const PDataType &value);

    void dataTableClick();
    void sceneImageViewClick();
    void toggleFullscreen();

    void setLogFile();
    void imageRecordingOutputDirectorySelected();
    void imageRecordingOutputZipSelected();
    void imageRecordingOutputVideoSelected();

    void updateMenus();
    void updateSingleCamerasMenu();
//    void updateOpenCVCamerasMenu();
    void updateWindowMenu();
    void about();
    void userGuide();
    void openSourceDialog();
    void resetGeometry();
//    void closeActiveSubWindow();
//    void closeAllSubWindows();

    //void onSubjectsSettingsChange(QString subject);
    void onSharpnessClick();

    void offerResetApplicationSettings();
    void offerRestartApplication();

//    void onGettingsStartedWizardFinish();

    void singleWebcamSelected(QAction *action);
    void onSingleWebcamSettingsClick();

    void onStreamClick();
    void onStreamingSettingsClick();

    void onPlaybackStartInitiated();
    void onPlaybackPauseInitiated();
    void onPlaybackStopInitiated();

    void onRemoteConnStateChanged();
    //void onStreamingConnStateChanged();

    void updateCurrentTrialLabel();
    void safelyResetTrialCounter();
    void safelyResetTrialCounter(const quint64 &timestamp);
    void forceResetTrialCounter();
    void forceResetTrialCounter(const quint64 &timestamp);
    void incrementTrialCounter();
    void incrementTrialCounter(const quint64 &timestamp);
    void logRemoteMessage(const quint64 &timestamp, const QString &str);
    void updateCurrentMessageLabel();
    void safelyResetMessageRegister();
    void safelyResetMessageRegister(const quint64 &timestamp);
    void forceResetMessageRegister();
    void forceResetMessageRegister(const quint64 &timestamp);

    void onStreamingUDPConnect();
    void onStreamingUDPDisconnect();
    void onStreamingCOMConnect();
    void onStreamingCOMDisconnect();
#ifdef USE_LSL
    void onStreamingLSLConnect();
    void onStreamingLSLDisconnect();
#endif

    void onImagesSkipped();
    void onImagesSkippedMsgClose();

    void createCamTempMonitor();
    void destroyCamTempMonitor();

public slots:

    // NOTE: definitions of functions for programmatic control of GUI elements (their names beginning with PRG...)
    // are stored in PRGmainwindow.cpp, to keep mainwindow.cpp cleaner
    void PRGlogRemoteMessage(const quint64 &timestamp, const QString &str);

    void PRGopenSingleCamera(const QString &camName);
    void PRGopenStereoCamera(const QString &camName1, const QString &camName2);
    void PRGopenSingleWebcam(int deviceID);
    void PRGcloseCamera();
    void PRGtrackStart();
    void PRGtrackStop();
    void PRGrecordStart();
    void PRGrecordStop();
    void PRGrecordImageStart();
    void PRGrecordImageStop();
    void PRGstreamStart();
    void PRGstreamStop();
    void PRGincrementTrialCounter(const quint64 &timestamp);
    void PRGforceResetTrialCounter(const quint64 &timestamp);
    // NOTE: there is no programmatic implementation for resetting the message register. It can be done by sending a blank message
    void PRGsetImageOutputTarget(QString str);
    void PRGsetCsvPathAndName(const QString &str);
    
    void PRGsetGlobalDelimiter(const QString &str);
    void PRGsetImageOutputFormat(QString format);
    void PRGsetPupilDetectionAlgorithm(const QString &alg);
    void PRGsetPupilDetectionUsingROI(const QString &state);
    void PRGsetPupilDetectionCompOutlineConf(const QString &state);
    void PRGsetPupilDetectionCompBRISQUE(const QString &state);
    void PRGconnectRemoteUDP(QString conf);
    void PRGconnectRemoteCOM(QString conf);
    void PRGconnectStreamUDP(QString conf);
    void PRGconnectStreamCOM(QString conf);
    void PRGconnectMicrocontrollerUDP(QString conf);
    void PRGconnectMicrocontrollerCOM(QString conf);
    void PRGdisconnectRemoteUDP();
    void PRGdisconnectRemoteCOM();
    void PRGdisconnectStreamUDP();
    void PRGdisconnectStreamCOM();
    void PRGdisconnectMicrocontroller();
#ifdef USE_LSL
    void PRGdisconnectStreamLSL();
    void PRGconnectStreamLSL(QString conf);
#endif

    void PRGenableHWT(bool state);
    void PRGstartHWT();
    void PRGstopHWT();
    void PRGsetHWTlineSource(int lineSourceNum);
    void PRGsetHWTruntime(float runtimeMinutes);
    void PRGsetHWTframerate(int fps);
    void PRGenableSWTframerateLimiting(const QString &state);
    void PRGsetSWTframerate(int fps);

    void PRGsetExposure(int value);
    void PRGsetGain(double value);
    void PRGsetBinning(int value);

    void onImageWriterFailed();
    void onImageWriterFailedMsgClose();
    //void onImageWriterStopDone();
    void onCameraUnexpectedlyDisconnected();
    void onDeviceWasReset();
    void onDeviceWasResetMsgClose();
    void onManualDeviceResetNecessary();

    void onCameraFreezePressed();

    void onCameraPlaybackChanged();

    void onStereoCamerasOpened();
    void onStereoCamerasClosed();

signals:
    void commitTrialCounterIncrement(quint64 timestamp);
    void commitTrialCounterReset(quint64 timestamp);
    void commitRemoteMessage(quint64 timestamp, QString str);
    void commitMessageRegisterReset(quint64 timestamp);

    void cameraPlaybackChanged();

    void playbackStartApproved();
    void playbackPauseApproved();
    void playbackStopApproved();

};



