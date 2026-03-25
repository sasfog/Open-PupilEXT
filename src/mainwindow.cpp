#include "mainwindow.h"
#include <QtWidgets>
#include <QtWidgets/QWidget>
#include "subwindows/graphPlot.h"
#include "subwindows/singleCameraView.h"
#include "subwindows/singleCameraCalibrationView.h"
#include "subwindows/dataTable.h"
#include "devices/fileCamera.h"
#include "subwindows/stereoCameraSettingsDialog.h"
#include "subwindows/stereoCameraView.h"
#include "subwindows/stereoCameraCalibrationView.h"
#include "subwindows/stereoFileCameraCalibrationView.h"
#include "subwindows/singleFileCameraCalibrationView.h"
#include "subwindows/RestorableQMdiSubWindow.h"
#include "subwindows/singleCameraSharpnessView.h"
#include "supportFunctions.h"
#include "mainwindow.h"
#include <QtWidgets>
#include <QtWidgets/QWidget>
#include "subwindows/graphPlot.h"
#include "subwindows/singleCameraView.h"
#include "subwindows/singleCameraCalibrationView.h"
#include "subwindows/dataTable.h"
#include "devices/fileCamera.h"
#include "subwindows/stereoCameraSettingsDialog.h"
#include "subwindows/stereoCameraView.h"
#include "subwindows/stereoCameraCalibrationView.h"
#include "subwindows/stereoFileCameraCalibrationView.h"
#include "subwindows/singleFileCameraCalibrationView.h"
#include "subwindows/RestorableQMdiSubWindow.h"
#include "subwindows/singleCameraSharpnessView.h"
#include "supportFunctions.h"

int const MainWindow::EXIT_CODE_REBOOT = 2000;

// Upon construction, worker objects for processing are created pupil detection and its respective thread
MainWindow::MainWindow():
                          mdiArea(new QMdiArea(this)),
                          signalPubSubHandler(new SignalPubSubHandler(this)),
                          

                          //subjectSelectionDialog(new SubjectSelectionDialog(this)),
                          singleCameraSettingsDialog(nullptr),
                          stereoCameraSettingsDialog(nullptr),
                          pupilDetectionThread(new QThread()),
                          selectedCamera(nullptr),
                          cameraViewWindow(nullptr),
                          calibrationWindow(nullptr),
                          sharpnessWindow(nullptr),
                          dataWriter(nullptr),
                          imageWriterThread(new QThread()),
                          imageWriter(nullptr),
                          recSectionExporter(nullptr),

                          singleWebcamSettingsDialog(nullptr),
                          singleCameraChildWidget(nullptr),
                          stereoCameraChildWidget(nullptr),
                          recEventTracker(nullptr),
                          camTempMonitor(nullptr),
                          connPoolCOM(new ConnPoolCOM(this)),
                          connPoolUDP(new ConnPoolUDP(this)),
                          dataStreamer(nullptr),
                          imagePlaybackControlDialog(nullptr),

                          /*MCUSettingsDialogInst(new MCUSettingsDialogInst(connPoolCOM, this)),
                          //remoteCCDialog(new RemoteCCDialog(connPoolUDP, connPoolCOM, this)),
                          remoteCCDialog(new RemoteCCDialog(connPoolCOM,this)), //connPoolCOM, pupilDetectionWorker, dataWriter, imageWriter, dataStreamer, offlineEventLogWriter, 
                          streamingSettingsDialog(new StreamingSettingsDialog(connPoolCOM, pupilDetectionWorker, dataStreamer, this)),
*/
                          applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), this)) {

    loadIcons();

    if(!AdminPrivileges::isRunningAsAdmin() && SupportFunctions::readBoolFromQSettings("adminWarning", true, applicationSettings)) {
        QString innerText = "PupilEXT detected that it was started without administrator privileges. It is however best advised to run the application with these elevated privileges. Would you like to try restart the application with privileges requested?";
#ifdef Q_OS_WIN
        innerText = innerText + "\nNote: if run with administrator rights on Windows, the drag-and-drop feature will not work due to UAC restrictions.";
#endif
        ThreeChoiceDialog *dialog = new ThreeChoiceDialog(
                "Application was started without administrator privileges",
                innerText,
                "Restart",
                "Dismiss",
                "Always dismiss",
                QSize(450,150),
                this);
        dialog->setModal(true);
        // dialog->raise();
        if(dialog->exec() == QDialog::Accepted)
        {
            auto resp = dialog->getResponse();

            if(resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_1) {
                // Try restart with privileges
                if(AdminPrivileges::restartAsAdmin(QCoreApplication::arguments())) {
                    //qApp->quit();
                    //qApp->exit();
                    //QCoreApplication::exit(0);
                    std::exit(0); // only this one surely exits right then
                    //return;
                } else {
                    // Could not start new instance with elevated privileges
                    // ...
                }
            } else if(resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_2) {
                // Do nothing
                // ...
            } else /*if(resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_3)*/ {
                applicationSettings->setValue("adminWarning", false);
            }
        }
    }

    if(SupportFunctions::readBoolFromQSettings("alwaysOnTop", false, applicationSettings)) {
        this->setWindowFlags(this->windowFlags() | Qt::WindowStaysOnTopHint);
        //show();
    }

    imageMutex = new QMutex();
    imagePublished = new QWaitCondition();
    imageProcessed = new QWaitCondition();
    pupilDetectionWorker = new PupilDetection(imageMutex, imagePublished, imageProcessed);
    MCUSettingsDialogInst = new MCUSettingsDialog(connPoolCOM, connPoolUDP, this);
    MCUSettingsDialogInst->setWindowIcon(cameraSerialConnectionIcon);
    remoteCCDialog = new RemoteCCDialog(connPoolCOM, connPoolUDP, this); //connPoolCOM, pupilDetectionWorker, dataWriter, imageWriter, dataStreamer, offlineEventLogWriter,
    remoteCCDialog->setWindowIcon(remoteCCIcon);
    streamingSettingsDialog = new StreamingSettingsDialog(connPoolCOM, connPoolUDP, pupilDetectionWorker, this);
    streamingSettingsDialog->setWindowIcon(streamingSettingsIcon);

    settingsDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    if(!settingsDirectory.exists()) {
// mkdir(".") DOES NOT WORK ON MACOS, ONLY WINDOWS. (Reported on MacOS 12.7.6 and Windows 10)
//        settingsDirectory.mkdir(".");
        QDir().mkpath(settingsDirectory.absolutePath());
    }

    qDebug() << "Application settings location: " << applicationSettings->fileName() << Qt::endl;

    connect(MCUSettingsDialogInst, SIGNAL (onConnect()), this, SLOT (onSerialConnect()));
    connect(MCUSettingsDialogInst, SIGNAL (onDisconnect()), this, SLOT (onSerialDisconnect()));

    pupilDetectionSettingsDialog = new PupilDetectionSettingsDialog(pupilDetectionWorker, this);
    pupilDetectionSettingsDialog->setWindowIcon(pupilDetectionSettingsIcon);
#ifdef QT_DEBUG
    setupGeometryDialog = new SetupGeometryDialog(this);
    setupGeometryDialog->setWindowIcon(setupGeometryIcon);
#endif
    generalSettingsDialog = new GeneralSettingsDialog(this);
    generalSettingsDialog->setWindowIcon(generalSettingsIcon);

    //subjectSelectionDialog->setWindowIcon(subjectsIcon);

    connect(generalSettingsDialog, SIGNAL (onSettingsChange()), this, SLOT (onGeneralSettingsChange()));
    connect(generalSettingsDialog, SIGNAL (onSettingsChangeNeedingRestart()), this, SLOT (offerRestartApplication()));
    //connect(subjectSelectionDialog, SIGNAL (onSubjectChange(QString)), this, SLOT (onSubjectsSettingsChange(QString)));
    //connect(subjectSelectionDialog, SIGNAL (onSettingsChange()), pupilDetectionSettingsDialog, SLOT (onSettingsChange()));
    connect(pupilDetectionSettingsDialog, SIGNAL (pupilDetectionProcModeChanged(int)), this, SLOT (onPupilDetectionProcModeChange(int)));

    // Pupil detection is conducted in another thread, move the created object to this thread and connect its finished signal for cleanup
    pupilDetectionWorker->moveToThread(pupilDetectionThread);
    connect(pupilDetectionThread, SIGNAL (finished()), pupilDetectionThread, SLOT (deleteLater()));
    pupilDetectionThread->start();
    pupilDetectionThread->setPriority(QThread::HighPriority); // TODO: highest priority

    // Image writing is going to be on a separate thread, allowing for a lot better control
//    imageWriter = new ImageWriter(this); // we cannot use mainwindow as parent, because then we could not move to a thread
    imageWriter = new ImageWriter();
    imageWriter->moveToThread(imageWriterThread);
    connect(imageWriterThread, SIGNAL (finished()), imageWriterThread, SLOT (deleteLater()));
    imageWriterThread->start();
    imageWriterThread->setPriority(QThread::NormalPriority);
    //
    connect(imageWriter, SIGNAL (writingFailed()), this, SLOT (onImageWriterFailed()));

    // Runs on GUI thread, okay for now
    recSectionExporter = new RecSectionExporter(this);
//    connect(recSectionExporter, SIGNAL (writingFailed()), this, SLOT (onRecSectionExporterFailed()));

    mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QVBoxLayout *verticalLayout = new QVBoxLayout(mdiArea);
    mdiArea->setLayout(verticalLayout);
    mdiArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setCentralWidget(mdiArea);

    connect(mdiArea, &QMdiArea::subWindowActivated, this, &MainWindow::updateMenus);

    createActions();
    createStatusBar();
    updateMenus();

    // read user settings stored in the user directory
    readSettings();

    setWindowTitle(QCoreApplication::applicationName());
    setUnifiedTitleAndToolBarOnMac(true);

    QIcon::setThemeName( "Breeze" );

    QStringList themeSearchPaths = QIcon::themeSearchPaths();
    themeSearchPaths.prepend(":/icons");
    QIcon::setThemeSearchPaths(themeSearchPaths);

    qRegisterMetaType<Pupil>("Pupil");
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<CameraImage>("CameraImage");
    qRegisterMetaType<cv::Rect>("cv::Rect");
    qRegisterMetaType<std::vector<Pupil>>("std::vector<Pupil>");
    qRegisterMetaType<std::vector<cv::Rect>>("std::vector<cv::Rect>");
    qRegisterMetaType<QSerialPort::SerialPortError>("QSerialPort::SerialPortError");
    qRegisterMetaType<std::vector<double>>("std::vector<double>");

    bool showGettingsStartedWizard = SupportFunctions::readBoolFromQSettings("ShowGettingsStartedWizard", true, applicationSettings);
    if(showGettingsStartedWizard) {
        aboutAndUserGuideWizard = new GettingStartedWizard(GettingStartedWizard::WizardPurpose::ABOUT_AND_USERGUIDE, this);
        aboutAndUserGuideWizard->show();
        connect(aboutAndUserGuideWizard->button(QWizard::FinishButton), &QPushButton::clicked, this,
                [this]() {applicationSettings->setValue("ShowGettingsStartedWizard", false); aboutAndUserGuideWizard = nullptr;});
        connect(aboutAndUserGuideWizard->button(QWizard::CancelButton), &QPushButton::clicked, this,
                [this]() {aboutAndUserGuideWizard = nullptr;});
    }

    connect(remoteCCDialog, SIGNAL (onConnStateChanged()), this, SLOT (onRemoteConnStateChanged()));
    //connect(streamingSettingsDialog, SIGNAL (onConnStateChanged()), this, SLOT (onStreamingConnStateChanged()));
    connect(streamingSettingsDialog, SIGNAL (onUDPConnect()), this, SLOT (onStreamingUDPConnect()));
    connect(streamingSettingsDialog, SIGNAL (onUDPDisconnect()), this, SLOT (onStreamingUDPDisconnect()));
    connect(streamingSettingsDialog, SIGNAL (onCOMConnect()), this, SLOT (onStreamingCOMConnect()));
    connect(streamingSettingsDialog, SIGNAL (onCOMDisconnect()), this, SLOT (onStreamingCOMDisconnect()));
#ifdef USE_LSL
    connect(streamingSettingsDialog, SIGNAL (onLSLConnect()), this, SLOT (onStreamingLSLConnect()));
    connect(streamingSettingsDialog, SIGNAL (onLSLDisconnect()), this, SLOT (onStreamingLSLDisconnect()));
#endif

    /*
    // if proc mode settings are not interpretable, reset them
    ProcMode pmSingle = (ProcMode)applicationSettings->value("PupilDetectionSettingsDialog.singleCam.procMode", ProcMode::SINGLE_IMAGE_ONE_PUPIL).toInt();
    ProcMode pmStereo = (ProcMode)applicationSettings->value("PupilDetectionSettingsDialog.stereoCam.procMode", ProcMode::STEREO_IMAGE_ONE_PUPIL).toInt();
    if( pmSingle != ProcMode::SINGLE_IMAGE_ONE_PUPIL ||
        pmSingle != ProcMode::SINGLE_IMAGE_TWO_PUPIL // ||
        // pmSingle != ProcMode::MIRR_IMAGE_ONE_PUPIL 
        ) {

        applicationSettings->setValue("PupilDetectionSettingsDialog.singleCam.procMode", ProcMode::SINGLE_IMAGE_ONE_PUPIL);
    }
    if( pmStereo != ProcMode::STEREO_IMAGE_ONE_PUPIL ||
        pmStereo != ProcMode::STEREO_IMAGE_TWO_PUPIL ) {

        applicationSettings->setValue("PupilDetectionSettingsDialog.stereoCam.procMode", ProcMode::STEREO_IMAGE_ONE_PUPIL);
    }
    */

#ifdef Q_OS_MACOS // Q_OS_WIN
    //subjectSelectionDialog->setWindowFlags(Qt::Tool);
    MCUSettingsDialogInst->setWindowFlags(Qt::Tool);
    remoteCCDialog->setWindowFlags(Qt::Tool);
    streamingSettingsDialog->setWindowFlags(Qt::Tool);
    pupilDetectionSettingsDialog->setWindowFlags(Qt::Tool);
    generalSettingsDialog->setWindowFlags(Qt::Tool);
#endif

    // NOTE: No matter what we do, DragEnter never triggers if the appliction is running with administrator rights,
    //  on windows. This cannot be bypassed.
//    setAttribute( Qt::WA_AcceptDrops, false );
//    setAttribute( Qt::WA_AcceptDrops, true );

//    for (auto *w : QApplication::allWidgets()) {
//        w->setAcceptDrops(true);
//        w->installEventFilter(this);
//    }

    setAcceptDrops(true);
//    this->centralWidget()->setAcceptDrops(true);
//    for (auto *w : findChildren<QWidget*>())
//        w->setAcceptDrops(true);
//    qApp->installEventFilter(this);
//    //this->installEventFilter(this); // NOTE: NO USE TO ADD. IT IS ONLY USED FOR ADDING TO CHILDREN. REALLY.

    qDebug() << "Window hints set currently:";
    qDebug() << this->windowFlags();


    playbackSynchroniser = nullptr;
}

void MainWindow::loadIcons() {
    fileOpenIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/document-open.svg"), applicationSettings);
    exportRecSectionIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/tool-animator.svg"), applicationSettings);
//    cameraSerialConnectionIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/rs232.svg"), applicationSettings);
    cameraSerialConnectionIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/show-gpu-effects.svg"), applicationSettings);
    pupilDetectionSettingsIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/draw-circle.svg"), applicationSettings);
    setupGeometryIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/setupGeometry.svg"), applicationSettings);
    remoteCCIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/computer-connection.svg"), applicationSettings);
    generalSettingsIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/mimetypes/16/application-x-sharedlib.svg"), applicationSettings);
    singleCameraIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/devices/22/camera-video.svg"), applicationSettings);
    stereoCameraIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/camera-video-stereo.svg"), applicationSettings);
    cameraSettingsIcon1 = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/configure.svg"), applicationSettings);
    cameraSettingsIcon2 = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/configure.svg"), applicationSettings);
//    calibrateIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/crosshairs.svg"), applicationSettings);
    calibrateIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/kdenlive-composite.svg"), applicationSettings);
    sharpnessIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/edit-select-all.svg"), applicationSettings);
    //subjectsIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/im-user.svg"), applicationSettings);
    outputDataFileIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/actions/22/edit-text-frame-update.svg"), applicationSettings);
    streamingSettingsIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/view-presentation.svg"), applicationSettings);
    imagePlaybackControlIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/run-build.svg"), applicationSettings);
    dataTableIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/table.svg"), applicationSettings);
    sceneImageViewIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/view-preview.svg"), applicationSettings);
    archiveIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/archive-extract.svg"), applicationSettings);
    videoFileIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/kdenlive-add-clip.svg"), applicationSettings);
}

void MainWindow::createActions() {

    QMenu *fileMenu = menuBar()->addMenu(tr("File"));

    // Note: made global to let is get disabled/enabled, whether there is already an opened directory or not
    fileOpenAct = fileMenu->addAction(tr("Open Image Recording"), this, &MainWindow::onOpenImageRecordingClicked);
    fileOpenAct->setIcon(fileOpenIcon);
    fileOpenAct->setIconVisibleInMenu(true);
    fileOpenAct->setStatusTip(tr("Open Image Recording for Playback. Single and Stereo Mode supported."));
    fileMenu->addAction(fileOpenAct);

    exportRecSectionAct = fileMenu->addAction(tr("Export Recording Section"), this, &MainWindow::onExportRecSectionClicked);
    exportRecSectionAct->setIcon(exportRecSectionIcon);
    exportRecSectionAct->setIconVisibleInMenu(true);
    exportRecSectionAct->setStatusTip(tr("Export a section of an existing Image Recording to animated .gif for presentation."));
    fileMenu->addAction(exportRecSectionAct);
    exportRecSectionAct->setEnabled(false);

    fileMenu->addSeparator();

    QAction *exitAct = fileMenu->addAction(tr("E&xit"), qApp, &QApplication::closeAllWindows);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setIconVisibleInMenu(true);
    exitAct->setStatusTip(tr("Exit the application"));
    fileMenu->addAction(exitAct);

    QMenu *viewMenu = menuBar()->addMenu(tr("View"));

    // temporarily removed this menu as it makes reaching items longer, and (yet) there are not so many different windows to reserve a separate submenu for them
    // formerly the camera view and data table windows were openable from there
//    QMenu *addWindowsMenu = viewMenu->addMenu(tr("Windows"));

    cameraViewAct = new QAction(singleCameraIcon, tr("Camera View Window"));
    connect(cameraViewAct, SIGNAL(triggered()), this, SLOT(cameraViewClick()));
    cameraViewAct->setIconVisibleInMenu(true);
    viewMenu->addAction(cameraViewAct);
    //
    dataTableAct = new QAction(dataTableIcon, tr("Data Table Window"));
    connect(dataTableAct, SIGNAL(triggered()), this, SLOT(dataTableClick()));
    dataTableAct->setIconVisibleInMenu(true);
    viewMenu->addAction(dataTableAct);
    //
    cameraViewAct->setEnabled(false);
    dataTableAct->setEnabled(false);

#ifdef QT_DEBUG
    sceneImageViewAct = new QAction(sceneImageViewIcon, tr("Scene Image View Window"));
    connect(sceneImageViewAct, SIGNAL(triggered()), this, SLOT(sceneImageViewClick()));
    sceneImageViewAct->setIconVisibleInMenu(true);
    viewMenu->addAction(sceneImageViewAct);
#endif

    viewMenu->addSeparator();
    toggleFullscreenAct = viewMenu->addAction(tr("Toggle Fullscreen"), this, &MainWindow::toggleFullscreen);
    toggleFullscreenAct->setIconVisibleInMenu(true);
    toggleFullscreenAct->setCheckable(true);
    toggleFullscreenAct->setChecked(this->isMaximized());
    //viewMenu->addAction(tr("Switch layout direction"), this, &MainWindow::switchLayoutDirection);

    QMenu *settingsMenu = menuBar()->addMenu(tr("Settings"));
    QAction *cameraSerialConnectionAct = new QAction(cameraSerialConnectionIcon, tr("Microcontroller Connection"));
    cameraSerialConnectionAct->setIconVisibleInMenu(true);
    connect(cameraSerialConnectionAct, SIGNAL(triggered()), MCUSettingsDialogInst, SLOT(show()));
    settingsMenu->addAction(cameraSerialConnectionAct);
    //
    QAction *pupilDetectionSettingsAct = new QAction(pupilDetectionSettingsIcon, tr("Pupil Detection"));
    pupilDetectionSettingsAct->setIconVisibleInMenu(true);
    connect(pupilDetectionSettingsAct, SIGNAL(triggered()), pupilDetectionSettingsDialog, SLOT(show()));
    settingsMenu->addAction(pupilDetectionSettingsAct);
    //
#ifdef QT_DEBUG
    QAction *setupGeometryAct = new QAction(setupGeometryIcon, tr("Setup Geometry"));
    setupGeometryAct->setIconVisibleInMenu(true);
    connect(setupGeometryAct, SIGNAL(triggered()), setupGeometryDialog, SLOT(show()));
    settingsMenu->addAction(setupGeometryAct);
    //
#endif
    QAction *remoteCCAct = new QAction(remoteCCIcon, tr("Remote Control Connection"));
    remoteCCAct->setIconVisibleInMenu(true);
    connect(remoteCCAct, SIGNAL(triggered()), remoteCCDialog, SLOT(show()));
    settingsMenu->addAction(remoteCCAct);
    //
    QAction *settingsAct = new QAction(generalSettingsIcon, tr("General Settings"));
    connect(settingsAct, SIGNAL(triggered()), generalSettingsDialog, SLOT(show()));
    settingsAct->setIconVisibleInMenu(true);
    settingsMenu->addAction(settingsAct);

    windowMenu = menuBar()->addMenu(tr("Windows"));
    connect(windowMenu, &QMenu::aboutToShow, this, &MainWindow::updateWindowMenu);

    menuBar()->addSeparator();

    QMenu *helpMenu = menuBar()->addMenu(tr("Help"));
    QAction *userGuideAct = helpMenu->addAction(tr("Open User Guide"), this, &MainWindow::userGuide);
    userGuideAct->setIcon(SVGIconColorAdjuster::loadAndAdjustColors(":/icons/Breeze/actions/22/question.svg",applicationSettings));
    userGuideAct->setIconVisibleInMenu(true);
    userGuideAct->setStatusTip(tr("Show a brief user guide"));
    helpMenu->addSeparator();
    QAction *openSourceAct = helpMenu->addAction(tr("Show Open Source Licenses"), this, &MainWindow::openSourceDialog);
    openSourceAct->setIcon(SVGIconColorAdjuster::loadAndAdjustColors(":/icons/Breeze/actions/16/license.svg",applicationSettings));
    openSourceAct->setIconVisibleInMenu(true);
    openSourceAct->setStatusTip(tr("Show the application's open source usages."));
    QAction *aboutAct = helpMenu->addAction(tr("About"), this, &MainWindow::about);
    aboutAct->setIcon(SVGIconColorAdjuster::loadAndAdjustColors(":/icons/Breeze/actions/16/help-about.svg",applicationSettings));
    aboutAct->setIconVisibleInMenu(true);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    helpMenu->addSeparator();
    QAction *clearPersistenceAct = helpMenu->addAction(tr("Reset application settings"), this, &MainWindow::offerResetApplicationSettings);
    clearPersistenceAct->setIcon(SVGIconColorAdjuster::loadAndAdjustColors(":/icons/Breeze/actions/16/edit-clear-history.svg",applicationSettings));
    clearPersistenceAct->setIconVisibleInMenu(true);
    clearPersistenceAct->setStatusTip(tr("Reset all application settings to factory defaults"));

    toolBar = new QToolBar(); // addToolBar(tr("Toolbar"));
    toolBar->setStyleSheet("QToolBar{spacing:10px;}");
    toolBar->setMovable(true); // Makes the toolbar moveable by the user, default is set ot left side
    toolBar->setFloatable(false); // Sets the toolbar as its own window
    toolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    menuBar()->setContextMenuPolicy(Qt::PreventContextMenu);

    addToolBar(Qt::LeftToolBarArea, toolBar); // Add the toolbar to the window, on the left side initially

    cameraAct = new QAction(singleCameraIcon, tr("Camera"), this);
    cameraAct->setIconVisibleInMenu(true);
    cameraAct->setStatusTip(tr("Connect to camera(s)."));
    cameraMenu = new QMenu(this);

    QWidget *cameraInfoWidget = new QWidget();
    QHBoxLayout *cameraInfoLayout = new QHBoxLayout();
    cameraInfoLayout->setContentsMargins(8,4,8,4);
#ifdef USE_PYLON
    QLabel *cameraInfoLabel = new QLabel("Connect Basler device:");
#else
    QLabel *cameraInfoLabel = new QLabel("Connect Aravis device:");
#endif
    cameraInfoLayout->addWidget(cameraInfoLabel);
    cameraInfoWidget->setLayout(cameraInfoLayout);

    QWidgetAction *bact1 = new QWidgetAction(cameraMenu);
    bact1->setCheckable(false);
    bact1->setIconVisibleInMenu(true);
    bact1->setDefaultWidget(cameraInfoWidget);
    cameraMenu->addAction(bact1);

    singleCamerasMenu = new QMenu(tr("&Single Camera"));
    singleCamerasMenu->setIcon(singleCameraIcon);
    //singleCamerasMenu->setIconVisibleInMenu(true); // TODO: does not exist, but there is no other waz to set this on macos.. what now?
    cameraMenu->addMenu(singleCamerasMenu);
    updateSingleCamerasMenu();
    connect(singleCamerasMenu, SIGNAL(triggered(QAction *)), this, SLOT(singleCameraSelected(QAction *)));
    connect(singleCamerasMenu, SIGNAL(aboutToShow()), this, SLOT(updateSingleCamerasMenu()));

    MouseLeaveCatchFilter *mlcf = new MouseLeaveCatchFilter(this);
    singleCamerasMenu->installEventFilter(mlcf);
    MouseLeaveCatchFilter *mlcf2 = new MouseLeaveCatchFilter(this);
    cameraMenu->installEventFilter(mlcf2);

    QAction *stereoCameraAct = cameraMenu->addAction(stereoCameraIcon, tr("Stereo Camera"), this, &MainWindow::stereoCameraSelected);
    stereoCameraAct->setIconVisibleInMenu(true);

    // DEV: yet stereo support is only for Pylon
#ifndef USE_PYLON
    stereoCameraAct->setEnabled(false);
#endif

    // updateBaslerCamerasMenu // Rather just check upon each new menu opening: this is just more convenient (see above)
//    cameraMenu->addSeparator();
//    cameraMenu->addAction(SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/refactor.svg"), applicationSettings), tr("Refresh Devices"), this, &MainWindow::updateBaslerCamerasMenu);

    cameraMenu->addSeparator();

    QWidget *webcamInfoWidget = new QWidget();
    QHBoxLayout *webcamInfoLayout = new QHBoxLayout();
    webcamInfoLayout->setContentsMargins(8,4,8,4);
    QLabel *webcamInfoLabel = new QLabel("Connect single OpenCV webcam:");
    webcamInfoLayout->addWidget(webcamInfoLabel);
    webcamInfoWidget->setLayout(webcamInfoLayout);

    QWidgetAction *wact1 = new QWidgetAction(cameraMenu);
    wact1->setCheckable(false);
    wact1->setIconVisibleInMenu(true);
    wact1->setDefaultWidget(webcamInfoWidget);
    cameraMenu->addAction(wact1);

    QWidget *webcamDeviceWidget = new QWidget();
    QHBoxLayout *webcamDeviceLayout = new QHBoxLayout();
    webcamDeviceLayout->setContentsMargins(8,0,8,4);
    QLabel *webcamDeviceLabel = new QLabel("Device ID:");
    webcamDeviceLabel->setFixedWidth(60);
    webcamDeviceBox = new QSpinBox();
    webcamDeviceBox->setMinimum(0);
    webcamDeviceBox->setMaximum(64);
    webcamDeviceBox->setSingleStep(1);
    webcamDeviceBox->setValue(0);
    QPushButton *webcamDeviceButton = new QPushButton("Connect");
    connect(webcamDeviceButton, &QPushButton::clicked, this, [this](){QAction *action = new QAction(); action->setData(webcamDeviceBox->value()); singleWebcamSelected(action);});

    webcamDeviceLayout->addWidget(webcamDeviceLabel);
    webcamDeviceLayout->addWidget(webcamDeviceBox);
    webcamDeviceLayout->addWidget(webcamDeviceButton);
    webcamDeviceWidget->setLayout(webcamDeviceLayout);

    QWidgetAction *wact2 = new QWidgetAction(cameraMenu);
    wact2->setCheckable(false);
    wact2->setIconVisibleInMenu(true);
    wact2->setDefaultWidget(webcamDeviceWidget);
    cameraMenu->addAction(wact2);

    /*
    openCVCamerasMenu = cameraMenu->addMenu(QIcon(":/icons/OpenCV.svg"), tr("&Single Webcam (OpenCV UVC)")); // icons/Breeze/devices/22/camera-web.svg
    updateOpenCVCamerasMenu();
    connect(openCVCamerasMenu, SIGNAL(triggered(QAction *)), this, SLOT(singleWebcamSelected(QAction *)));
    connect(openCVCamerasMenu, SIGNAL(aboutToShow()), this, SLOT(updateOpenCVCamerasMenu()));
    */

    cameraAct->setMenu(cameraMenu);
    connect(cameraAct, &QAction::triggered, this, &MainWindow::onCameraClick);
    //fileMenu->addAction(newAct);
    toolBar->addAction(cameraAct);

    const QIcon disconnectIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/network-disconnect.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
    cameraActDisconnectAct = new QAction(disconnectIcon, tr("Disconnect"), this);
    cameraActDisconnectAct->setIconVisibleInMenu(true);
    //trackAct->setShortcuts(QKeySequence::New);
    cameraActDisconnectAct->setStatusTip(tr("Disconnect camera."));
    connect(cameraActDisconnectAct, &QAction::triggered, this, &MainWindow::onCameraDisconnectClick);
    cameraActDisconnectAct->setDisabled(true);
    //fileMenu->addAction(newAct);
    toolBar->addAction(cameraActDisconnectAct);

    cameraSettingsAct = new QAction(cameraSettingsIcon1, tr("Camera Settings"), this);
    cameraSettingsAct->setIconVisibleInMenu(true);
    //trackAct->setShortcuts(QKeySequence::New);
    cameraSettingsAct->setStatusTip(tr("Camera settings."));
    connect(cameraSettingsAct, &QAction::triggered, this, &MainWindow::onCameraSettingsClick);
    cameraSettingsAct->setDisabled(true);
    settingsMenu->addAction(cameraSettingsAct);
    toolBar->addAction(cameraSettingsAct);

    // NOTE: these should have come before, when the menu actions are defined, but as these whould come after cameraSettingsAct, I put them down here
    settingsMenu->addSeparator();

    const QIcon forceResetTrialIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/equals1b.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
    forceResetTrialAct = new QAction(forceResetTrialIcon, tr("Force reset trial counter"), this);
    forceResetTrialAct->setIconVisibleInMenu(true);
    forceResetTrialAct->setEnabled(false);
    //connect(forceResetTrialAct, &QAction::triggered, this, &MainWindow::forceResetTrialCounter);
    connect(forceResetTrialAct, SIGNAL(triggered()), this, SLOT(forceResetTrialCounter()));
    settingsMenu->addAction(forceResetTrialAct);

    const QIcon manualIncTrialIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/plus1b.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
    manualIncTrialAct = new QAction(manualIncTrialIcon, tr("Manually increment trial counter"), this);
    manualIncTrialAct->setIconVisibleInMenu(true);
    manualIncTrialAct->setEnabled(false);
    connect(manualIncTrialAct, SIGNAL(triggered()), this, SLOT(incrementTrialCounter()));
    settingsMenu->addAction(manualIncTrialAct);

    settingsMenu->addSeparator();

    const QIcon forceResetMessageIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/messageEmpty.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
    forceResetMessageAct = new QAction(forceResetMessageIcon, tr("Force reset message register"), this);
    forceResetMessageAct->setIconVisibleInMenu(true);
    forceResetMessageAct->setEnabled(false);
    //connect(forceResetMessageAct, &QAction::triggered, this, &MainWindow::forceResetMessageRegister);
    connect(forceResetMessageAct, SIGNAL(triggered()), this, SLOT(forceResetMessageRegister()));
    settingsMenu->addAction(forceResetMessageAct);

    toolBar->addSeparator();

    const QIcon trackOffIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/view-visible.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
    trackAct = new QAction(trackOffIcon, tr("Track"), this);
    trackAct->setCheckable(true);
    //trackAct->setShortcuts(QKeySequence::New);
    trackAct->setStatusTip(tr("Start/Stop pupil tracking."));
    connect(trackAct, &QAction::triggered, this, &MainWindow::onTrackActClick);
    trackAct->setDisabled(true);
    //fileMenu->addAction(newAct);
    toolBar->addAction(trackAct);

    toolBar->addSeparator();

    calibrateAct = new QAction(calibrateIcon, tr("Camera calibration for lens undistortion and px-mm mapping"), this);
    calibrateAct->setStatusTip(tr("Start camera calibration for lens undistortion and px-mm mapping."));
    connect(calibrateAct, &QAction::triggered, this, &MainWindow::onCalibrateClick);
    //fileMenu->addAction(newAct);
    toolBar->addAction(calibrateAct);
    calibrateAct->setDisabled(true);

    sharpnessAct = new QAction(sharpnessIcon, tr("Sharpness validation of camera image (single only)"), this);
    sharpnessAct->setStatusTip(tr("Start sharpness validation of camera image (single only)."));
    connect(sharpnessAct, &QAction::triggered, this, &MainWindow::onSharpnessClick);
    //fileMenu->addAction(newAct);
    toolBar->addAction(sharpnessAct);
    sharpnessAct->setDisabled(true);

    /*
    subjectsAct = new QAction(subjectsIcon, tr("Subjects"), this);
    subjectsAct->setStatusTip(tr("Load subject-specific pupil detection configurations."));
    connect(subjectsAct, &QAction::triggered, this, &MainWindow::onSubjectsClick);
    //fileMenu->addAction(newAct);
    toolBar->addAction(subjectsAct);
    subjectsAct->setDisabled(true);
     */

    toolBar->addSeparator();

    logFileAct = new QAction(outputDataFileIcon, tr("Output data file"), this);
    logFileAct->setStatusTip(tr("Set output data file path and name."));
    connect(logFileAct, &QAction::triggered, this, &MainWindow::setLogFile);
    //fileMenu->addAction(newAct);
    toolBar->addAction(logFileAct);
    logFileAct->setDisabled(true);


    const QIcon recordIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/media-record.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
    recordAct = new QAction(recordIcon, tr("Record"), this);
    recordAct->setStatusTip(tr("Start pupil recording."));
    connect(recordAct, &QAction::triggered, this, &MainWindow::onRecordClick);
    //fileMenu->addAction(newAct);
    toolBar->addAction(recordAct);
    recordAct->setDisabled(true);


    toolBar->addSeparator();

    /*
    outputDirectoryAct = new QAction(fileOpenIcon, tr("Output Directory"), this);
    outputDirectoryAct->setStatusTip(tr("Set output directory."));
    connect(outputDirectoryAct, &QAction::triggered, this, &MainWindow::setOutputDirectory);
    //fileMenu->addAction(newAct);
    toolBar->addAction(outputDirectoryAct);
    outputDirectoryAct->setDisabled(true);
    */
    imageRecordingOutputAct = new QAction(fileOpenIcon, tr("Image Recording Output"), this);
    imageRecordingOutputAct->setIconVisibleInMenu(true);
    imageRecordingOutputAct->setStatusTip(tr("Set where the images should be recorded."));
    QMenu* imageRecordingOutputMenu = new QMenu(this);

    QAction *iaAct = imageRecordingOutputMenu->addAction(fileOpenIcon, tr("Directory"), this, &MainWindow::imageRecordingOutputDirectorySelected);
    iaAct->setIconVisibleInMenu(true);
    //imageRecordingOutputMenu->addSeparator();
    QAction *izAct = imageRecordingOutputMenu->addAction(archiveIcon, tr("Zip archive"), this, &MainWindow::imageRecordingOutputZipSelected);
    izAct->setIconVisibleInMenu(true);
    //imageRecordingOutputMenu->addSeparator();
#ifdef QT_DEBUG
    QAction *ivAct = imageRecordingOutputMenu->addAction(videoFileIcon, tr("Video file"), this, &MainWindow::imageRecordingOutputVideoSelected);
    ivAct->setIconVisibleInMenu(true);
#endif

    imageRecordingOutputAct->setMenu(imageRecordingOutputMenu);
    connect(imageRecordingOutputAct, &QAction::triggered, this, &MainWindow::onImageRecordingOutputClick);
    toolBar->addAction(imageRecordingOutputAct);
    imageRecordingOutputAct->setDisabled(true);

















    const QIcon recordImagesIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/media-record-blue.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
    recordImagesAct = new QAction(recordImagesIcon, tr("Record Images"), this);
    recordImagesAct->setStatusTip(tr("Start image recording."));
    connect(recordImagesAct, &QAction::triggered, this, &MainWindow::onRecordImageClick);
    //fileMenu->addAction(newAct);
    toolBar->addAction(recordImagesAct);
    recordImagesAct->setDisabled(true);

    toolBar->addSeparator();

    streamingSettingsAct = new QAction(streamingSettingsIcon, tr("Streaming settings"), this);
    streamingSettingsAct->setStatusTip(tr("Settings for data streaming."));
    connect(streamingSettingsAct, &QAction::triggered, this, &MainWindow::onStreamingSettingsClick);
    //fileMenu->addAction(newAct);
    toolBar->addAction(streamingSettingsAct);
    //streamingSettingsAct->setDisabled(true);
//    streamingSettingsAct->setDisabled(true); // This should be enabled even if no camera is connected. It is like remote control conn settings dialog

    const QIcon streamIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/media-record-green.svg"), applicationSettings);
    streamAct = new QAction(streamIcon, tr("Stream"), this);
    streamAct->setStatusTip(tr("Stream pupil detection output."));
    connect(streamAct, &QAction::triggered, this, &MainWindow::onStreamClick);
    //fileMenu->addAction(newAct);
    toolBar->addAction(streamAct);
    streamAct->setDisabled(true);

    // This causes problems because it cannot leave the pointers of the closed windows
    // (or their encapsulated dialog instances) in nullptr state, thus we cannot check for their state later..
    // the easiest is now to disable them until there is a better solution.
    // However this functionality is hardly ever used, so it is not really important
//    closeAct = new QAction(tr("Cl&ose"), this);
//    closeAct->setStatusTip(tr("Close the active window"));
//    connect(closeAct, &QAction::triggered, this, &MainWindow::closeActiveSubWindow);
//
//    closeAllAct = new QAction(tr("Close &All"), this);
//    closeAllAct->setStatusTip(tr("Close all the windows"));
//    connect(closeAllAct, &QAction::triggered, this, &MainWindow::closeAllSubWindows);

    // This really jams up the GUI so disabled this option
//    tileAct = new QAction(tr("&Tile"), this);
//    tileAct->setStatusTip(tr("Tile the windows"));
//    connect(tileAct, &QAction::triggered, mdiArea, &QMdiArea::tileSubWindows);

    cascadeAct = new QAction(tr("&Cascade"), this);
    cascadeAct->setStatusTip(tr("Cascade the windows"));
    connect(cascadeAct, &QAction::triggered, mdiArea, &QMdiArea::cascadeSubWindows);

    resetGeometryAct = new QAction(tr("&Reset Interface"), this);
    resetGeometryAct->setStatusTip(tr("Reset the position and size of the windows"));
    connect(resetGeometryAct, &QAction::triggered, this, &MainWindow::resetGeometry);

    nextAct = new QAction(tr("Ne&xt"), this);
    nextAct->setShortcuts(QKeySequence::NextChild);
    nextAct->setStatusTip(tr("Move the focus to the next window"));
    connect(nextAct, &QAction::triggered, mdiArea, &QMdiArea::activateNextSubWindow);

    previousAct = new QAction(tr("Pre&vious"), this);
    previousAct->setShortcuts(QKeySequence::PreviousChild);
    previousAct->setStatusTip(tr("Move the focus to the previous window"));
    connect(previousAct, &QAction::triggered, mdiArea, &QMdiArea::activatePreviousSubWindow);

    windowMenuSeparatorAct = new QAction(this);
    windowMenuSeparatorAct->setSeparator(true);

    updateWindowMenu();



    
}

void MainWindow::createStatusBar() {

    //statusBar()->setMaximumHeight(20);
    //statusBar()->layout()->setContentsMargins(0,0,0,0);
    //statusBar()->layout()->setSpacing(0);
    //statusBar()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    QWidget *widget = new QWidget();
    QHBoxLayout *statusBarLayout = new QHBoxLayout(widget);
    statusBarLayout->setContentsMargins(8,0,8,0);

    trialWidget = new QWidget();
    QHBoxLayout *trialWidgetLayout = new QHBoxLayout(trialWidget);
    trialWidgetLayout->setContentsMargins(8,0,8,0);
    QLabel *trialLabel = new QLabel("Trial: ");
    currentTrialLabel = new QLabel();
    trialWidgetLayout->addWidget(trialLabel);
    trialWidgetLayout->addWidget(currentTrialLabel);
    updateCurrentTrialLabel();
    trialWidget->setVisible(false);

    messageWidget = new QWidget();
    QHBoxLayout *messageWidgetLayout = new QHBoxLayout(messageWidget);
    messageWidgetLayout->setContentsMargins(8,0,8,0);
    QLabel *messageLabel = new QLabel("Message: ");
    currentMessageLabel = new QLabel();
    messageWidgetLayout->addWidget(messageLabel);
    messageWidgetLayout->addWidget(currentMessageLabel);
    updateCurrentMessageLabel();
    messageWidget->setVisible(false);

    QLabel *remoteLabel = new QLabel("Remote Control Conn.");
    const QIcon remoteIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/actions/22/media-record.svg"), applicationSettings);
    remoteStatusIcon = new QLabel();
    remoteStatusIcon->setPixmap(remoteIcon.pixmap(16, 16));
    remoteStatusIcon->setToolTip("Remote control connection is not established");

    QLabel *calibrationLabel = new QLabel("Camera Calibration");
    const QIcon calibrationIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/actions/22/media-record.svg"), applicationSettings);
    calibrationStatusIcon = new QLabel();
    calibrationStatusIcon->setPixmap(calibrationIcon.pixmap(16, 16));
    calibrationStatusIcon->setToolTip("Camera calibration is not loaded");

    QLabel *serialLabel = new QLabel("Microcontroller Conn.");
    const QIcon offlineIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/actions/22/media-record.svg"), applicationSettings);
    serialStatusIcon = new QLabel();
    serialStatusIcon->setPixmap(offlineIcon.pixmap(16, 16));
    serialStatusIcon->setToolTip("Microcontroller connection is not established");

    QLabel *hwTriggerLabel = new QLabel("Hardware Trigger");
    hwTriggerStatusIcon = new QLabel();
    hwTriggerStatusIcon->setPixmap(offlineIcon.pixmap(16, 16));
    hwTriggerStatusIcon->setToolTip("Hardware triggering is not running");

    QLabel *warmedUpLabel = new QLabel("Warmup");
    warmedUpStatusIcon = new QLabel();
    warmedUpStatusIcon->setPixmap(offlineIcon.pixmap(16, 16));
    warmedUpStatusIcon->setEnabled(false);
    warmedUpStatusIcon->setToolTip("Warmup state indication will appear here");

    QLabel *versionLabel = new QLabel(QCoreApplication::applicationVersion());

    QFrame* sep1 = new QFrame();
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFrameShadow(QFrame::Plain);
    QFrame* sep2 = new QFrame();
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFrameShadow(QFrame::Plain);
    QFrame* sep3 = new QFrame();
    sep3->setFrameShape(QFrame::VLine);
    sep3->setFrameShadow(QFrame::Plain);
    QFrame* sep4 = new QFrame();
    sep4->setFrameShape(QFrame::VLine);
    sep4->setFrameShadow(QFrame::Plain);
    QFrame* sep5 = new QFrame();
    sep5->setFrameShape(QFrame::VLine);
    sep5->setFrameShadow(QFrame::Plain);

    trialWidgetLayoutSep = new QFrame();
    trialWidgetLayoutSep->setFrameShape(QFrame::VLine);
    trialWidgetLayoutSep->setFrameShadow(QFrame::Plain);
    trialWidgetLayoutSep->setVisible(false);
    messageWidgetLayoutSep = new QFrame();
    messageWidgetLayoutSep->setFrameShape(QFrame::VLine);
    messageWidgetLayoutSep->setFrameShadow(QFrame::Plain);
    messageWidgetLayoutSep->setVisible(false);

    statusBarLayout->addWidget(trialWidget);
    statusBarLayout->addWidget(trialWidgetLayoutSep);
    statusBarLayout->addWidget(messageWidget);
    statusBarLayout->addWidget(messageWidgetLayoutSep);

    statusBarLayout->addWidget(remoteLabel);
    statusBarLayout->addWidget(remoteStatusIcon);
    statusBarLayout->addWidget(sep1);
    statusBarLayout->addWidget(calibrationLabel);
    statusBarLayout->addWidget(calibrationStatusIcon);
    statusBarLayout->addWidget(sep2);
    statusBarLayout->addWidget(serialLabel);
    statusBarLayout->addWidget(serialStatusIcon);
    statusBarLayout->addWidget(sep3);
    statusBarLayout->addWidget(hwTriggerLabel);
    statusBarLayout->addWidget(hwTriggerStatusIcon);
    statusBarLayout->addWidget(sep4);
    statusBarLayout->addWidget(warmedUpLabel);
    statusBarLayout->addWidget(warmedUpStatusIcon);

    statusBarLayout->addWidget(sep5);
    statusBarLayout->addWidget(versionLabel);

    statusBar()->addPermanentWidget(widget);

    //subjectConfigurationLabel = new QLabel("");
    //statusBar()->addWidget(subjectConfigurationLabel);

    currentStatusMessageLabel = new QLabel("");
    statusBar()->addWidget(currentStatusMessageLabel);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    onCameraDisconnectClick();

    mdiArea->closeAllSubWindows();
    if (mdiArea->currentSubWindow()) {
        event->ignore();
    } else {
        writeSettings();
        event->accept();
    }
}

void MainWindow::changeEvent(QEvent *event) {
    if(event->type() == QEvent::Type::WindowStateChange)
        toggleFullscreenAct->setChecked(this->isMaximized());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F){
        Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
        if (modifiers == Qt::ShiftModifier){
            onCameraFreezePressed();
        }
        else
            QWidget::keyPressEvent(event);
    }
    else {
        QWidget::keyPressEvent(event);
    }
}

void MainWindow::onCameraFreezePressed()
{
    emit cameraPlaybackChanged();
}

// TODO: remove. Playback is not anymore managed by mainwindow,
//  but playback control dialog. However, the usages of cameraPlaying bool should be
//  precisely removed/changed everywhere, so yet I left this here.
//  However, for the special case when someone hits Freeze, this is now run. Could be changed to a cleaner solution
void MainWindow::onCameraPlaybackChanged() {
    if (!selectedCamera)
        return;
    if (selectedCamera->getType() != STEREO_IMAGE_FILE && selectedCamera->getType() != SINGLE_IMAGE_FILE) {
        if (selectedCamera->isGrabbing()) {
            selectedCamera->stopGrabbing();
            qInfo() << "Camera Freeze: on";
        } else {
            selectedCamera->startGrabbing();
            qInfo() << "Camera Freeze: off";
        }
    }
    cameraPlaying = !cameraPlaying;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {

    ////QWidget *w = qobject_cast<QWidget*>(obj);
    //qDebug() << event->type();

//    // NOTE: Yet we have separate overridden listeners for this. Change if necessary
//    if (event->type() == QEvent::DragEnter) {
//        qDebug() << "DragEnter caught!";
//    }

    if(
            obj == singleCameraSettingsDialog ||
            obj == stereoCameraSettingsDialog ||
            obj == singleCameraChildWidget ||
            obj == stereoCameraChildWidget ||
            (singleCameraSettingsDialog && singleCameraSettingsDialog->hasFocus()) ||
            (stereoCameraSettingsDialog && stereoCameraSettingsDialog->hasFocus() ) ||
            (singleCameraChildWidget && singleCameraChildWidget->hasFocus() ) ||
            (stereoCameraChildWidget && stereoCameraChildWidget->hasFocus() )
            ) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

            //qDebug() << "Keypress: " << keyEvent->key();

            if (keyEvent->key() == Qt::Key_F){
                keyPressEvent(keyEvent);
                return true;
            } else if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return){
//                keyPressEvent(keyEvent);
                // TODO: add special cases for when specific numeric edit boxes are in focus, in camera settings dialogs,
                //  so that enter swaps between them. Right now the enter is just caught (not to cause trouble) and discarded
                return true;
            }  else if (keyEvent->key() == Qt::Key_Space){

                // This key always does something that the user did not want. Just discard the event.
                //  E.g. ticking a checkbox or changing a radiobutton, ..
                return true;

            } else {
                return false;
            }

        } else {
            return false;
        }
    }
    else
        return QObject::eventFilter(obj, event);
//    else {
//        if (event->type() == QEvent::DragEnter) {
//            QDragEnterEvent *de = static_cast<QDragEnterEvent*>(event);
//            //dragEnterEvent(de);
//            return true;
//        }
//        if (event->type() == QEvent::DragMove) {
//            QDragMoveEvent *de = static_cast<QDragMoveEvent*>(event);
//            //dragMoveEvent(de);
//            return true;
//        }
//        else if (event->type() == QEvent::Drop) {
//            QDropEvent *de = static_cast<QDropEvent*>(event);
//            //dropEvent(de);
//            return true;
//        }
//        else
//            return QObject::eventFilter(obj, event);
//    }
}

void MainWindow::about() {
    if(!aboutAndUserGuideWizard && !aboutWizard) {
        aboutWizard = new GettingStartedWizard(GettingStartedWizard::WizardPurpose::ABOUT_ONLY, this);
        connect(aboutWizard->button(QWizard::FinishButton), &QPushButton::clicked, this,
                [this]() {aboutWizard = nullptr;});
        connect(aboutWizard->button(QWizard::CancelButton), &QPushButton::clicked, this,
                [this]() {aboutWizard = nullptr;});
    }
    if(aboutWizard)
        aboutWizard->show();
}

void MainWindow::userGuide() {
    if(!aboutAndUserGuideWizard && !userGuideWizard) {
        userGuideWizard = new GettingStartedWizard(GettingStartedWizard::WizardPurpose::USERGUIDE_ONLY, this);
        connect(userGuideWizard->button(QWizard::FinishButton), &QPushButton::clicked, this,
                [this]() {userGuideWizard = nullptr;});
        connect(userGuideWizard->button(QWizard::CancelButton), &QPushButton::clicked, this,
                [this]() {userGuideWizard = nullptr;});
    }
    if(userGuideWizard)
        userGuideWizard->show();
}

void MainWindow::openSourceDialog() {

    // TODO: Show this automated somehow. This list should be kapt at one place at one time, not in
    //  two different files, and also this source file! (the .md, the .html, and this .cpp)

    QDialog *dialog = new QDialog(this);
    dialog->resize(500, 300);
    dialog->setWindowTitle("Open Source Contributions and Licenses");

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);

    QVBoxLayout *l = new QVBoxLayout();

    QScrollArea *scroll = new QScrollArea();
    l->addWidget(scroll);
    l->addWidget(buttonBox);

    QLabel *label = new QLabel();
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    label->setWordWrap(true);

    label->setText(
            "Wolfgang Fuhl, Thiago Santini, Thomas Kübler, Enkelejda Kasneci, \"ElSe: Ellipse Selection for Robust Pupil Detection in Real-World Environments.\", 2016<br/>Part of the <a href=\"https://www-ti.informatik.uni-tuebingen.de/santini/EyeRecToo\">EyeRecToo</a> software. Copyright (c) 2018, Thiago Santini / University of Tübingen<br><br>"
            "Wolfgang Fuhl, Thomas Kübler, Katrin Sippel, Wolfgang Rosenstiel, Enkelejda Kasneci, \"ExCuSe: Robust Pupil Detection in Real-World Scenarios.\", 2015<br/>Part of the <a href=\"https://www-ti.informatik.uni-tuebingen.de/santini/EyeRecToo\">EyeRecToo</a> software. Copyright (c) 2018, Thiago Santini / University of Tübingen<br><br>"
            "Thiago Santini, Wolfgang Fuhl, Enkelejda Kasneci, \"PuRe: Robust pupil detection for real-time pervasive eye tracking.\", 2018<br/>Part of the <a href=\"https://www-ti.informatik.uni-tuebingen.de/santini/EyeRecToo\">EyeRecToo</a> software. Copyright (c) 2018, Thiago Santini<br><br>"
            "Thiago Santini, Wolfgang Fuhl, Enkelejda Kasneci, \"PuReST: Robust Pupil Tracking for Real-Time Pervasive Eye.\", 2018<br/>Part of the <a href=\"https://www-ti.informatik.uni-tuebingen.de/santini/EyeRecToo\">EyeRecToo</a> software. Copyright (c) 2018, Thiago Santini<br><br>"
            "Li, Dongheng & Winfield, D. & Parkhurst, D.J., \"Starburst: A hybrid algorithm for video-based eye tracking combining feature-based and model-based approaches.\", 2005<br/>Part of the <a href=\"http://thirtysixthspan.com/openEyes/software.html\">cvEyeTracker</a> software License: <a href=\"https://www.gnu.org/licenses/gpl-3.0.txt\">GPL 3</a><br><br>"
            "Lech Swirski, Andreas Bulling, Neil A. Dodgson, \"Robust real-time pupil tracking in highly off-axis images\", 2012 <a href=\"http://www.cl.cam.ac.uk/research/rainbow/projects/pupiltracking\">Website</a><br/>License: <a href=\"https://opensource.org/licenses/MIT\">MIT</a><br><br>"
            "Aravis Library, License: <a href=\"https://github.com/AravisProject/aravis/blob/main/COPYING\">LGPL-2.1</a><br><br>"
            "Boost libraries, License: <a href=\"https://www.boost.org/LICENSE_1_0.txt\">Boost License</a><br><br>"
            "bzip2 Library, License: <a href=\"https://github.com/opencor/bzip2/blob/master/LICENSE\">bzip2 License</a><br><br>"
            "Ceres-Solver Library, License: <a href=\"https://github.com/ceres-solver/ceres-solver/blob/master/LICENSE\">Apache-2.0</a><br><br>"
            "Eigen Library, License: <a href=\"https://eigen.tuxfamily.org/index.php?title=Main_Page#License\">MPL2</a><br><br>"
            "Qt Framework, License: <a href=\"https://www.gnu.org/licenses/lgpl-3.0.txt\">LGPL v3</a><br><br>"
            "OpenCV Library, License: <a href=\"https://opencv.org/license/\">BSD 3-Clause</a><br><br>"
            "FFmpeg Library, License: <a href=\"https://github.com/FFmpeg/FFmpeg/blob/master/COPYING.LGPLv2.1\">LGPL v2.1</a><br><br>"
            "spii Library, License: <a href=\"https://github.com/PetterS/spii/blob/master/LICENSE\">BSD 2-Clause</a><br><br>"
            "TBB Library, License: <a href=\"https://www.apache.org/licenses/LICENSE-2.0.txt\">Apache 2.0</a><br><br>"
            "Gflags Library, License: <a href=\"https://opencv.org/license/\">BSD 3-Clause</a><br><br>"
            "Glog Library, License: <a href=\"https://github.com/google/glog/blob/master/COPYING\">Glog License</a><br><br>"
            "liblsl Library, License: <a href=\"https://github.com/sccn/liblsl/blob/dev/LICENSE\">liblsl License</a><br><br>"
            "QCustomPLot Library, License: <a href=\"https://www.gnu.org/licenses/gpl-3.0.txt\">GPL 3</a><br><br>"
            "QJsonModel, License: <a href=\"https://github.com/dridk/QJsonModel/blob/master/LICENSE\">MIT</a><br><br>"
            "QuaZip Library, License: <a href=\"https://github.com/stachenov/quazip/blob/master/COPYING\">QuaZip License</a><br><br>"
            "QtOpenGLViewer, License: <a href=\"https://github.com/marcel-goldschen-ohm/QtOpenGLViewer/blob/master/LICENSE\">MIT</a><br><br>"
            "Breeze Icon Theme, License: <a href=\"https://www.gnu.org/licenses/lgpl-3.0.txt\">LGPL v3</a><br><br>");

    scroll->setWidget(label);
    label->setOpenExternalLinks(true);
    dialog->setLayout(l);
    dialog->show();
}


void MainWindow::setLogFile() {

    QString pupilDetectionDataFileCandidate = QFileDialog::getSaveFileName(this, tr("Save Log File"), recentDataWritingDirectory, tr("CSV files (*.csv)"), nullptr, QFileDialog::DontConfirmOverwrite);
    if((pupilDetectionDataFileCandidate.isEmpty() || !QFileInfo(pupilDetectionDataFileCandidate).dir().exists()) && !dataRecordingOutputTarget.isEmpty()) {
        return;
    }
    if((pupilDetectionDataFileCandidate.isEmpty() || !QFileInfo(pupilDetectionDataFileCandidate).dir().exists()) && dataRecordingOutputTarget.isEmpty()) {
        recordAct->setDisabled(true);
        return;
    }

    dataRecordingOutputTarget = pupilDetectionDataFileCandidate;
    QFileInfo fileInfo(dataRecordingOutputTarget);
    setRecentDataWritingDirectory(fileInfo.dir().path());

    // check if filename has extension
    if(fileInfo.suffix().isEmpty()) {
        dataRecordingOutputTarget = dataRecordingOutputTarget + ".csv";
    }

    //QFile file(pupilDetectionDataFile);
    //file.open(QIODevice::WriteOnly); // Or QIODevice::ReadWrite
    //file.close();

    if(trackingOn)
        recordAct->setEnabled(!exportingRecSection);
}

void MainWindow::imageRecordingOutputDirectorySelected() {

    QString outputDirectoryCandidate = QFileDialog::getExistingDirectory(this, tr("Output Directory"), recentImageWritingDirectory);

    if(outputDirectoryCandidate.isEmpty() && !imageRecordingOutputTarget.isEmpty()) {
        return;
    }
    if(outputDirectoryCandidate.isEmpty() && imageRecordingOutputTarget.isEmpty()) {
        recordImagesAct->setDisabled(true);
        return;
    }

    imageRecordingOutputTarget = outputDirectoryCandidate;

    setRecentImageWritingDirectory(imageRecordingOutputTarget);
//    std::cout << recentPath.toStdString() << std::endl;
    currentStatusMessageLabel->setText("Image rec. target directory: " + SupportFunctions::shortenStringForDisplay(imageRecordingOutputTarget, 100));
    currentStatusMessageLabel->setToolTip(imageRecordingOutputTarget);

    recordImagesAct->setDisabled(false);
}

void MainWindow::imageRecordingOutputZipSelected() {

    QString filters("Zip archive (*.zip)");
    //QString filters("Zip archive (*.zip);;Any file (*.*)");
    //QString defaultFilter("Zip archive (*.zip)");

    // NOTE: The file name will be considered the recording (or participant) name
    // TODO: remember last/default path !!
    QFileDialog dialog(0, "Save file", recentImageWritingDirectory, filters);
    //dialog.selectNameFilter(defaultFilter);

    dialog.setOptions(QFileDialog::DontResolveSymlinks);

    // just in case it was erroneously left enabled from before
    if(imageRecordingOutputTarget.isEmpty()) {
        recordImagesAct->setDisabled(true);
    }

    if(!dialog.exec())
        return;

    if(dialog.selectedFiles().empty())
        return;

    auto bbbb = dialog.selectedNameFilter();

    QString selectedFilePathAndName = dialog.selectedFiles()[0];
    if(!selectedFilePathAndName.endsWith(".zip") && dialog.selectedNameFilter().contains(".zip"))
        selectedFilePathAndName.append(".zip");
    // TODO: Also pre-check if location can be written
    // TODO: At this point we can beautify the given save file name. Change strange characters in it, etc.

    imageRecordingOutputTarget = selectedFilePathAndName;

    qDebug() << QFileInfo(imageRecordingOutputTarget).dir().path();
    setRecentImageWritingDirectory(QFileInfo(imageRecordingOutputTarget).dir().path());
//    std::cout << recentPath.toStdString() << std::endl;
    currentStatusMessageLabel->setText("Image rec. target archive: " + SupportFunctions::shortenStringForDisplay(imageRecordingOutputTarget, 100));
    currentStatusMessageLabel->setToolTip(imageRecordingOutputTarget);

    recordImagesAct->setDisabled(false);
}

void MainWindow::imageRecordingOutputVideoSelected() {

    QString filters("Matroska Video Format (*.mkv)");

    // NOTE: The file name will be considered the recording (or participant) name
    // TODO: remember last/default path !!
    QFileDialog dialog(0, "Save file", recentImageWritingDirectory, filters);
    //dialog.selectNameFilter(defaultFilter);

    dialog.setOptions(QFileDialog::DontResolveSymlinks);

    // just in case it was erroneously left enabled from before
    if(imageRecordingOutputTarget.isEmpty()) {
        recordImagesAct->setDisabled(true);
    }

    // Yeah its weird and all, but works perfectly, and the user can easily escape with a cancel button
    videoSelecRetry:
    if(!dialog.exec())
        return;

    if(dialog.selectedFiles().empty())
        return;

    QString selectedFilePathAndName = dialog.selectedFiles()[0];

    if(QFile(selectedFilePathAndName).exists()) {
        QMessageBox *msgBox = new QMessageBox(this);
        msgBox->setWindowTitle("Existing file selected");
        msgBox->setText("You selected an already existing video file. In case of video recordings, appending to existing ones is not supported. Please specify a video target file that does not yet exist.");
        msgBox->setMinimumSize(330,240);
        msgBox->setIcon(QMessageBox::Warning);
        //msgBox->setModal(false);
        //msgBox->show();
        msgBox->setModal(true); // needs user to hit OK before showing directory selection dialog again
        msgBox->exec();

        goto videoSelecRetry;
    }

    // TODO: make this less spacey, and make proper

    if(!selectedFilePathAndName.endsWith(".mkv") && dialog.selectedNameFilter().contains(".mkv"))
        selectedFilePathAndName.append(".mkv");
    // TODO: Also pre-check if location can be written
    // TODO: At this point we can beautify the given save file name. Change strange characters in it, etc.

    imageRecordingOutputTarget = selectedFilePathAndName;

    qDebug() << QFileInfo(imageRecordingOutputTarget).dir().path();
    setRecentImageWritingDirectory(QFileInfo(imageRecordingOutputTarget).dir().path());
//    std::cout << recentPath.toStdString() << std::endl;
    currentStatusMessageLabel->setText("Image rec. target archive: " + SupportFunctions::shortenStringForDisplay(imageRecordingOutputTarget, 100));
    currentStatusMessageLabel->setToolTip(imageRecordingOutputTarget);

    recordImagesAct->setDisabled(false);
}

void MainWindow::updateMenus() {
    bool hasMdiChild = (activeMdiChild() != nullptr);

//    closeAct->setEnabled(hasMdiChild);
//    closeAllAct->setEnabled(hasMdiChild);
//    tileAct->setEnabled(hasMdiChild);
    cascadeAct->setEnabled(hasMdiChild);
    nextAct->setEnabled(hasMdiChild);
    previousAct->setEnabled(hasMdiChild);
    windowMenuSeparatorAct->setVisible(hasMdiChild);
}

void MainWindow::updateWindowMenu() {

    //std::cout<<"updateWindowMenu"<<std::endl;

    windowMenu->clear();
//    windowMenu->addAction(closeAct);
//    windowMenu->addAction(closeAllAct);
    windowMenu->addSeparator();
//    windowMenu->addAction(tileAct);
    windowMenu->addAction(cascadeAct);
    windowMenu->addSeparator();
    windowMenu->addAction(resetGeometryAct);
    windowMenu->addSeparator();
    windowMenu->addAction(nextAct);
    windowMenu->addAction(previousAct);
    windowMenu->addAction(windowMenuSeparatorAct);

    QList<QMdiSubWindow *> windows = mdiArea->subWindowList();
    windowMenuSeparatorAct->setVisible(!windows.isEmpty()); //

    for(auto mdiSubWindow : windows) {
        QWidget *child = mdiSubWindow->widget();

        QString text = child->windowTitle();
        QAction *action = windowMenu->addAction(text, mdiSubWindow, [this, mdiSubWindow]() {
            mdiArea->setActiveSubWindow(mdiSubWindow);
        });
        action->setCheckable(true);
        action ->setChecked(child == activeMdiChild());
    }
}

void MainWindow::readSettings() {

    const QByteArray geometry = applicationSettings->value("MainWindow.geometry", QByteArray()).toByteArray();

    if (geometry.isEmpty()) {
        this->showMaximized();
    } else {
        restoreGeometry(geometry);
    }
    toggleFullscreenAct->setChecked(this->isMaximized());

    recentImageReadingDirectory = applicationSettings->value("RecentImageReadingDirectory", "").toString();
    recentImageWritingDirectory = applicationSettings->value("RecentImageWritingDirectory", "").toString();
    recentDataWritingDirectory = applicationSettings->value("RecentDataWritingDirectory", "").toString();

    Qt::ToolBarArea toolBarPosition = static_cast<Qt::ToolBarArea>(applicationSettings->value("MainWindow.ToolbarPosition", Qt::LeftToolBarArea).toUInt());
    addToolBar(toolBarPosition, toolBar); // As toolbar is already attached to the window, it is only moved to this position by addToolBar
}

void MainWindow::writeSettings() {
    applicationSettings->setValue("MainWindow.geometry", saveGeometry());

    applicationSettings->setValue("RecentImageReadingDirectory", recentImageReadingDirectory);
    applicationSettings->setValue("RecentImageWritingDirectory", recentImageWritingDirectory);
    applicationSettings->setValue("RecentDataWritingDirectory", recentDataWritingDirectory);

    applicationSettings->setValue("MainWindow.ToolbarPosition", static_cast<uint>(toolBarArea(toolBar)));
}

QWidget* MainWindow::activeMdiChild() const {
    if (QMdiSubWindow *activeSubWindow = mdiArea->activeSubWindow())
        return activeSubWindow->widget();
    return nullptr;
}

#ifdef USE_PYLON
void MainWindow::updateSingleCamerasMenu() {

    QApplication::setOverrideCursor(Qt::WaitCursor);
    singleCamerasMenu->clear();

    try {
        Pylon::DeviceInfoList_t allDevices = enumerateCameraDevices();
        if(allDevices.empty()) {
            goto enumerateCameras_noDevicesFound;
        }
        Pylon::DeviceInfoList_t::const_iterator deviceIt;
        for (deviceIt = allDevices.begin(); deviceIt != allDevices.end(); ++deviceIt) {

            QString friendlyName = QString::fromStdString(deviceIt->GetFriendlyName().c_str());

            // In some cases, for GigE on Apple specifically, devices might appear twice. This is a workaround.
            bool sameAlreadyFound = false;
            for(auto eal : singleCamerasMenu->actions()) {
                if(eal->text() == friendlyName) {
                    sameAlreadyFound = true;
                    break;
                }
            }
            if(sameAlreadyFound)
                continue;

            QAction *cameraAction = singleCamerasMenu->addAction(friendlyName);
            //qDebug() << "---------------------------------" << QString(deviceIt->GetFriendlyName().c_str());
            //qDebug() << "---------------------------------" << QString(deviceIt->GetFullName().c_str());
            cameraAction->setData(friendlyName);
            //cameraAction->setData(QVariant::fromValue<Pylon::CDeviceInfo>(*deviceIt));
            if (QString(deviceIt->GetModelName().c_str()).toLower().contains("emu")) {
                cameraAction->setIcon(SVGIconColorAdjuster::loadAndAdjustColors(
                        QString(":/icons/Breeze/actions/22/composite-track-preview.svg"), applicationSettings));
                cameraAction->setIconVisibleInMenu(true);
            }
        }
        QApplication::restoreOverrideCursor();
        return;
    } catch (const GenericException &e) {
        // These are for the Pylon errors
        std::cerr << "An exception occurred." << std::endl << e.GetDescription() << std::endl;
        QMessageBox err(this);
        err.critical(this, "Device Error", QString("Device error occured, or an exception was raised in the Pylon library.\n\n") + e.GetDescription());
        QAction *cameraAction = singleCamerasMenu->addAction("Could not retrieve list of devices.");
        QApplication::restoreOverrideCursor();
        return;
    } catch (const std::exception &e) {
        // These are for any other
        std::cerr << "An exception occurred." << std::endl << e.what() << std::endl;
        QMessageBox err(this);
        err.critical(this, "Device Error", QString("Device error occured, or an exception was raised in the Pylon wrapper.\n\n") + e.what());
        QAction *cameraAction = singleCamerasMenu->addAction("Could not retrieve list of devices.");
        QApplication::restoreOverrideCursor();
        return;
    }

    enumerateCameras_noDevicesFound:
    // "finally", if we did not find any device
    QAction *cameraAction = singleCamerasMenu->addAction("No devices.");
    QApplication::restoreOverrideCursor();
    cameraAction->setEnabled(false);
}
#else
void MainWindow::updateSingleCamerasMenu() {

    QApplication::setOverrideCursor(Qt::WaitCursor);
    singleCamerasMenu->clear();

    // To prevent the user from accedentally triggering a focus change (and making
    //  the newly updated device list menu disappear just upon appearance)
    cameraMenu->blockSignals(true);

    try {
        uint nDevices = enumerateCameraDevices();
        if(nDevices < 1) {
            goto enumerateCameras_noDevicesFound;
        }

        // Now the user regains the right to select any other menu by hovering on
        cameraMenu->blockSignals(false);

        for (uint i = 0; i < nDevices; ++i) {

            GError *error = nullptr;

            qDebug() << "arv_get_device_id() = " << arv_get_device_id(i);
            qDebug() << "arv_get_device_vendor() = " << arv_get_device_vendor(i);
            qDebug() << "arv_get_device_model() = " << arv_get_device_model(i);
            qDebug() << "arv_get_device_serial_nbr() = " << arv_get_device_serial_nbr(i);
            qDebug() << "arv_get_device_address() = " << arv_get_device_address(i);
            qDebug() << "arv_get_device_physical_id() = " << arv_get_device_physical_id(i);
            qDebug() << "arv_get_device_protocol() = " << arv_get_device_protocol(i);
            qDebug() << "arv_get_device_manufacturer_info() = " << arv_get_device_manufacturer_info(i);

            //auto c = arv_camera_new_with_device(*deviceIt, &error);
            //if(error) continue;
            //auto mn = arv_camera_get_model_name(c, &error);
            //if(error) continue;
            //auto id = arv_camera_get_device_id(c, &error);

            QString friendlyName =
                    QString(arv_get_device_vendor(i)) + " " +
                    QString(arv_get_device_model(i)) + " (" +
                    QString(arv_get_device_serial_nbr(i)) + ")";

            // In some cases, for GigE on Apple specifically, devices might appear twice. This is a workaround.
            bool sameAlreadyFound = false;
            for(auto eal : singleCamerasMenu->actions()) {
                if(eal->text() == friendlyName) {
                    sameAlreadyFound = true;
                    break;
                }
            }
            if(sameAlreadyFound)
                continue;

            QAction *cameraAction = singleCamerasMenu->addAction(friendlyName);
            qDebug() << "--------------------------------- friendly name: " << friendlyName;
            qDebug() << "--------------------------------- device id: " << arv_get_device_id(i);
            cameraAction->setData(arv_get_device_id(i));
            //cameraAction->setData(QVariant::fromValue<Pylon::CDeviceInfo>(*deviceIt));
            if(QString(arv_get_device_id(i)).toLower().contains("emu")) {
                cameraAction->setIcon(SVGIconColorAdjuster::loadAndAdjustColors(
                        QString(":/icons/Breeze/actions/22/composite-track-preview.svg"), applicationSettings));
                cameraAction->setIconVisibleInMenu(true);
            }
        }
        QApplication::restoreOverrideCursor();
        return;
    } catch (const std::exception &e) {
        std::cerr << "An exception occurred." << std::endl << e.what() << std::endl;
        QMessageBox err(this);
        err.critical(this, "Device Error", e.what());
        QAction *cameraAction = singleCamerasMenu->addAction("Could not retrieve list of devices.");
        QApplication::restoreOverrideCursor();
        return;
    }

    enumerateCameras_noDevicesFound:
    // "finally", if we did not find any device
    QAction *cameraAction = singleCamerasMenu->addAction("No devices.");
    QApplication::restoreOverrideCursor();
    cameraAction->setEnabled(false);
}
#endif

// not working currently. On windows 10 it returns an empty list sometimes, even if camera is connected
/*
void MainWindow::updateOpenCVCamerasMenu() {

    openCVCamerasMenu->clear();

    auto a = QCameraInfo::defaultCamera();

    try {
        QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
        if(!cameras.empty()) {
            for(int i=0; i<cameras.size(); i++) {
                QAction *cameraAction = openCVCamerasMenu->addAction(cameras[i].deviceName());
                cameraAction->setData(i);
                if(cameras[i].deviceName().toLower().contains("emu")) {
                    cameraAction->setIcon(SVGIconColorAdjuster::loadAndAdjustColors(
                            QString(":/icons/Breeze/actions/22/composite-track-preview.svg"), applicationSettings));
                }
            }
            return;
        }
    } catch (const QException &e) {
        std::cerr << "An exception occurred." << std::endl << e.what() << std::endl;
        QMessageBox err(this);
        err.critical(this, "Device Error", e.what());
        return;
    }

    // "finally", if we did not find any device
    QAction *cameraAction = openCVCamerasMenu->addAction("No devices.");
    cameraAction->setEnabled(false);
}
 */

void MainWindow::updateRois() {
    int val = pupilDetectionWorker->getCurrentProcMode();
    // this needs to happen, because if we just open a camera, and start tracking, no ROI has been set for pupilDetection before
    if(val == ProcMode::SINGLE_IMAGE_ONE_PUPIL) {
        QRectF roi = applicationSettings->value("SingleCameraView.ROIsingleImageOnePupil.discrete", QRectF()).toRectF();
        if(!roi.isEmpty()){
            QRectF roi_rat = applicationSettings->value("SingleCameraView.ROIsingleImageOnePupil.rational", QRectF()).toRectF();
            pupilDetectionWorker->setROIsingleImageOnePupil(SupportFunctions::calculateRoiD(selectedCamera->getImageROI(), roi, roi_rat));
        }
    } else if(val == ProcMode::SINGLE_IMAGE_TWO_PUPIL) {
        QRectF roiR = applicationSettings->value("SingleCameraView.ROIsingleImageTwoPupilR.discrete", QRectF()).toRectF();
        QRectF roiL = applicationSettings->value("SingleCameraView.ROIsingleImageTwoPupilL.discrete", QRectF()).toRectF();
        if(!roiR.isEmpty()) {
            QRectF roiR_rat = applicationSettings->value("SingleCameraView.ROIsingleImageTwoPupilR.rational", QRectF()).toRectF();
            pupilDetectionWorker->setROIsingleImageTwoPupilR(SupportFunctions::calculateRoiD(selectedCamera->getImageROI(), roiR, roiR_rat));
            //pupilDetectionWorker->setROIsingleImageTwoPupilR(roiR);
        }
        if(!roiL.isEmpty()) {
            QRectF roiL_rat = applicationSettings->value("SingleCameraView.ROIsingleImageOnePupilL.rational", QRectF()).toRectF();
            pupilDetectionWorker->setROIsingleImageTwoPupilL(SupportFunctions::calculateRoiD(selectedCamera->getImageROI(), roiL, roiL_rat));
            //pupilDetectionWorker->setROIsingleImageTwoPupilL(roiB);
        }
    } else if(val == ProcMode::STEREO_IMAGE_ONE_PUPIL) {
        QRectF roiM = applicationSettings->value("StereoCameraView.ROIstereoImageOnePupilM.discrete", QRectF()).toRectF();
        QRectF roiS = applicationSettings->value("StereoCameraView.ROIstereoImageOnePupilS.discrete", QRectF()).toRectF();
        if(!roiM.isEmpty()) {
            QRectF roiM_rat = applicationSettings->value("SingleCameraView.ROIstereoImageOnePupilM.rational", QRectF()).toRectF();
            pupilDetectionWorker->setROIstereoImageOnePupilM(
                    SupportFunctions::calculateRoiD(selectedCamera->getImageROI(), roiM, roiM_rat));
            //pupilDetectionWorker->setROIstereoImageOnePupilM(roiMain1);
        }
        if(!roiS.isEmpty()) {
            QRectF roiS_rat = applicationSettings->value("SingleCameraView.ROIstereoImageOnePupilS.rational", QRectF()).toRectF();
            pupilDetectionWorker->setROIstereoImageOnePupilS(
                    SupportFunctions::calculateRoiD(selectedCamera->getImageROI(), roiS, roiS_rat));
            //pupilDetectionWorker->setROIstereoImageOnePupilS(roiSecondary1);
        }
    } else if(val == ProcMode::STEREO_IMAGE_TWO_PUPIL) {
        QRectF roiRM = applicationSettings->value("StereoCameraView.ROIstereoImageTwoPupilRM.discrete", QRectF()).toRectF();
        QRectF roiLM = applicationSettings->value("StereoCameraView.ROIstereoImageTwoPupilLM.discrete", QRectF()).toRectF();
        QRectF roiRS = applicationSettings->value("StereoCameraView.ROIstereoImageTwoPupilRS.discrete", QRectF()).toRectF();
        QRectF roiLS = applicationSettings->value("StereoCameraView.ROIstereoImageTwoPupilLS.discrete", QRectF()).toRectF();
        if(!roiRM.isEmpty()) {
            QRectF roiRM_rat = applicationSettings->value("SingleCameraView.ROIstereoImageTwoPupilRM.rational", QRectF()).toRectF();
            pupilDetectionWorker->setROIstereoImageTwoPupilRM(
                    SupportFunctions::calculateRoiD(selectedCamera->getImageROI(), roiRM, roiRM_rat));
            //pupilDetectionWorker->setROIstereoImageTwoPupilRM(roiMain1);
        }
        if(!roiLM.isEmpty()) {
            QRectF roiLM_rat = applicationSettings->value("SingleCameraView.ROIstereoImageTwoPupilLM.rational", QRectF()).toRectF();
            pupilDetectionWorker->setROIstereoImageTwoPupilLM(
                    SupportFunctions::calculateRoiD(selectedCamera->getImageROI(), roiLM, roiLM_rat));
            //pupilDetectionWorker->setROIstereoImageTwoPupilLM(roiMain2);
        }
        if(!roiRS.isEmpty()) {
            QRectF roiRS_rat = applicationSettings->value("SingleCameraView.ROIstereoImageTwoPupilRS.rational", QRectF()).toRectF();
            pupilDetectionWorker->setROIstereoImageTwoPupilRS(
                    SupportFunctions::calculateRoiD(selectedCamera->getImageROI(), roiRS, roiRS_rat));
            //pupilDetectionWorker->setROIstereoImageTwoPupilRS(roiSecondary1);
        }
        if(!roiLS.isEmpty()) {
            QRectF roiLS_rat = applicationSettings->value("SingleCameraView.ROIstereoImageTwoPupilLS.rational", QRectF()).toRectF();
            pupilDetectionWorker->setROIstereoImageTwoPupilLS(
                    SupportFunctions::calculateRoiD(selectedCamera->getImageROI(), roiLS, roiLS_rat));
            //pupilDetectionWorker->setROIstereoImageTwoPupilLS(roiSecondary2);
        }
        // } else if(val == ProcMode::MIRR_IMAGE_ONE_PUPIL) {
        //     QRectF roi1 = applicationSettings->value("SingleCameraView.ROImirrImageOnePupil1.discrete", QRectF()).toRectF();
        //     QRectF roi2 = applicationSettings->value("SingleCameraView.ROImirrImageOnePupil2.discrete", QRectF()).toRectF();
        //     if(!roi1.isEmpty())
        //         pupilDetectionWorker->setROImirrImageOnePupil1(roi1);
        //     if(!roi2.isEmpty())
        //         pupilDetectionWorker->setROImirrImageOnePupil2(roi2);
    }
}

void MainWindow::onTrackActClick() {

    if(trackingOn) {
        // Deactivate tracking
        pupilDetectionWorker->stopTracking();

        if(pupilDetectionSettingsDialog) {
            //pupilDetectionSettingsDialog->updateProcModeEnabled();
            pupilDetectionSettingsDialog->onSettingsChange();
        }

        const QIcon trackOffIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/view-visible.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
        trackAct->setIcon(trackOffIcon);
        trackingOn = false;

        if(recordOn)
            onRecordClick();
        if(streamOn)
            onStreamClick();

        if(singleCameraSettingsDialog && !recordImagesOn)
            singleCameraSettingsDialog->setLimitationsWhileTracking(false);
        if(stereoCameraSettingsDialog && !recordImagesOn)
            stereoCameraSettingsDialog->setLimitationsWhileTracking(false);
        if(singleWebcamSettingsDialog && !recordImagesOn)
            singleWebcamSettingsDialog->setLimitationsWhileTracking(false);
        
        recordAct->setDisabled(true);
        streamAct->setDisabled(true);
    } else {
        // Activate tracking

        // Even though its already set in the selection of the camera, camera calibration may changed till now so we need to load it again
        // TODO better way of doing this without setting the camera two times (or loading the config at selection differently)
        pupilDetectionWorker->setCamera(selectedCamera);

        // TODO: ?does fileCamera have a default procMode set?
        if(singleCameraChildWidget)
            singleCameraChildWidget->updateForPupilDetectionProcMode();
        if(stereoCameraChildWidget)
            stereoCameraChildWidget->updateForPupilDetectionProcMode();

        if(singleCameraSettingsDialog)
            singleCameraSettingsDialog->setLimitationsWhileTracking(true);
        if(stereoCameraSettingsDialog)
            stereoCameraSettingsDialog->setLimitationsWhileTracking(true);
        if(singleWebcamSettingsDialog)
            singleWebcamSettingsDialog->setLimitationsWhileTracking(true);

        if(pupilDetectionSettingsDialog) {
            //pupilDetectionSettingsDialog->updateProcModeEnabled();
            pupilDetectionSettingsDialog->onSettingsChange();
        }

        // NOTE: 2026.03.17. COMMENTED OUT. The rois do not need to be reset to rational unless necessary.
        //  necessary means: when opening a new recording
        //updateRois();
        // NOTE: This needs to be called AFTER all pupil detection ROIs are loaded and set in the current
        // pupilDetection instance, otherwise autoParam will not be done

        //if(SupportFunctions::readBoolFromQSettings("PupilDetectionSettingsDialog.computeBRISQUE", true, applicationSettings));
        //pupilDetectionWorker->enableComputeBRISQUE();

        pupilDetectionWorker->startTracking();
        if(pupilDetectionSettingsDialog) {
            //again, because we need updateProcModeEnabled() private method to be evoked by onSettingsChange in pupilDetectionSettingsDialog
            pupilDetectionSettingsDialog->onSettingsChange();
        }

        const QIcon trackOnIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/redeyes.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
        trackAct->setIcon(trackOnIcon);
        trackingOn = true;

        recordAct->setEnabled(!dataRecordingOutputTarget.isEmpty() && !exportingRecSection);

        // NOTE: streaming can only be enabled if the underlying connection is established, and tracking is on
        streamAct->setEnabled(trackingOn && streamingSettingsDialog && streamingSettingsDialog->isAnyConnected() && !exportingRecSection);
    }

    if(stereoCameraChildWidget && (selectedCamera->getType() == CameraImageType::LIVE_STEREO_CAMERA || selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE)) {
        stereoCameraChildWidget->update();
    } else if(singleCameraChildWidget && (selectedCamera->getType() == CameraImageType::LIVE_SINGLE_CAMERA || selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE)) {
        singleCameraChildWidget->update();
    }
}

void MainWindow::onStreamingSettingsClick() {
    if(streamingSettingsDialog)
        streamingSettingsDialog->show();
}

void MainWindow::onStreamClick() {

    if(dataStreamer) { // if streaming is on, deactivate streaming

        disconnect(pupilDetectionWorker, SIGNAL (processedPupilData(quint64, int, std::vector<Pupil>)), dataStreamer, SLOT (newPupilData(quint64, int, std::vector<Pupil>)));

        streamingSettingsDialog->setLimitationsWhileStreamingUDP(false);
        streamingSettingsDialog->setLimitationsWhileStreamingCOM(false);
#ifdef USE_LSL
        streamingSettingsDialog->setLimitationsWhileStreamingLSL(false);
#endif
        streamingSettingsDialog->setLimitationsWhileStreamingAny(false);

        dataStreamer->close(); // TODO check if may terminate writing to early? because of the lag of the event queue in pupildetection
        dataStreamer->deleteLater();
        dataStreamer = nullptr;

        const QIcon streamIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/media-record-green.svg"), applicationSettings);
        streamAct->setIcon(streamIcon);
        streamOn = false;

    } else { // Activate streaming

        if( !streamingSettingsDialog->isAnyConnected() || !trackingOn ) {
            return;
        }

        safelyResetTrialCounter();
        safelyResetMessageRegister();

        dataStreamer = new DataStreamer(
            connPoolCOM,
            connPoolUDP,
            pupilDetectionWorker,
            recEventTracker,
            this
            );
        
        if(streamingSettingsDialog->isUDPConnected()) {
            dataStreamer->startUDPStreamer(
                    streamingSettingsDialog->getConnPoolUDPIndex(),
                    applicationSettings->value("StreamingSettings.UDP.sampleRate", 30).toInt(),
                    streamingSettingsDialog->getDataContainerUDP() );
            streamingSettingsDialog->setLimitationsWhileStreamingUDP(true);
        }
        if(streamingSettingsDialog->isCOMConnected()) {
            dataStreamer->startCOMStreamer(
                    streamingSettingsDialog->getConnPoolCOMIndex(),
                    applicationSettings->value("StreamingSettings.COM.sampleRate", 30).toInt(),
                    streamingSettingsDialog->getDataContainerCOM() );
            streamingSettingsDialog->setLimitationsWhileStreamingCOM(true);
        }
#ifdef USE_LSL
        if(streamingSettingsDialog->isLSLConnected()) {
            dataStreamer->startLSLStreamer(
                    applicationSettings->value("StreamingSettings.LSL.sampleRate", 30).toInt(),
                    streamingSettingsDialog->getDataContainerLSL(),
                    pupilDetectionWorker->getCurrentProcMode() );
            streamingSettingsDialog->setLimitationsWhileStreamingLSL(true);
        }
#endif
        streamingSettingsDialog->setLimitationsWhileStreamingAny(true);

        connect(pupilDetectionWorker, SIGNAL (processedPupilData(quint64, int, std::vector<Pupil>)), dataStreamer, SLOT (newPupilData(quint64, int, std::vector<Pupil>)));

        const QIcon streamIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/kt-stop-all.svg"), applicationSettings);
        streamAct->setIcon(streamIcon);
        streamOn = true;
    }
}

void MainWindow::onRecordClick() {

    if(dataRecordingOutputTarget.isEmpty())
        return;

    if(recordOn && dataWriter) {
        // Deactivate recording

        dataWriter->close(); // TODO check if may terminate writing to early? because of the lag of the event queue in pupildetection
        dataWriter->deleteLater();
        dataWriter = nullptr;

        if(generalSettingsDialog)
            generalSettingsDialog->setLimitationsWhileDataWriting(false);

        const QIcon recordOffIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/media-record.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
        recordAct->setIcon(recordOffIcon);
        recordOn = false;

    } else {
        // Activate recording

        if(dataRecordingOutputTarget.isEmpty())
            return;

        // TODO: this version is imperfect yet, as it permanently overwrites pupilDetectionDataFile name
        bool changedGiven = false; // unused yet
        dataRecordingOutputTarget = SupportFunctions::prepareOutputFileForDataWriter(dataRecordingOutputTarget, applicationSettings, changedGiven, this);
        if(dataRecordingOutputTarget.isEmpty()) {
            QMessageBox *msgBox = new QMessageBox(this);
            msgBox->setWindowTitle("The set output path and file name could not be opened for writing");
            msgBox->setText("The set output path and file name could not be opened for writing. Please check that PupilEXT has the permissions to write there, and try again.");
            msgBox->setMinimumSize(330,240);
            msgBox->setIcon(QMessageBox::Warning);
            msgBox->setModal(false);
            msgBox->show();

            recordAct->setDisabled(true);
            return;
        }

        if(generalSettingsDialog)
            generalSettingsDialog->setLimitationsWhileDataWriting(true);

        dataWriter = 
            new DataWriter(
                    dataRecordingOutputTarget,
                    pupilDetectionWorker,
                    recEventTracker,
                    this);
        if(!dataWriter->isReady()) { // in case we failed to open csv file for writing
            dataWriter->deleteLater();
            dataWriter = nullptr;
            return;
        }

        safelyResetTrialCounter();
        safelyResetMessageRegister();
        
        QFileInfo fi(dataRecordingOutputTarget);
        QDir pupilDetectionDir = fi.dir();
        QString metadataFileName = fi.baseName() + QString::fromStdString("_datarec_meta.xml");

        int currentProcMode = pupilDetectionWorker->getCurrentProcMode();

        MetaSnapshotOrganizer::writeSnapshotFile(
            pupilDetectionDir.filePath(metadataFileName),
            selectedCamera, imageWriter, pupilDetectionWorker, dataWriter, MetaSnapshotOrganizer::Purpose::DATA_REC, applicationSettings);

        connect(pupilDetectionWorker, SIGNAL (processedPupilData(quint64, int, std::vector<Pupil>)), dataWriter, SLOT (newPupilData(quint64, int, std::vector<Pupil>)));

        const QIcon recordOnIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/kt-stop-all.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
        recordAct->setIcon(recordOnIcon);
        recordOn = true;
    }
}

void MainWindow::onRecordImageClick() {

    if(recordImagesOn) {
        // Deactivate recording

        // TODO. might not be necessary
        disconnect(signalPubSubHandler, SIGNAL(onNewGrabResult(CameraImage)), imageWriter, SLOT (onNewImage(CameraImage)));

        /*
        const QIcon recordAttemptedToStopIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/appointment-new.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
        recordImagesAct->setIcon(recordAttemptedToStopIcon);
        //recordImagesOn = false;
        recordImagesAct->setDisabled(true);
         */


        const QIcon recordOffIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/media-record-blue.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
        recordImagesAct->setIcon(recordOffIcon);
        recordImagesOn = false;

        /*
        if (imageWriter != nullptr){
            imageWriter->deleteLater();
            imageWriter = nullptr;
        }
        */
//        imageWriter->attemptToStop();

        QString foundEventLogContent = imageWriter->getFoundOfflineEventLogContent();
        imageWriter->writeOfflineEventLog(recEventTracker->generateOfflineEventLogContent(
                imageRecStartTimestamp,
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(),
                foundEventLogContent) );

        // in case it is a zip, it closes the file. In a multithread approach, this will have more to do of course
        imageWriter->stopWriting();
        // Note: this call does not destruct imagewriter
        
        if(singleCameraSettingsDialog && !trackingOn)
            singleCameraSettingsDialog->setLimitationsWhileTracking(false);
        if(stereoCameraSettingsDialog && !trackingOn)
            stereoCameraSettingsDialog->setLimitationsWhileTracking(false);
        if(singleWebcamSettingsDialog && !trackingOn)
            singleWebcamSettingsDialog->setLimitationsWhileTracking(false);

        if(generalSettingsDialog)
            generalSettingsDialog->setLimitationsWhileImageWriting(false);

    } else {
        // Activate recording

        if(imageRecordingOutputTarget.isEmpty())
            return;

        qDebug() << "imageRecordingOutputTarget before checking for eligibility: " << imageRecordingOutputTarget << "---";

        bool stereo = selectedCamera->getType() == CameraImageType::LIVE_STEREO_CAMERA || selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE;

        //---------------------------------
        // TODO ASAP: HA ZIP, nézze meg, hogy nincs-e már ugyanilyen nevű fájl. Ha van, akkor szóljon, és készítsen megtoldott nevűt!!
        // TODO: a metasnapshot és rec event log, képes legyen szintén zipbe íródni!
        // -------------------------------------------------

        // TODO: why use string everywhere for directory? Use QDir instead, or clarify naming ("directory" variables should all be QString or QDir type)
        // TODO: this version is imperfect yet, as it permanently overwrites outputDirectory (image output directory) name
        bool changedGiven = false; // unused yet
        while(  imageWriter->getImageWriterStatus() == ImageWriter::IWSTATUS_UNDETERMINED ||
                imageWriter->getImageWriterStatus() == ImageWriter::IWSTATUS_ZIP_UNOPENABLE ||
                imageWriter->getImageWriterStatus() == ImageWriter::IWSTATUS_VIDEO_START_FAILURE) {

            if (    imageRecordingOutputTarget.endsWith(".zip") ||
                    imageRecordingOutputTarget.endsWith(".mkv") ) {
                imageRecordingOutputTarget = SupportFunctions::prepareOutputFileDirForImageWriter(
                        imageRecordingOutputTarget, applicationSettings, changedGiven, this);
            } else {
                imageRecordingOutputTarget = SupportFunctions::prepareOutputDirForImageWriter(
                        imageRecordingOutputTarget, applicationSettings, changedGiven, this);
            }

            if (imageRecordingOutputTarget.isEmpty()) {
                QMessageBox *msgBox = new QMessageBox(this);
                msgBox->setWindowTitle("The set output path could not be opened for writing");
                msgBox->setText(
                        "The set output path could not be opened for writing. Please check that PupilEXT has the permissions to write there, and try again.");
                msgBox->setMinimumSize(330, 240);
                msgBox->setIcon(QMessageBox::Warning);
                msgBox->setModal(false);
                msgBox->show();

                recordImagesAct->setDisabled(true);
                return;
            }

            // TODO: only make record button clickable again, if the last recording has ended (signals in queue were dealt with)
            imageWriter->prepareForWriting(imageRecordingOutputTarget, stereo, QSize(selectedCamera->getImageROIwidth(), selectedCamera->getImageROIheight()), selectedCamera->getResultingFrameRateValue() );
            if (imageWriter->getImageWriterStatus() == ImageWriter::IWSTATUS_ZIP_UNOPENABLE) {

                QMessageBox *msgBox = new QMessageBox(this);
                msgBox->setWindowTitle("The set existing output Zip archive could not be opened");
                msgBox->setText(
                        "The set existing output Zip archive could not be opened for appending. The archive file might be corrupted or it is compressed in an unknown format. Please check that PupilEXT has the permissions, and try again. Importantly, this does not mean that the archive is lost: the file might still contain a portion of its original contents, which could be retrieved by a proper extractor program.");
                msgBox->setMinimumSize(330, 260);
                msgBox->setIcon(QMessageBox::Warning);
                msgBox->setModal(true);
                msgBox->exec();

            }
            if (imageWriter->getImageWriterStatus() == ImageWriter::IWSTATUS_ERROR) {

                imageWriter->stopWriting();
                // Note: this call does not destruct imagewriter
                return;
            }

            if (imageWriter->getImageWriterStatus() == ImageWriter::IWSTATUS_VIDEO_START_FAILURE) {

                QMessageBox *msgBox = new QMessageBox(this);
                msgBox->setWindowTitle("Video writing could not start");
                msgBox->setText(
                        "We could not start writing the video file with the current video output settings. Either the codec could not be set up or sufficient memory cannot be allocated. Please set another video output codec and try again. If the issue persists, try rather writing the images into a Zip archive or image directory, as they are reliable fallbacks.");
                msgBox->setMinimumSize(330, 260);
                msgBox->setIcon(QMessageBox::Warning);
                msgBox->setModal(true);
                msgBox->exec();

                // TODO: auto countdown if user does not interact, and start writing with a fallback option of e.g. zip

                return;
            }
        }
        qDebug() << "imageRecordingOutputTarget finally set to: " << imageRecordingOutputTarget;

        imageRecStartTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        // trial counter and message register are both reset with a first entry that corresponds to recording start
        safelyResetTrialCounter(imageRecStartTimestamp);
        safelyResetMessageRegister(imageRecStartTimestamp);

        if(singleCameraSettingsDialog)
            singleCameraSettingsDialog->setLimitationsWhileTracking(true);
        if(stereoCameraSettingsDialog)
            stereoCameraSettingsDialog->setLimitationsWhileTracking(true);
        if(singleWebcamSettingsDialog)
            singleWebcamSettingsDialog->setLimitationsWhileTracking(true);

        if(generalSettingsDialog)
            generalSettingsDialog->setLimitationsWhileImageWriting(true);

        // this should come here as the "directory already exists" dialog is only answered before, upon creation of imageWriter, and meta snapshot creation relies on that response
        imageWriter->writeMetaSnapshot(MetaSnapshotOrganizer::generateSnapshotFileContent(
                selectedCamera, imageWriter, pupilDetectionWorker, dataWriter, MetaSnapshotOrganizer::Purpose::IMAGE_REC, applicationSettings));
        // GB: maybe write unix timestamp too in the name of meta snapshot file?

        // TODO: might not be necessary here, once imageWriter will be in a separate thread itself
        connect(signalPubSubHandler, SIGNAL(onNewGrabResult(CameraImage)), imageWriter, SLOT (onNewImage(CameraImage)));

        const QIcon recordOnIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/kt-stop-all.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
        recordImagesAct->setIcon(recordOnIcon);
        recordImagesOn = true;
    }
}

void MainWindow::onCameraClick() {
    // fix to open submenu in the camera menu
    cameraAct->menu()->exec(QCursor::pos());
}
void MainWindow::onImageRecordingOutputClick() {
    // fix to open submenu in the camera menu
    imageRecordingOutputAct->menu()->exec(QCursor::pos());
}

void MainWindow::onCameraDisconnectClick() {

    // Clear previously set output paths
    dataRecordingOutputTarget = "";
    //setRecentDataWritingDirectory("");

    QList<QMdiSubWindow *> windows = mdiArea->subWindowList();
    for(auto mdiSubWindow : windows) {
        // NOTE: cameraViewWindow->close(); happens here already, with its cleanup call, done in an onClose lambda
        mdiSubWindow->close();
        mdiSubWindow->deleteLater();
    }

    if(imagePlaybackControlDialog) {
//        disconnect(selectedCamera, SIGNAL(finished()), imagePlaybackControlDialog, SLOT(onPlaybackFinished()));
        //disconnect(selectedCamera, SIGNAL(endReached()), imagePlaybackControlDialog, SLOT(onAutomaticFinish()));
        disconnect(imagePlaybackControlDialog, SIGNAL(onPlaybackStartInitiated()), this, SLOT(onPlaybackStartInitiated()));
        disconnect(imagePlaybackControlDialog, SIGNAL(onPlaybackPauseInitiated()), this, SLOT(onPlaybackPauseInitiated()));
        disconnect(imagePlaybackControlDialog, SIGNAL(onPlaybackStopInitiated()), this, SLOT(onPlaybackStopInitiated()));
        disconnect(this, SIGNAL(playbackStartApproved()), imagePlaybackControlDialog, SLOT(onPlaybackStartApproved()));
        disconnect(this, SIGNAL(playbackPauseApproved()), imagePlaybackControlDialog, SLOT(onPlaybackPauseApproved()));
        disconnect(this, SIGNAL(playbackStopApproved()), imagePlaybackControlDialog, SLOT(onPlaybackStopApproved()));
        imagePlaybackControlDialog = nullptr;
    }
    destroyCamTempMonitor();

    // Todo: is this necesary? All closes happen already a few lines above. Although this pointer does not delete itself, etc.
    //  employ a similar solution to what there is already for the camera view window?
    if (sharpnessWindow) {
        sharpnessWindow->deleteLater();
        sharpnessWindow = nullptr;
    }

    if(recEventTracker) {
        disconnect(this, SIGNAL(commitTrialCounterIncrement(quint64)), recEventTracker, SLOT(addTrialIncrement(quint64)));
        disconnect(this, SIGNAL(commitTrialCounterReset(quint64)), recEventTracker, SLOT(resetBufferTrialCounter(quint64)));
        disconnect(this, SIGNAL(commitMessageRegisterReset(quint64)), recEventTracker, SLOT(resetBufferMessageRegister(quint64)));
        disconnect(this, SIGNAL(commitRemoteMessage(quint64, QString)), recEventTracker, SLOT(addMessage(quint64, QString)));

        recEventTracker->close();
        recEventTracker->deleteLater();
        recEventTracker = nullptr;
    }

    if (calibrationWindow){
        calibrationWindow->deleteLater();
        calibrationWindow = nullptr;
    }

    if(streamOn) {
        onStreamClick();
    }

    if(recordOn) {
        onRecordClick();
    }

    if(recordImagesOn) {
        onRecordImageClick();
    }

    if(trackingOn) {
        trackAct->setChecked(false);
        onTrackActClick();
    }

    // TODO: figure out a better way, because singleCameraSettingsDialog and the other 2 dialogs ALWAYS exist, they do not get deleted now
    if(selectedCamera  != nullptr) {
        // only if not "file camera":
        if(singleCameraSettingsDialog && (selectedCamera->getType() == CameraImageType::LIVE_SINGLE_CAMERA)) {
            //onSingleCameraSettingsClick();
            singleCameraSettingsDialog->accept();
            singleCameraSettingsDialog->deleteLater();
            singleCameraSettingsDialog = nullptr;
        } else if(stereoCameraSettingsDialog && selectedCamera->getType() == CameraImageType::LIVE_STEREO_CAMERA) {
            //stereoCameraSelected();
            stereoCameraSettingsDialog->accept();
            stereoCameraSettingsDialog->deleteLater();
            stereoCameraSettingsDialog = nullptr;
        } else if(singleWebcamSettingsDialog && selectedCamera->getType() == CameraImageType::LIVE_SINGLE_WEBCAM) {
            singleWebcamSettingsDialog->accept();
            singleWebcamSettingsDialog->deleteLater();
            singleWebcamSettingsDialog = nullptr;
        }

        // for all occasions:
        selectedCamera->close();
        pupilDetectionWorker->setCamera(nullptr);
        selectedCamera->deleteLater();
        selectedCamera = nullptr;
    }

    if (playbackSynchroniser != nullptr){
        playbackSynchroniser->deleteLater();
        playbackSynchroniser = nullptr;
    }

    // NOTE: thease are already dealt with, in resettatus(false);
//    fileOpenAct->setEnabled(true);
//    exportRecSectionAct->setEnabled(false);

    if (selectedCamera && signalPubSubHandler) {
        disconnect(selectedCamera, SIGNAL(onNewGrabResult(CameraImage)), signalPubSubHandler,
                   SIGNAL(onNewGrabResult(CameraImage)));
        disconnect(selectedCamera, SIGNAL(fps(double)), signalPubSubHandler, SIGNAL(cameraFPS(double)));
        disconnect(selectedCamera, SIGNAL(framecount(int)), signalPubSubHandler, SIGNAL(cameraFramecount(int)));

        disconnect(selectedCamera, SIGNAL (imagesSkipped()), this, SLOT (onImagesSkipped()));
        disconnect(selectedCamera, SIGNAL (cameraDeviceRemoved()), this, SLOT (onCameraUnexpectedlyDisconnected()));
        disconnect(selectedCamera, SIGNAL (deviceWasReset()), this, SLOT (onDeviceWasReset()));
        disconnect(selectedCamera, SIGNAL (manualDeviceResetNecessary()), this, SLOT (onManualDeviceResetNecessary()));
    }

    pupilDetectionSettingsDialog->onSettingsChange();
    if (pupilDetectionSettingsDialog && (singleCameraChildWidget || stereoCameraChildWidget)) {
        disconnect(pupilDetectionSettingsDialog, SIGNAL(pupilDetectionProcModeChanged(int)), singleCameraChildWidget,
                   SLOT(updateForPupilDetectionProcMode()));
        disconnect(pupilDetectionSettingsDialog, SIGNAL(pupilDetectionProcModeChanged(int)), stereoCameraChildWidget,
                   SLOT(updateForPupilDetectionProcMode()));
    }

    if(hwTriggerOn) {
        MCUSettingsDialogInst->sendCommand(QString("<SX>"));
        onHwTriggerDisable();
    }

    if(MCUSettingsDialogInst->isConnected()) {
        MCUSettingsDialogInst->doDisconnect();
    }
    if(MCUSettingsDialogInst->isVisible()) {
        MCUSettingsDialogInst->close();
    }
    if(pupilDetectionSettingsDialog->isVisible()) {
        pupilDetectionSettingsDialog->close();
    }
    if(setupGeometryDialog && setupGeometryDialog->isVisible()) {
        setupGeometryDialog->close();
    }
    //if(subjectSelectionDialog->isVisible()) {
    //    subjectSelectionDialog->close();
    //}

    onCameraCalibrationDisabled();

    //subjectConfigurationLabel->setText("");
    currentStatusMessageLabel->setText("");
    currentStatusMessageLabel->setToolTip("");

//    cameraSettingsAct->setEnabled(false);
//    cameraViewAct->setEnabled(false);
//    dataTableAct->setEnabled(false); // TODO: close datatable and all graph plots

    resetStatus(false);
}

void MainWindow::singleCameraSelected(QAction *action) {

    QVariant actionData = action->data();
    // Pylon::CDeviceInfo deviceInfo = actionData.value<Pylon::CDeviceInfo>();
    // // QString deviceFullname = QString(deviceInfo.GetFullName());
    QString deviceFriendlyName = actionData.value<QString>();

    // SINGLE INDUSTRIAL CAMERA
    try {
        selectedCamera = new SingleCamera(deviceFriendlyName, this);
    }
#ifdef USE_PYLON
    catch (const GenericException &e) {
        std::cerr << "An exception occurred." << std::endl << e.GetDescription() << std::endl;
        QMessageBox err(this);
        err.critical(this, "Device Error", QString("Device error occured, or an exception was raised in the Pylon wrapper.\n\n") + e.GetDescription());
        return;
    }
#endif
    catch (const std::exception &e) {
        std::cerr << "An exception occurred." << std::endl << e.what() << std::endl;
        QMessageBox err(this);
        err.critical(this, "Device Error", QString("Device error occured, or an exception was raised in the camera wrapper.\n\n") + e.what());
        return;
    }

    //safelyResetTrialCounter();
    //safelyResetMessageRegister();

    if(dynamic_cast<SingleCamera*>(selectedCamera)->getCameraCalibration()->isCalibrated())
        onCameraCalibrationEnabled();

    connect(selectedCamera, SIGNAL (imagesSkipped()), this, SLOT (onImagesSkipped()));
    connect(selectedCamera, SIGNAL (cameraDeviceRemoved()), this, SLOT (onCameraUnexpectedlyDisconnected()));
    connect(selectedCamera, SIGNAL (deviceWasReset()), this, SLOT (onDeviceWasReset()));
    connect(selectedCamera, SIGNAL (manualDeviceResetNecessary()), this, SLOT (onManualDeviceResetNecessary()));

    connect(selectedCamera, SIGNAL (onNewGrabResult(CameraImage)), signalPubSubHandler, SIGNAL (onNewGrabResult(CameraImage)));
    connect(selectedCamera, SIGNAL(fps(double)), signalPubSubHandler, SIGNAL(cameraFPS(double)));
    connect(selectedCamera, SIGNAL(framecount(int)), signalPubSubHandler, SIGNAL(cameraFramecount(int)));

    connect(dynamic_cast<SingleCamera*>(selectedCamera)->getCameraCalibration(), SIGNAL (finishedCalibration()), this, SLOT (onCameraCalibrationEnabled()));
    connect(dynamic_cast<SingleCamera*>(selectedCamera)->getCameraCalibration(), SIGNAL (unavailableCalibration()), this, SLOT (onCameraCalibrationDisabled())); 

    cameraViewClick();
    onSingleCameraSettingsClick();

//    cameraSettingsAct->setEnabled(true);
//    cameraViewAct->setEnabled(true);
//    dataTableAct->setEnabled(true);

    //pupilDetectionSettingsDialog->onSettingsChange(); // must come in this order, to set proc mode first
    pupilDetectionWorker->setCamera(selectedCamera);
    pupilDetectionSettingsDialog->onSettingsChange();

    recEventTracker = new RecEventTracker();
    connect(this, SIGNAL(commitTrialCounterIncrement(quint64)), recEventTracker, SLOT(addTrialIncrement(quint64)));
    connect(this, SIGNAL(commitTrialCounterReset(quint64)), recEventTracker, SLOT(resetBufferTrialCounter(quint64)));
    connect(this, SIGNAL(commitMessageRegisterReset(quint64)), recEventTracker, SLOT(resetBufferMessageRegister(quint64)));
    connect(this, SIGNAL(commitRemoteMessage(quint64, QString)), recEventTracker, SLOT(addMessage(quint64, QString)));
    safelyResetTrialCounter();
    safelyResetMessageRegister();

    createCamTempMonitor();

    connect(pupilDetectionSettingsDialog, SIGNAL (pupilDetectionProcModeChanged(int)), singleCameraChildWidget, SLOT (updateForPupilDetectionProcMode()));

    resetStatus(true);

}



void MainWindow::singleWebcamSelected(QAction *action) {

    try {
        int deviceID = action->data().toString().toInt();
        selectedCamera = new SingleWebcam(deviceID, "Webcam", this);
    } catch (const QException &e) {
        std::cerr << "An exception occurred." << std::endl << e.what() << std::endl;
        QMessageBox err(this);
        err.critical(this, "Device Error", e.what());
        return;
    }

    //safelyResetTrialCounter();
    //safelyResetMessageRegister();

    if(dynamic_cast<SingleWebcam*>(selectedCamera)->getCameraCalibration()->isCalibrated())
        onCameraCalibrationEnabled();

    // TODO: handle unexpected device removal
    connect(selectedCamera, SIGNAL(onNewGrabResult(CameraImage)), signalPubSubHandler, SIGNAL (onNewGrabResult(CameraImage)));
    connect(selectedCamera, SIGNAL(fps(double)), signalPubSubHandler, SIGNAL(cameraFPS(double)));
    connect(selectedCamera, SIGNAL(framecount(int)), signalPubSubHandler, SIGNAL(cameraFramecount(int)));

    connect(dynamic_cast<SingleWebcam*>(selectedCamera)->getCameraCalibration(), SIGNAL (finishedCalibration()), this, SLOT (onCameraCalibrationEnabled()));
    connect(dynamic_cast<SingleWebcam*>(selectedCamera)->getCameraCalibration(), SIGNAL (unavailableCalibration()), this, SLOT (onCameraCalibrationDisabled()));

    cameraViewClick();
    onSingleWebcamSettingsClick();

    if(singleWebcamSettingsDialog) {
        singleWebcamSettingsDialog->setLimitationsWhileWaitingToOpen(true);
    }

//    cameraSettingsAct->setEnabled(true);
//    cameraViewAct->setEnabled(true);
//    dataTableAct->setEnabled(true);

    // These are exclusive for webcam right now
    cameraAct->setEnabled(false);
    cameraSettingsAct->setEnabled(true);
    cameraActDisconnectAct->setEnabled(true);
    cameraViewAct->setEnabled(true);

    pupilDetectionWorker->setCamera(selectedCamera);
    pupilDetectionSettingsDialog->onSettingsChange();

    recEventTracker = new RecEventTracker();
    connect(this, SIGNAL(commitTrialCounterIncrement(quint64)), recEventTracker, SLOT(addTrialIncrement(quint64)));
    connect(this, SIGNAL(commitTrialCounterReset(quint64)), recEventTracker, SLOT(resetBufferTrialCounter(quint64)));
    connect(this, SIGNAL(commitMessageRegisterReset(quint64)), recEventTracker, SLOT(resetBufferMessageRegister(quint64)));
    connect(this, SIGNAL(commitRemoteMessage(quint64, QString)), recEventTracker, SLOT(addMessage(quint64, QString)));
    safelyResetTrialCounter();
    safelyResetMessageRegister();

    connect(pupilDetectionSettingsDialog, SIGNAL (pupilDetectionProcModeChanged(int)), singleCameraChildWidget, SLOT (updateForPupilDetectionProcMode()));

//    resetStatus(true);

    // Special, webcam-only signals
    connect(selectedCamera, SIGNAL(startedToOpenCamera()), this, SLOT(onWebcamStartedToOpen()));
    connect(selectedCamera, SIGNAL(couldNotOpenCamera()), this, SLOT(onWebcamCouldNotBeOpened()));
    connect(selectedCamera, SIGNAL(successfullyOpenedCamera()), this, SLOT(onWebcamSuccessfullyOpened()));

}

void MainWindow::stereoCameraSelected() {

    try {
        selectedCamera = new StereoCamera(this);
    }
#ifdef USE_PYLON
    catch (const GenericException &e) {
        std::cerr << "An exception occurred." << std::endl << e.GetDescription() << std::endl;
        QMessageBox err(this);
        err.critical(this, "Device Error", e.GetDescription());
        return;
    }
#else
    catch (const std::exception &e) {
        std::cerr << "An exception occurred." << std::endl << e.what() << std::endl;
        QMessageBox err(this);
        err.critical(this, "Device Error", e.what());
        return;
    }
#endif

    //safelyResetTrialCounter();
    //safelyResetMessageRegister();

    connect(selectedCamera, SIGNAL (imagesSkipped()), this, SLOT (onImagesSkipped()));
    connect(selectedCamera, SIGNAL (cameraDeviceRemoved()), this, SLOT (onCameraUnexpectedlyDisconnected()));
    connect(selectedCamera, SIGNAL (deviceWasReset()), this, SLOT (onDeviceWasReset()));
    connect(selectedCamera, SIGNAL (manualDeviceResetNecessary()), this, SLOT (onManualDeviceResetNecessary()));

    connect(selectedCamera, SIGNAL(onNewGrabResult(CameraImage)), signalPubSubHandler, SIGNAL(onNewGrabResult(CameraImage)));
    connect(selectedCamera, SIGNAL(fps(double)), signalPubSubHandler, SIGNAL(cameraFPS(double)));
    connect(selectedCamera, SIGNAL(framecount(int)), signalPubSubHandler, SIGNAL(cameraFramecount(int)));

    connect(dynamic_cast<StereoCamera*>(selectedCamera)->getCameraCalibration(), SIGNAL (finishedCalibration()), this, SLOT (onCameraCalibrationEnabled()));
    connect(dynamic_cast<StereoCamera*>(selectedCamera)->getCameraCalibration(), SIGNAL (unavailableCalibration()), this, SLOT (onCameraCalibrationDisabled()));

    cameraViewClick();
    onStereoCameraSettingsClick();

//    cameraSettingsAct->setEnabled(true);
//    cameraViewAct->setEnabled(true);
////    dataTableAct->setEnabled(true); // not here, as the actual camera has not been opened yet

    // These are exclusive for stereo camera right now
    cameraAct->setEnabled(false);
    cameraSettingsAct->setEnabled(true);
    cameraActDisconnectAct->setEnabled(true);
    cameraViewAct->setEnabled(true);

    pupilDetectionWorker->setCamera(selectedCamera); // NOTE: this should not be here, but in stereo camera settings dialog
    pupilDetectionSettingsDialog->onSettingsChange();

    recEventTracker = new RecEventTracker();
    connect(this, SIGNAL(commitTrialCounterIncrement(quint64)), recEventTracker, SLOT(addTrialIncrement(quint64)));
    connect(this, SIGNAL(commitTrialCounterReset(quint64)), recEventTracker, SLOT(resetBufferTrialCounter(quint64)));
    connect(this, SIGNAL(commitMessageRegisterReset(quint64)), recEventTracker, SLOT(resetBufferMessageRegister(quint64)));
    connect(this, SIGNAL(commitRemoteMessage(quint64, QString)), recEventTracker, SLOT(addMessage(quint64, QString)));
    safelyResetTrialCounter();
    safelyResetMessageRegister();
    
    createCamTempMonitor();

    connect(pupilDetectionSettingsDialog, SIGNAL (pupilDetectionProcModeChanged(int)), stereoCameraChildWidget, SLOT (updateForPupilDetectionProcMode()));

//    resetStatus(true);
}

void MainWindow::onWebcamStartedToOpen() {
    currentStatusMessageLabel->setText("Opening OpenCV webcam. This might take a few seconds...");
    QApplication::setOverrideCursor(Qt::WaitCursor);
//    if(singleWebcamSettingsDialog) {
//        singleWebcamSettingsDialog->setLimitationsWhileWaitingToOpen(false);
//    }
}

void MainWindow::onWebcamCouldNotBeOpened() {
    QMessageBox *webcamCouldNotBeOpenedMsgBox = new QMessageBox(this);
    webcamCouldNotBeOpenedMsgBox->setWindowTitle("OpenCV webcam could not be opened");
    webcamCouldNotBeOpenedMsgBox->setText("The OpenCV webcam you specified could not be opened.\nExecuting the open() method on the OpenCV VideoCapture() instance returned false.\nPlease check whether you have all the necessary drivers installed, and that the specified camera is accessible from other programs.\nIf this problem persists, notify the developer team.");
    webcamCouldNotBeOpenedMsgBox->setMinimumSize(330,240);
    webcamCouldNotBeOpenedMsgBox->setIcon(QMessageBox::Warning);
    webcamCouldNotBeOpenedMsgBox->setModal(false);
    webcamCouldNotBeOpenedMsgBox->show();

    onCameraDisconnectClick();
}


void MainWindow::onImageWriterFailed() {
    if(imageWriterFailedMsgBox != nullptr) {
        return;
    }
    imageWriterFailedMsgBox = new QMessageBox(this);
    imageWriterFailedMsgBox->setWindowTitle("Image writing failure");
    imageWriterFailedMsgBox->setText("At least one frame could not be written to disk.\nExecuting the imwrite() method of OpenCV returned false.\nPlease check if the disk is surely mounted and accessible, there is sufficient disk space for recording, and that PupilEXT is running with proper rights to write in the specified location.\nIf this problem persists, notify the developer team.");
    imageWriterFailedMsgBox->setMinimumSize(330,240);
    imageWriterFailedMsgBox->setIcon(QMessageBox::Warning);
    imageWriterFailedMsgBox->setModal(false);
    connect(imageWriterFailedMsgBox, SIGNAL(accepted()), this, SLOT(onImageWriterFailedMsgClose()));
    imageWriterFailedMsgBox->show();
}

void MainWindow::onImageWriterFailedMsgClose() {
    disconnect(imageWriterFailedMsgBox, SIGNAL(accepted()), this, SLOT(onImageWriterFailedMsgClose()));
    imageWriterFailedMsgBox->deleteLater();
    imageWriterFailedMsgBox = nullptr;
}

/*
void MainWindow::onImageWriterStopDone() {
    const QIcon recordOffIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/media-record-blue.svg"), applicationSettings);
    recordImagesAct->setIcon(recordOffIcon);
    recordImagesOn = false;
    recordImagesAct->setDisabled(false);
}
*/

void MainWindow::onWebcamSuccessfullyOpened() {
    currentStatusMessageLabel->setText("Webcam successfully opened.");
    if(singleWebcamSettingsDialog) {
        singleWebcamSettingsDialog->setLimitationsWhileWaitingToOpen(false);
    }
    QApplication::restoreOverrideCursor();
    resetStatus(true);
}

void MainWindow::cameraViewClick() {

    // TODO:
    // This currently could not work with the lines commented out below.
    // However, now it has to close and reopen the window to work, because the X button on the MDI subwindow
    // seems not only to hide the window, but set its pointer to deleted, for which we cannot check...
    // maybe some sort of smart pointers would help?
//    if(cameraViewWindow && !cameraViewWindow->isVisible()) {
//        cameraViewWindow->show();
//        return;
//    }

    if(cameraViewWindow && cameraViewWindow->isVisible()) {

        //// TODO: are these necessary? Or only an "cameraViewWindow->close();" would be enough?
        //if (singleCameraChildWidget) {
        //    disconnect(singleCameraChildWidget, SIGNAL (doingPupilDetectionROIediting(bool)), pupilDetectionSettingsDialog, SLOT (onDisableProcModeSelector(bool)));
        //    singleCameraChildWidget->deleteLater();
        //    singleCameraChildWidget = nullptr;
        //}
        //if (stereoCameraChildWidget) {
        //    disconnect(stereoCameraChildWidget, SIGNAL (doingPupilDetectionROIediting(bool)), pupilDetectionSettingsDialog, SLOT (onDisableProcModeSelector(bool)));
        //    stereoCameraChildWidget->deleteLater();
        //    stereoCameraChildWidget = nullptr;
        //}
        cameraViewWindow->close();

        //cameraViewWindow->deleteLater();
        //cameraViewWindow = nullptr;
    }

    RestorableQMdiSubWindow *child;
    if(selectedCamera && (
        selectedCamera->getType() == CameraImageType::LIVE_SINGLE_CAMERA || 
        selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE ||
        selectedCamera->getType() == CameraImageType::LIVE_SINGLE_WEBCAM
        ) ) {
        //SingleCameraView *childWidget = new SingleCameraView(selectedCamera, pupilDetectionWorker, this);
        singleCameraChildWidget = new SingleCameraView(selectedCamera, pupilDetectionWorker, !cameraPlaying, this);
        //connect(subjectSelectionDialog, SIGNAL (onSettingsChange()), singleCameraChildWidget, SLOT (onSettingsChange()));
        connect(singleCameraChildWidget, SIGNAL (doingPupilDetectionROIediting(bool)), pupilDetectionSettingsDialog, SLOT (onDisableProcModeSelector(bool)));

        child = new RestorableQMdiSubWindow(singleCameraChildWidget, "SingleCameraView", this);
        //SingleCameraView *child = new SingleCameraView(selectedCamera, pupilDetectionWorker, this);

    } else if(selectedCamera && (
        selectedCamera->getType() == CameraImageType::LIVE_STEREO_CAMERA || 
        selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE
        ) ) {
        stereoCameraChildWidget = new StereoCameraView(selectedCamera, pupilDetectionWorker, !cameraPlaying, this);
        //connect(subjectSelectionDialog, SIGNAL (onSettingsChange()), stereoCameraChildWidget, SLOT (onSettingsChange()));
        connect(stereoCameraChildWidget, SIGNAL (doingPupilDetectionROIediting(bool)), pupilDetectionSettingsDialog, SLOT (onDisableProcModeSelector(bool)));

        child = new RestorableQMdiSubWindow(stereoCameraChildWidget, "StereoCameraView", this);
    }

    mdiArea->addSubWindow(child);
    child->show();
    child->restoreGeometry();
    connect(child, SIGNAL (onCloseSubWindow()), this, SLOT (updateWindowMenu()));
    connect(child, &RestorableQMdiSubWindow::onCloseSubWindow, this, [this]() {
        if(cameraViewWindow) {
            cameraViewWindow->deleteLater();
            cameraViewWindow = nullptr;
        }
        if(selectedCamera && (
                selectedCamera->getType() == CameraImageType::LIVE_SINGLE_CAMERA ||
                selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE ||
                selectedCamera->getType() == CameraImageType::LIVE_SINGLE_WEBCAM
        ) ) {
            singleCameraChildWidget->deleteLater();
            singleCameraChildWidget = nullptr;
        } else if(selectedCamera && (
                selectedCamera->getType() == CameraImageType::LIVE_STEREO_CAMERA ||
                selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE
        ) ) {
            stereoCameraChildWidget->deleteLater();
            stereoCameraChildWidget = nullptr;
        }
    });
    cameraViewWindow = child;

    if(selectedCamera->getType() == CameraImageType::LIVE_SINGLE_WEBCAM)
        cameraViewWindow->setWindowIcon(SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/devices/22/camera-web.svg"), applicationSettings));
    else
        cameraViewWindow->setWindowIcon(singleCameraIcon);

    connectCameraPlaybackChangedSlotsForCameraViews();

    if(selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE && singleCameraChildWidget) {
        //cv::Mat temp1 = dynamic_cast<FileCamera*>(selectedCamera)->getStillImageSingle(0);
        //singleCameraChildWidget->displayStillImage(temp1);
//        singleCameraChildWidget->displayFileCameraFrame(0);
        singleCameraChildWidget->displayFileCameraFrame(dynamic_cast<FileCamera*>(selectedCamera)->getLastCommissionedFrameNumber());

        singleCameraChildWidget->installEventFilter(this);
    } else if(selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE && stereoCameraChildWidget) {
        //std::vector<cv::Mat> temp2 = dynamic_cast<FileCamera*>(selectedCamera)->getStillImageStereo(0);
        //stereoCameraChildWidget->displayStillImage(temp2);
//        stereoCameraChildWidget->displayFileCameraFrame(0);
        stereoCameraChildWidget->displayFileCameraFrame(dynamic_cast<FileCamera*>(selectedCamera)->getLastCommissionedFrameNumber());

        stereoCameraChildWidget->installEventFilter(this);
    }
}

void MainWindow::onCameraSettingsClick() {

    if(selectedCamera && singleCameraSettingsDialog && (
        selectedCamera->getType() == CameraImageType::LIVE_SINGLE_CAMERA) ) {
        //onSingleCameraSettingsClick();
        singleCameraSettingsDialog->show();
    } else if(selectedCamera && stereoCameraSettingsDialog && 
        selectedCamera->getType() == CameraImageType::LIVE_STEREO_CAMERA ) {
        //stereoCameraSelected();
        stereoCameraSettingsDialog->show();
    } else if(selectedCamera->getType() == CameraImageType::LIVE_SINGLE_WEBCAM) {
        singleWebcamSettingsDialog->show();
    }
}

void MainWindow::onSingleCameraSettingsClick() {
    singleCameraSettingsDialog = new SingleCameraSettingsDialog(dynamic_cast<SingleCamera*>(selectedCamera), MCUSettingsDialogInst, this);
#ifdef Q_OS_MACOS // Q_OS_WIN
    singleCameraSettingsDialog->setWindowFlags(Qt::Tool);
#endif
    singleCameraSettingsDialog->setWindowIcon(cameraSettingsIcon2);
//    singleCameraSettingsDialog->installEventFilter(this); // THIS HAS ITS OWN EVENTFILTER NOW
    //auto *child = new RestorableQMdiSubWindow(childWidget, "SingleCameraSettingsDialog", this);
    singleCameraSettingsDialog->show();

    connect(singleCameraSettingsDialog, &SingleCameraSettingsDialog::onMCUConfig, MCUSettingsDialogInst, &MCUSettingsDialog::show);
    //connect(subjectSelectionDialog, SIGNAL (onSettingsChange()), singleCameraSettingsDialog, SLOT (onSettingsChange()));

//    connect(MCUSettingsDialogInst, SIGNAL (onConnect()), singleCameraSettingsDialog, SLOT (onSerialConnect()));
//    connect(MCUSettingsDialogInst, SIGNAL (onDisconnect()), singleCameraSettingsDialog, SLOT (onSerialDisconnect()));

    connect(singleCameraSettingsDialog, SIGNAL (onHardwareTriggerStart(QString)), MCUSettingsDialogInst, SLOT (sendCommand(QString)));
    connect(singleCameraSettingsDialog, SIGNAL (onHardwareTriggerStop(QString)), MCUSettingsDialogInst, SLOT (sendCommand(QString)));

    connect(singleCameraSettingsDialog, SIGNAL (onHardwareTriggerEnable()), this, SLOT (onHwTriggerEnable()));
    connect(singleCameraSettingsDialog, SIGNAL (onHardwareTriggerDisable()), this, SLOT (onHwTriggerDisable()));

    connect(singleCameraSettingsDialog, SIGNAL(onImageROIChanged(QRect)), singleCameraChildWidget, SLOT(onImageROIChanged(QRect)));
    connect(singleCameraSettingsDialog, SIGNAL(onSensorSizeChanged(QSize)), singleCameraChildWidget, SLOT(onSensorSizeChanged(QSize)));

    // call these once again, to evoke a signal that will tell the camera view window where the image ROI is, upon window creation
    singleCameraSettingsDialog->updateImageROISettingsValues();
    singleCameraSettingsDialog->updateCamImageRegionsWidget();
    singleCameraSettingsDialog->updateSensorSize();

    connectCameraPlaybackChangedSlotsForCameraSettings();
}

void MainWindow::onSingleWebcamSettingsClick() {
    singleWebcamSettingsDialog = new SingleWebcamSettingsDialog(dynamic_cast<SingleWebcam*>(selectedCamera), this);
#ifdef Q_OS_MACOS // Q_OS_WIN
    singleWebcamSettingsDialog->setWindowFlags(Qt::Tool);
#endif
    //auto *child = new RestorableQMdiSubWindow(childWidget, "SingleWebcamSettingsDialog", this);
    singleWebcamSettingsDialog->show();

    //connect(subjectSelectionDialog, SIGNAL (onSettingsChange()), singleWebcamSettingsDialog, SLOT (onSettingsChange()));
    connectCameraPlaybackChangedSlotsForCameraSettings();
}

void MainWindow::onStereoCameraSettingsClick() {
    stereoCameraSettingsDialog = new StereoCameraSettingsDialog(dynamic_cast<StereoCamera*>(selectedCamera), MCUSettingsDialogInst, this);
#ifdef Q_OS_MACOS // Q_OS_WIN
    stereoCameraSettingsDialog->setWindowFlags(Qt::Tool);
#endif
    stereoCameraSettingsDialog->setWindowIcon(cameraSettingsIcon1);
//    stereoCameraSettingsDialog->installEventFilter(this); // THIS HAS ITS OWN EVENTFILTER NOW
    //auto *child = new RestorableQMdiSubWindow(childWidget, "StereoCameraSettingsDialog", this);
    stereoCameraSettingsDialog->show();

    connect(stereoCameraSettingsDialog, &StereoCameraSettingsDialog::onMCUConfig, MCUSettingsDialogInst, &MCUSettingsDialog::show);
    //connect(subjectSelectionDialog, SIGNAL (onSettingsChange()), stereoCameraSettingsDialog, SLOT (onSettingsChange()));

//    connect(MCUSettingsDialogInst, SIGNAL (onConnect()), stereoCameraSettingsDialog, SLOT (onSerialConnect()));
//    connect(MCUSettingsDialogInst, SIGNAL (onDisconnect()), stereoCameraSettingsDialog, SLOT (onSerialDisconnect()));

    connect(stereoCameraSettingsDialog, SIGNAL (onHardwareTriggerStart(QString)), MCUSettingsDialogInst, SLOT (sendCommand(QString)));
    connect(stereoCameraSettingsDialog, SIGNAL (onHardwareTriggerStop(QString)), MCUSettingsDialogInst, SLOT (sendCommand(QString)));

    connect(stereoCameraSettingsDialog, SIGNAL (onHardwareTriggerEnable()), this, SLOT (onHwTriggerEnable()));
    connect(stereoCameraSettingsDialog, SIGNAL (onHardwareTriggerDisable()), this, SLOT (onHwTriggerDisable()));

    connect(stereoCameraSettingsDialog, SIGNAL (stereoCamerasOpened()), this, SLOT (onStereoCamerasOpened()));
    connect(stereoCameraSettingsDialog, SIGNAL (stereoCamerasClosed()), this, SLOT (onStereoCamerasClosed()));

    connect(stereoCameraSettingsDialog, SIGNAL(onImageROIChanged(QRect)), stereoCameraChildWidget, SLOT(onImageROIChanged(QRect)));
    connect(stereoCameraSettingsDialog, SIGNAL(onSensorSizeChanged(QSize)), stereoCameraChildWidget, SLOT(onSensorSizeChanged(QSize)));

    // call these once again, to evoke a signal that will tell the camera view window where the image ROI is, upon window creation
    stereoCameraSettingsDialog->updateImageROISettingsValues();
    stereoCameraSettingsDialog->updateCamImageRegionsWidget();
    stereoCameraSettingsDialog->updateSensorSize();

    connectCameraPlaybackChangedSlotsForCameraSettings();
}

#ifdef USE_PYLON
Pylon::DeviceInfoList_t MainWindow::enumerateCameraDevices() {

    CTlFactory& TlFactory = CTlFactory::GetInstance();
    IGigETransportLayer* pTl = dynamic_cast<IGigETransportLayer*>(TlFactory.CreateTl( Pylon::BaslerGigEDeviceClass ));

    Pylon::DeviceInfoList_t allDevices;
    Pylon::DeviceInfoList_t lstDevices;
    TlFactory.EnumerateDevices(lstDevices);

    qDebug() << "lstDevices.size() = " << lstDevices.size();

    if (pTl == NULL) {
        qDebug() << "Error: No GigE transport layer installed.";
        qDebug() << "       Please install GigE support as it is required for this sample.";
        //return {};
        allDevices = lstDevices;
    } else {
        Pylon::DeviceInfoList_t lstDevicesGigE;
        pTl->EnumerateAllDevices(lstDevicesGigE);
        std::merge(lstDevices.begin(), lstDevices.end(), lstDevicesGigE.begin(), lstDevicesGigE.end(), std::back_inserter(allDevices));
    }
    return allDevices;
}
#else

// NOTE: If we are to retreive ArvDevice's, it would take very long, and not useful.
//  Retrieving just the device indexes (later usable for retrieving deviceIDS or anything) is enough.
uint MainWindow::enumerateCameraDevices() {

    // Weidrly, these are ALSO needed, or sometimes it does not detect devices.
    qDebug() << "arv_get_n_devices() = " << QString::number(arv_get_n_devices());
    qDebug() << "arv_get_n_interfaces() = " << QString::number(arv_get_n_interfaces());

    auto arvInterfaceInstanceUSB = arv_uv_interface_get_instance();
    auto arvInterfaceInstanceGigE = arv_gv_interface_get_instance();

    arv_interface_update_device_list(arvInterfaceInstanceUSB);
    arv_interface_update_device_list(arvInterfaceInstanceGigE);

    /*
    uint nInterfaces = arv_get_n_interfaces();
    for(uint cit = 0; cit < nInterfaces; cit++) {

        arv_interface_update_device_list(ArvInterface *interface);
    }
     */
    //arv_get_interface_id(unsigned int index);

    qDebug() << "arv_get_n_devices() = " << QString::number(arv_get_n_devices());

    // And this too
    uint n = 0;
    qDebug() << "Attempting to update Aravis device list.";
    arv_update_device_list(); // may be time consuming

    n = arv_get_n_devices();
    qDebug() << "Number of found devices: " << QString::number(n);

    /*
    for(int i = 0; i < n; i++) {
        std::string deviceID = arv_get_device_id(i);

        qDebug() << "arv_get_device_id() = " << arv_get_device_id(i);
        qDebug() << "arv_get_device_vendor() = " << arv_get_device_vendor(i);
        qDebug() << "arv_get_device_model() = " << arv_get_device_model(i);
        qDebug() << "arv_get_device_serial_nbr() = " << arv_get_device_serial_nbr(i);
        qDebug() << "arv_get_device_address() = " << arv_get_device_address(i);
        qDebug() << "arv_get_device_physical_id() = " << arv_get_device_physical_id(i);
        qDebug() << "arv_get_device_protocol() = " << arv_get_device_protocol(i);
        qDebug() << "arv_get_device_manufacturer_info() = " << arv_get_device_manufacturer_info(i);

        // Expected output:
        // arv_get_device_id() =  Basler-acA1300-60gm-22385478
        // arv_get_device_vendor() =  Basler
        // arv_get_device_model() =  acA1300-60gm
        // arv_get_device_serial_nbr() =  22385478
        // arv_get_device_address() =  100.1.1.100
        // arv_get_device_physical_id() =  00:30:53:24:66:46
        // arv_get_device_protocol() =  GigEVision
        // arv_get_device_manufacturer_info() =  none

        //ArvCamera* a = arv_camera_new(deviceID.c_str(), &error);
        //if(!error) {
        //    ArvDevice* d = arv_camera_get_device(a);
        //    // ...
        //    allDevices.append(d);
        //}
    }
    */

    return n;
}
#endif

MainWindow::~MainWindow() {

#ifdef USE_PYLON
    // Releases all pylon resources
    PylonTerminate();
#endif

    pupilDetectionThread->quit();
    pupilDetectionThread->wait();
}

void MainWindow::onCalibrateClick() {
    
    if(mdiArea->subWindowList().contains(calibrationWindow)) {
        calibrationWindow->show();
        calibrationWindow->raise();
        calibrationWindow->activateWindow();
        calibrationWindow->setFocus();
        if(calibrationWindow->isMinimized() || calibrationWindow->isShaded())
            calibrationWindow->showNormal();
    }
    else {
        loadCalibrationWindow();
    }
}

void MainWindow::onSharpnessClick() {

    if (mdiArea->subWindowList().contains(sharpnessWindow)){
        sharpnessWindow->show();
        sharpnessWindow->raise();
        sharpnessWindow->activateWindow();
        sharpnessWindow->setFocus();
        if(sharpnessWindow->isMinimized() || sharpnessWindow->isShaded())
            sharpnessWindow->showNormal();
        return;
    }

    loadSharpnessWindow();
}

void MainWindow::dataTableClick() {

    if (mdiArea->subWindowList().contains(dataTableWindow)){
        dataTableWindow->show();
        dataTableWindow->raise();
        dataTableWindow->activateWindow();
        dataTableWindow->setFocus();
        if(dataTableWindow->isMinimized() || dataTableWindow->isShaded())
            dataTableWindow->showNormal();
        return;
    }

    loadDataTableWindow();
}

void MainWindow::loadDataTableWindow() {
    // TODO: it may be possible that the user closes the dataTable window, and we just create a new one without freeing the former one
    // NOTE: removed code that decides if mode is stereo or single, as not necessary anymore, due to procMode

    DataTable *childWidget = new DataTable(pupilDetectionWorker->getCurrentProcMode(), this);
    //RestorableQMdiSubWindow *child = new RestorableQMdiSubWindow(childWidget, "DataTable", this);
    RestorableQMdiSubWindow *child = new RestorableQMdiSubWindow(childWidget, "DataTable"+QString::number(pupilDetectionWorker->getCurrentProcMode()), this);
    /*QMdiSubWindow *child = new QMdiSubWindow(this);
    child->setWidget(childWidget);
    child->setAttribute(Qt::WA_DeleteOnClose);
    QSize hint = childWidget->sizeHint();
    child->setGeometry(QRect(QPoint(0, 0), hint));*/

    if(selectedCamera) {
        connect(pupilDetectionWorker, SIGNAL (processedPupilDataLowFPS(quint64, int, std::vector<Pupil>)), childWidget, SLOT (onPupilData(quint64, int, std::vector<Pupil>)));
        //std::cout << "dataTableClick()" << std::endl;

        connect(signalPubSubHandler, SIGNAL(cameraFPS(double)), childWidget, SLOT(onCameraFPS(double)));
        assert(connect(signalPubSubHandler, SIGNAL(cameraFramecount(int)), childWidget, SLOT(onCameraFramecount(int))));
    }

    connect(pupilDetectionWorker, SIGNAL(fps(double)), childWidget, SLOT(onProcessingFPS(double)));
    connect(childWidget, SIGNAL(createGraphPlot(PDataType)), this, SLOT(onCreateGraphPlot(PDataType)));

    // TODO: make data table window adapt to the change
    connect(pupilDetectionSettingsDialog, SIGNAL (pupilDetectionProcModeChanged(int)), childWidget, SLOT (close()));

    if(imagePlaybackControlDialog) {
        connect(imagePlaybackControlDialog, SIGNAL(onPlaybackSafelyStopped()), childWidget, SLOT(scheduleReset()));
        connect(imagePlaybackControlDialog, SIGNAL(cameraPlaybackPositionChanged()), childWidget, SLOT(scheduleReset()));

        // In case one stops tracking, but playback goes on, clear the table to not display the old stuck pupil detection output
        connect(pupilDetectionWorker, SIGNAL(processingFinished()), childWidget, SLOT(scheduleReset()));
    }

    mdiArea->addSubWindow(child);
    child->show();
    child->restoreGeometry();
    // NOTE: I really tried many ways but this is still the only working.
    // MDI subwindows are only resizeable, and no minimum size can be given to them AFAIK
    childWidget->fitForTableSize();
    connect(child, SIGNAL (onCloseSubWindow()), this, SLOT (updateWindowMenu()));
    dataTableWindow = child;
    dataTableWindow->setWindowIcon(dataTableIcon);
}

void MainWindow::sceneImageViewClick() {

    if (mdiArea->subWindowList().contains(sceneImageWindow)){
        sceneImageWindow->show();
        sceneImageWindow->raise();
        sceneImageWindow->activateWindow();
        sceneImageWindow->setFocus();
        if(sceneImageWindow->isMinimized() || sceneImageWindow->isShaded())
            sceneImageWindow->showNormal();
        return;
    }

    loadSceneImageWindow();
}

void MainWindow::loadSceneImageWindow() {

    SceneImageView *childWidget = new SceneImageView(this);
    //RestorableQMdiSubWindow *child = new RestorableQMdiSubWindow(childWidget, "DataTable", this);
    RestorableQMdiSubWindow *child = new RestorableQMdiSubWindow(childWidget, "SceneImageView", this);
    /*QMdiSubWindow *child = new QMdiSubWindow(this);
    child->setWidget(childWidget);
    child->setAttribute(Qt::WA_DeleteOnClose);
    QSize hint = childWidget->sizeHint();
    child->setGeometry(QRect(QPoint(0, 0), hint));*/

    if(selectedCamera) {
        // connect the output of gaze mapper thread to the scene image window updateView
//        connect(pupilDetectionWorker, SIGNAL (processedPupilDataLowFPS(quint64, int, std::vector<Pupil>)), childWidget, SLOT (onPupilData(quint64, int, std::vector<Pupil>)));
    }

    mdiArea->addSubWindow(child);
    child->show();
    child->restoreGeometry();
    connect(child, SIGNAL (onCloseSubWindow()), this, SLOT (updateWindowMenu()));
    sceneImageWindow = child;
    sceneImageWindow->setWindowIcon(sceneImageViewIcon);
}

void MainWindow::toggleFullscreen() {
    if(this->isMaximized()) {
        this->showNormal();
    } else {
        this->showMaximized();
    }
    toggleFullscreenAct->setChecked(this->isMaximized());
}

void MainWindow::onCreateGraphPlot(const PDataType &value) {

    // Do not create duplicates
    QList<QMdiSubWindow *> windows = mdiArea->subWindowList();
    for(auto mdiSubWindow : windows) {
        // NOTE: The .mid(...) is necessary because we systematically name these plots, all of them begins
        // with the same string "Graph Plot: " (12 characters) and continues with the plotted value DataType key name
//        if(mdiSubWindow->windowTitle() == PDataTypes::map.value(value))
        if(mdiSubWindow->windowTitle().mid(12) == PDataTypes::tyf.at(value))
            return;
    }

    // Now create the Graph Plot window
    std::cout << "Created GraphPlot slot: " << PDataTypes::tyf.at(value).toStdString() << std::endl;

    GraphPlot* graphPlot = new GraphPlot(value, pupilDetectionWorker->getCurrentProcMode(), false, this);
    if(selectedCamera->getType() == SINGLE_IMAGE_FILE || selectedCamera->getType() == STEREO_IMAGE_FILE) {
        // NOTE: virtually it can happen that the newly created graphplot already has its connects set and receives a pupil detection signal
        // just before we set this known time zero in case of file camera, but it is unlikely, and worst case is that one data point is wrongly plotted
        graphPlot->setKnownTimeZero(dynamic_cast<FileCamera*>(selectedCamera)->getTimestampForFrameNumber(0));
    }
    QWidget *childWidget = graphPlot;
    auto *child = new RestorableQMdiSubWindow(childWidget, "GraphPlot_" + PDataTypes::tyf.at(value), this);
    child->setWindowIcon(SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/labplot-xy-interpolation-curve.svg"), applicationSettings));

    bool connSuccess = false;

    // switch values to connect different slots/signals to GraphPlot and the data
    /*if(value == DataTable::FRAME_NUMBER) {

        connSuccess = connect(signalPubSubHandler, SIGNAL (cameraFramecount(int)), childWidget, SLOT (appendData(int)));
    } else*/ if(value == PDataType::CAMERA_FPS) {

        connSuccess = connect(signalPubSubHandler, SIGNAL (cameraFPS(double)), childWidget, SLOT (appendData(double)));
    } else if(value == PDataType::PUPIL_FPS) {

        connSuccess = connect(pupilDetectionWorker, SIGNAL (fps(double)), childWidget, SLOT (appendData(double)));
    } else {
        connSuccess = connect(pupilDetectionWorker, SIGNAL (processedPupilDataLowFPS(quint64, int, std::vector<Pupil>)), childWidget, SLOT (appendData(quint64, int, std::vector<Pupil>)));
    }
    mdiArea->addSubWindow(child);
    child->show();
    child->restoreGeometry();
    connect(child, SIGNAL (onCloseSubWindow()), this, SLOT (updateWindowMenu()));

    // In case of proc mode change, we need to close the current graph window
    // TODO: make graph window adapt to the change
    connect(pupilDetectionSettingsDialog, SIGNAL (pupilDetectionProcModeChanged(int)), childWidget, SLOT (close()));

    if(imagePlaybackControlDialog) {
        connect(imagePlaybackControlDialog, SIGNAL(onPlaybackSafelyStopped()), childWidget, SLOT(scheduleReset()));
        connect(imagePlaybackControlDialog, SIGNAL(cameraPlaybackPositionChanged()), childWidget, SLOT(scheduleReset()));
    }
}

void MainWindow::onOpenImageRecordingClicked() {
    //QFileDialog dialog(this, tr("Image Directory"), recentPath,tr("Image Files (*.png *.jpg *.jpeg *.bmp *.tiff *.tif *.webp)"));
    QFileDialog dialog(
            this,
            tr("Open Image Recording"),
            recentImageReadingDirectory,
#ifdef QT_DEBUG
            tr("Any Supported (*.tiff *.tif *.png *.bmp *.jpeg *.jpg *.jpe *.jp2 *.webp *.pgm *.zip *.mkv);;Image Files (*.tiff *.tif *.png *.bmp *.jpeg *.jpg *.jpe *.jp2 *.webp *.pgm);;Zip Archive (*.zip);;Matroska Video Format (*.mkv)")
#else
            tr("Any Supported (*.tiff *.tif *.png *.bmp *.jpeg *.jpg *.jpe *.jp2 *.webp *.pgm *.zip *.mkv);;Image Files (*.tiff *.tif *.png *.bmp *.jpeg *.jpg *.jpe *.jp2 *.webp *.pgm);;Zip Archive (*.zip)")
#endif
            );

    /*
    QString selFilter = "Any Supported (*.tiff *.tif *.png *.bmp *.jpeg *.jpg *.jpe *.jp2 *.webp *.pgm *.zip)";
    QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("Image Directory"),
            recentPath,
            tr("Image Files (*.tiff *.tif *.png *.bmp *.jpeg *.jpg *.jpe *.jp2 *.webp *.pgm);;Zip Archive (*.zip);;Any Supported (*.tiff *.tif *.png *.bmp *.jpeg *.jpg *.jpe *.jp2 *.webp *.pgm *.zip)")
    );
     */
    dialog.setOptions(QFileDialog::DontResolveSymlinks); // BG: tried QFileDialog::DontUseNativeDialog flag too but it is slow. TODO: make own dialog

//    dialog.setOption(QFileDialog::ShowDirsOnly, true);
//    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, false);
    dialog.setFileMode(QFileDialog::ExistingFile);

    if(!dialog.exec())
        return;

    QString imageSource = "";
    if(dialog.selectedFiles().empty())
        return;

    QString selectedFile = dialog.selectedFiles()[0];
    if(selectedFile.endsWith("zip") || selectedFile.endsWith("mkv")) {
        // TODO: check if file can be read? even here
        imageSource = selectedFile;
    } else {
        // TODO: The below checks for the existence of images inside 0 and 1 folders, and the folders themselves,
        //  only happens in case of a directory. But they could also be done quickly for a selected Zip file.
        //  Implement that.

        imageSource = dialog.directory().absolutePath();
        if (imageSource.isEmpty())
            return;

        QDir imageDir(imageSource);
        //qDebug() << imageDir;
        if (imageDir.isEmpty())
            return;

        QStringList nameFilter = QStringList()
                << "*.tiff" << "*.tif" << "*.png" << "*.bmp" << "*.jpeg" << "*.jpg" << "*.jpe" << "*.jp3"
                << "*.webp" << "*.pgm";
        QStringList fileNames = imageDir.entryList(nameFilter, QDir::Files);
        QStringList folderNames = imageDir.entryList(QStringList() << "0" << "1", QDir::Dirs);
        if (fileNames.isEmpty() && folderNames.size() < 2)
            return;

        // Needed if we do not use the folder opener dialog, but the file opener dialog instead.
        QDir imageDirUp = imageDir;
        imageDirUp.cdUp();
        QStringList folderNamesUp = imageDirUp.entryList(QStringList() << "0" << "1", QDir::Dirs);

        //qDebug() << fileNames;

        if (folderNamesUp.size() == 2) {
            QDir stereo0Dir(imageDirUp.filePath("0"));
            if (stereo0Dir.isEmpty() || stereo0Dir.entryList(nameFilter, QDir::Files).isEmpty())
                return;
            QDir stereo1Dir(imageDirUp.filePath("1"));
            if (stereo1Dir.isEmpty() || stereo1Dir.entryList(nameFilter, QDir::Files).isEmpty())
                return;

            // if its really stereo
            imageSource = imageDirUp.absolutePath();
        }
        //qDebug() << tempDir;
    }

    // NOTE: Yet we only pass this string, as the openImageFileSource function should be callable by
    //  remote control commands or specified in CMD arguments, where before we have no other checks
    openImageFileSource(imageSource, 0);
}

void MainWindow::onExportRecSectionClicked() {

    // show dialog, and if it returns with "ok" response, do the export
    ExportRecSectionDialog *dialog = new ExportRecSectionDialog(
            "Export Recording Section",
            dynamic_cast<FileCamera*>(selectedCamera),
            this);
    dialog->setModal(true);
    // dialog->raise();
    if(dialog->exec() != QDialog::Accepted)
        return;

    auto resp = dialog->getResponse();

    if(resp != ExportRecSectionDialog::ExportRecSectionResponse::PERFORM)
        return;

    // ...

    bool success = recSectionExporter->prepareExport(
            selectedCamera->getImageROIwidth(),
            selectedCamera->getImageROIheight(),
            pupilDetectionWorker->getCurrentProcMode(),
            recEventTracker);
    if(!success)
        return;

    // TODO: this is ugly, we should just use a singleton class, also on on its own thread

    if(!trackingOn)
        connect(pupilDetectionWorker,
                   SIGNAL(processedImageLowFPS(CameraImage)),
                   recSectionExporter,
                   SLOT(onNewImage(CameraImage)));
    else
        connect(pupilDetectionWorker,
                SIGNAL (processedImageLowFPS(CameraImage, int, std::vector<cv::Rect>, std::vector<Pupil>)),
                recSectionExporter,
                SLOT (onNewImage(CameraImage, int, std::vector<cv::Rect>, std::vector<Pupil>)));

    if(recordOn)
        onRecordClick();
    if(streamOn)
        onStreamClick();

    trackAct->setEnabled(false);

    // TODO: connect some signals from the camera view window

    imagePlaybackControlDialog->startExportRecSection();

    exportingRecSection = true;
    resetStatus(true);

    // tell imagePlaybackControlDialog to rewind to set position, and play with export on.
    // importantly, tracking can be enabled or disabled on the fly, etc, everything is the
    // same as during regular playback, but with export now. Also importantly, do not re-loop, even if checked
    // Also, no data recording this time, it is suppressed while export is going. Add note text for this

}

void MainWindow::onExportAllowedToEnd() {
    exportingRecSection = false;
    // trackAct->setEnabled(true);
    resetStatus(true);
    if(!trackingOn)
        disconnect(pupilDetectionWorker,
                SIGNAL(processedImageLowFPS(CameraImage)),
                recSectionExporter,
                SLOT(onNewImage(CameraImage)));
    else
        disconnect(pupilDetectionWorker,
                SIGNAL (processedImageLowFPS(CameraImage, int, std::vector<cv::Rect>, std::vector<Pupil>)),
                recSectionExporter,
                SLOT (onNewImage(CameraImage, int, std::vector<cv::Rect>, std::vector<Pupil>)));
}

/*

void MainWindow::openImageFileSource(QString imageSource) {

    if(imageSource[imageSource.length()-1]=='/') {
        imageSource.chop(1);
    }
    QStringList lst = imageSource.split('/');
    if(lst.count() <= 1) {
        return;
    }

    QString recordingName = lst[lst.count()-1];
    QString recordingParentLocation = imageSource.chopped(recordingName.length());
    QString suggestedCSVPathAndName = recordingParentLocation + '/' + recordingName + ".csv";
    PRGsetCsvPathAndName(suggestedCSVPathAndName);

    if(selectedCamera) {
        selectedCamera->close();
        selectedCamera = nullptr;
    }
    onCameraCalibrationDisabled();
    resetStatus(true);

    //const int playbackSpeed = applicationSettings->value("playbackSpeed", generalSettingsDialog->getPlaybackSpeed()).toInt();
    //const bool playbackLoop = (bool) applicationSettings->value("playbackLoop", (int) generalSettingsDialog->getPlaybackLoop()).toInt();
    const int playbackSpeed = applicationSettings->value("playbackSpeed", 30).toInt();
    bool playbackLoop = SupportFunctions::readBoolFromQSettings("playbackLoop", true, applicationSettings);

    QString offlineEventLogFileName = recordingParentLocation + '/' + "offline_event_log.xml";
    std::cout << "expected offlineEventLogFileName = " << offlineEventLogFileName.toStdString() << std::endl;
    if(QFileInfo(offlineEventLogFileName).exists()) {
        recEventTracker = new RecEventTracker(offlineEventLogFileName);
        if(recEventTracker->isReady()) {
            //connect( ...
        } else {
            recEventTracker->deleteLater();
            recEventTracker = nullptr;
        }
    }
    safelyResetTrialCounter();
    safelyResetMessageRegister();

    // selectedCamera = new FileCamera(imageSource, 0, imageMutex, imagePublished, imageProcessed, playbackSpeed, playbackLoop, this)
    // std::cout<<"FileCamera created using playbackspeed [fps]: "<<playbackSpeed <<std::endl;
    while(
            (selectedCamera = new FileCamera(imageSource, 0, imageMutex, imagePublished, imageProcessed, playbackSpeed, playbackLoop, this)) &&
            !selectedCamera->isOpen()
            ) {
        if( dynamic_cast<FileCamera*>(selectedCamera)->getImageReaderStatus() == ImageReader::IMSTATUS_ZIP_INDECISIVE ){

        } if( dynamic_cast<FileCamera*>(selectedCamera)->getImageReaderStatus() == ImageReader::IMSTATUS_ERROR ){

        }
        selectedCamera->close();
        selectedCamera = nullptr;
    }
    std::cout<<"FileCamera created using playbackspeed [fps]: "<<playbackSpeed <<std::endl;

    connect(selectedCamera, SIGNAL(onNewGrabResult(CameraImage)), signalPubSubHandler, SIGNAL (onNewGrabResult(CameraImage)));
    connect(selectedCamera, SIGNAL(fps(double)), signalPubSubHandler, SIGNAL(cameraFPS(double)));
    connect(selectedCamera, SIGNAL(framecount(int)), signalPubSubHandler, SIGNAL(cameraFramecount(int)));

    if(selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE) {
        connect(dynamic_cast<FileCamera*>(selectedCamera)->getCameraCalibration(), SIGNAL (finishedCalibration()), this, SLOT (onCameraCalibrationEnabled()));
        connect(dynamic_cast<FileCamera*>(selectedCamera)->getCameraCalibration(), SIGNAL (unavailableCalibration()), this, SLOT (onCameraCalibrationDisabled()));

        int pmSingle = applicationSettings->value("PupilDetectionSettingsDialog.singleCam.procMode", ProcMode::SINGLE_IMAGE_ONE_PUPIL).toInt();
        if( pmSingle != ProcMode::SINGLE_IMAGE_ONE_PUPIL &&
            pmSingle != ProcMode::SINGLE_IMAGE_TWO_PUPIL // &&
            // pmSingle != ProcMode::MIRR_IMAGE_ONE_PUPIL
                )
            pmSingle = ProcMode::SINGLE_IMAGE_ONE_PUPIL;
        pupilDetectionWorker->setCurrentProcMode(pmSingle);
        // this line below is to ensure if an erroneous value was found in the QSettings ini, a good one gets in place
        applicationSettings->setValue("PupilDetectionSettingsDialog.singleCam.procMode", pmSingle);

    } else if(selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE) {
        connect(dynamic_cast<FileCamera*>(selectedCamera)->getStereoCameraCalibration(), SIGNAL (finishedCalibration()), this, SLOT (onCameraCalibrationEnabled()));
        connect(dynamic_cast<FileCamera*>(selectedCamera)->getStereoCameraCalibration(), SIGNAL (unavailableCalibration()), this, SLOT (onCameraCalibrationDisabled()));

        int pmStereo = applicationSettings->value("PupilDetectionSettingsDialog.stereoCam.procMode", ProcMode::STEREO_IMAGE_ONE_PUPIL).toInt();
        if( pmStereo != ProcMode::STEREO_IMAGE_ONE_PUPIL &&
            pmStereo != ProcMode::STEREO_IMAGE_TWO_PUPIL )
            pmStereo = ProcMode::STEREO_IMAGE_ONE_PUPIL;
        pupilDetectionWorker->setCurrentProcMode(pmStereo);
        // this line below is to ensure if an erroneous value was found in the QSettings ini, a good one gets in place
        applicationSettings->setValue("PupilDetectionSettingsDialog.stereoCam.procMode", pmStereo);
    }
    this->cameraPlaying = false;

    cameraViewClick(); // GB: moved here. Had to ensure that proc mode is correctly set before creating camera view (as not it relies on pupilDetection instance too)
    onCalibrateClick();

//    cameraSettingsAct->setEnabled(false);
//    cameraViewAct->setEnabled(true);
//    dataTableAct->setEnabled(true);

    // Basically only that pupilDetectionSettingsDialog knows which type of camera is connected
    pupilDetectionWorker->setCamera(selectedCamera);
    // NOTE: this line below calls loadSettings too
    // NOTE: importantly, this call must lead to calls in pupilDetectionSettingsDialog for
    // updateProcModeEnabled() and updateProcModeCompatibility()
    pupilDetectionSettingsDialog->onSettingsChange();

    // NOTE:
    // This must happen here, after cameraViewClick() call, because only then will a
    // singleCameraChildWidget or stereoCameraChildWidget exist in memory
    if(selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE) {
        connect(pupilDetectionSettingsDialog, SIGNAL (pupilDetectionProcModeChanged(int)), singleCameraChildWidget, SLOT (updateForPupilDetectionProcMode()));
    } else if(selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE) {
        connect(pupilDetectionSettingsDialog, SIGNAL (pupilDetectionProcModeChanged(int)), stereoCameraChildWidget, SLOT (updateForPupilDetectionProcMode()));
    }

    imagePlaybackControlDialog = new ImagePlaybackControlDialog(dynamic_cast<FileCamera*>(selectedCamera), pupilDetectionWorker, recEventTracker, this);
    RestorableQMdiSubWindow *imagePlaybackControlWindow = new RestorableQMdiSubWindow(imagePlaybackControlDialog, "ImagePlaybackControlDialog", this);
    imagePlaybackControlWindow->setWindowIcon(imagePlaybackControlIcon); // TODO: this somehow does not work
    mdiArea->addSubWindow(imagePlaybackControlWindow);
    //imagePlaybackControlWindow->resize(650, 230); // Min. size will set automatically anyways
    // No "X" button on this window
    imagePlaybackControlWindow->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowStaysOnTopHint);
    //imagePlaybackControlWindow->setWindowFlags(imagePlaybackControlWindow->windowFlags() & ~Qt::WindowCloseButtonHint);
    //imagePlaybackControlWindow->setWindowFlags( (Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint) & ~Qt::WindowCloseButtonHint );
    imagePlaybackControlWindow->show();
    //imagePlaybackControlWindow->restoreGeometry();

    //connect(selectedCamera, SIGNAL(finished()), imagePlaybackControlDialog, SLOT(onPlaybackFinished()));
    // GB: right now, this only gets called when playbackLoop is false, and we need to finish playing (with possible overhead)
    //connect(selectedCamera, SIGNAL(endReached()), imagePlaybackControlDialog, SLOT(onAutomaticFinish()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackStartInitiated()), this, SLOT(onPlaybackStartInitiated()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackPauseInitiated()), this, SLOT(onPlaybackPauseInitiated()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackStopInitiated()), this, SLOT(onPlaybackStopInitiated()));
    connect(this, SIGNAL(playbackStartApproved()), imagePlaybackControlDialog, SLOT(onPlaybackStartApproved()));
    connect(this, SIGNAL(playbackPauseApproved()), imagePlaybackControlDialog, SLOT(onPlaybackPauseApproved()));
    connect(this, SIGNAL(playbackStopApproved()), imagePlaybackControlDialog, SLOT(onPlaybackStopApproved()));

    connectCameraPlaybackChangedSlotsForCameraViews();

    playbackSynchroniser = new PlaybackSynchroniser();
    playbackSynchroniser->setCamera(selectedCamera);
    playbackSynchroniser->setPupilDetection(pupilDetectionWorker);


    connect(pupilDetectionWorker, SIGNAL(processingStarted()), playbackSynchroniser, SLOT(onPupilDetectionStarted()));
    connect(pupilDetectionWorker, SIGNAL(processingFinished()), playbackSynchroniser, SLOT(onPupilDetectionStopped()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackSafelyStarted()), playbackSynchroniser, SLOT(onPlaybackStarted()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackSafelyStopped()), playbackSynchroniser, SLOT(onPlaybackStopped()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackSafelyPaused()), playbackSynchroniser, SLOT(onPlaybackStopped()));


    // If everything went fine

    fileOpenAct->setEnabled(false);
    currentStatusMessageLabel->setText("Image file source: " + SupportFunctions::shortenStringForDisplay(imageSource, 100));
    currentStatusMessageLabel->setToolTip(imageSource);
    // We also store the recent path in QSettings
    setRecentPath(recordingParentLocation);

}

*/

void MainWindow::openImageFileSource(QString imageSource, int subrecordingNumber = 0) {

    if(imageSource[imageSource.length()-1]=='/') {
        imageSource.chop(1);
    }
    QStringList lst = imageSource.split('/');
    if(lst.count() <= 1) {
        return;
    }

    QString recordingName = lst[lst.count()-1];
    QString recordingParentLocation = imageSource.chopped(recordingName.length());
    QString recordingNameBase = recordingName;
    while(recordingNameBase.endsWith(".zip") && recordingNameBase.length() > 4)
        recordingNameBase = recordingName.mid(0, recordingName.lastIndexOf('.'));
    QString suggestedCSVPathAndName = recordingParentLocation + '/' + recordingNameBase + ".csv";
    PRGsetCsvPathAndName(suggestedCSVPathAndName);

    if(selectedCamera) {
        selectedCamera->close();
        selectedCamera = nullptr;
    }

    //const int playbackSpeed = applicationSettings->value("playbackSpeed", generalSettingsDialog->getPlaybackSpeed()).toInt();
    //const bool playbackLoop = (bool) applicationSettings->value("playbackLoop", (int) generalSettingsDialog->getPlaybackLoop()).toInt();
    const int playbackSpeed = applicationSettings->value("playbackSpeed", 30).toInt();
    bool playbackLoop = SupportFunctions::readBoolFromQSettings("playbackLoop", true, applicationSettings);

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // selectedCamera = new FileCamera(imageSource, 0, imageMutex, imagePublished, imageProcessed, playbackSpeed, playbackLoop, this)
    // std::cout<<"FileCamera created using playbackspeed [fps]: "<<playbackSpeed <<std::endl;
    while(  (selectedCamera = new FileCamera(imageSource, subrecordingNumber, imageMutex, imagePublished, imageProcessed, playbackSpeed, playbackLoop, this)) &&
            !selectedCamera->isOpen()   ) {

        if( dynamic_cast<FileCamera*>(selectedCamera)->getImageReaderStatus() == ImageReader::IMSTATUS_ZIP_INDECISIVE ){
            qDebug() << "Could not open this FileCamera, due to ImageReader error.";
            auto zipMultiInfo = dynamic_cast<FileCamera*>(selectedCamera)->getFoundZipMultiInfo();

            QApplication::restoreOverrideCursor();
            OpenZipChoiceDialog *dialog = new OpenZipChoiceDialog("Zip file contains multiple recordings", zipMultiInfo, this);
            dialog->setModal(true);
            // dialog->raise();
            if(dialog->exec() == QDialog::Accepted)
            {
                auto resp = dialog->getResponse();
                int selectedRecNumber = dialog->getSelectedRecNumber();

                if( resp == OpenZipChoiceDialog::OpenZipChoiceResponse::OPEN_SPECIFIC && abs(selectedRecNumber) <= zipMultiInfo.length() ) {
                    selectedCamera->close();
                    selectedCamera = nullptr;
                    subrecordingNumber = selectedRecNumber;
                    QApplication::setOverrideCursor(Qt::WaitCursor);
                    continue;
                } else /*if(resp == OpenZipChoiceDialog::OpenZipChoiceResponse::CANCEL)*/ {
                    selectedCamera->close();
                    selectedCamera = nullptr;
                    return;
                }
            }
        } if( dynamic_cast<FileCamera*>(selectedCamera)->getImageReaderStatus() == ImageReader::IMSTATUS_ERROR ){
            QApplication::restoreOverrideCursor();
            qDebug() << "Could not open this FileCamera, due to ImageReader error.";
            selectedCamera->close();
            selectedCamera = nullptr;
            return;
        } else if (dynamic_cast<FileCamera*>(selectedCamera)->getImageReaderStatus() == ImageReader::IMSTATUS_ZIP_UNOPENABLE) {
            QApplication::restoreOverrideCursor();
            QMessageBox *msgBox = new QMessageBox(this);
            msgBox->setWindowTitle("Zip archive could not be opened");
            msgBox->setText(
                    "This Zip archive could not be opened for reading. The archive file might be corrupted, empty, password protected, or it is compressed in an unknown format. Please check that PupilEXT has the permissions, and try again. In case you are sure this is an existing and accessible file, but you keep experiencing an opening issue, it does not mean that the archive is lost: the file might still contain a portion of its original contents, which could be retrieved by a proper extractor program.");
            msgBox->setMinimumSize(330, 280);
            msgBox->setIcon(QMessageBox::Warning);
            msgBox->setModal(false);
            msgBox->show();

            selectedCamera->close();
            selectedCamera = nullptr;
            return;
        } else if (dynamic_cast<FileCamera*>(selectedCamera)->getImageReaderStatus() == ImageReader::IMSTATUS_VIDEO_UNOPENABLE) {
            QApplication::restoreOverrideCursor();
            QMessageBox *msgBox = new QMessageBox(this);
            msgBox->setWindowTitle("Video file could not be opened");
            msgBox->setText(
                    "This video file could not be opened for reading. The file might be corrupted or it is encoded in an unknown format. Please check that PupilEXT has the permissions, and try again. In case you are sure this is an existing and accessible file, but you keep experiencing an opening issue, it does not mean that the content is completely lost: the file might still contain a portion of its original contents, which could be retrieved by a proper extractor program.");
            msgBox->setMinimumSize(330, 260);
            msgBox->setIcon(QMessageBox::Warning);
            msgBox->setModal(false);
            msgBox->show();

            selectedCamera->close();
            selectedCamera = nullptr;
            return;
        } else {
            break;
        }
        QApplication::restoreOverrideCursor();
        selectedCamera->close();
        selectedCamera = nullptr;
        return;
    }
    bool aha = selectedCamera->isOpen();
    QApplication::restoreOverrideCursor();
    std::cout<<"FileCamera created using playbackspeed [fps]: "<<playbackSpeed <<std::endl;

    // NOTE: FROM THIS POINT we can safely say that the camera is opened!

    onCameraCalibrationDisabled();
    resetStatus(true);

    if(dynamic_cast<FileCamera*>(selectedCamera)->getMetaSnapshotContent().isEmpty()) {
        // TODO: add tickbox to let the user disable this popup in the future

        QApplication::restoreOverrideCursor();
        QMessageBox *msgBox = new QMessageBox(this);
        msgBox->setWindowTitle("No image recording meta file found");
        msgBox->setText(
                "This recording seems to have no meta file attached.");
        msgBox->setMinimumSize(330, 260);
        msgBox->setIcon(QMessageBox::Information);
        msgBox->setModal(false);
        msgBox->show();
    }

    QString offlineEventLogContent = dynamic_cast<FileCamera*>(selectedCamera)->getOfflineEventLogContent();

    // Rec event tracker
    if(!offlineEventLogContent.isEmpty()) {
        recEventTracker = new RecEventTracker(offlineEventLogContent);
        if(recEventTracker->isReady()) {
            //connect( ...
        } else {
            recEventTracker->deleteLater();
            recEventTracker = nullptr;
        }
    } else {
        // TODO: add tickbox to let the user disable this popup in the future

        QApplication::restoreOverrideCursor();
        QMessageBox *msgBox = new QMessageBox(this);
        msgBox->setWindowTitle("No offline event log found");
        msgBox->setText(
                "This recording seems to have no event log attached. Trial numbering and message triggers are not loaded accordingly.");
        msgBox->setMinimumSize(330, 260);
        msgBox->setIcon(QMessageBox::Warning);
        msgBox->setModal(false);
        msgBox->show();
    }
    safelyResetTrialCounter();
    safelyResetMessageRegister();

    // GB: (old comment) moved here. Had to ensure that proc mode is correctly set before creating camera view (as now it relies on pupilDetection instance too)
    // TODO: this internally cascades to call connectCameraPlaybackChangedSlotsForCameraViews();, which is not efficient, as that is also called later.
    //  I think it is necessary to call it later (as well) because imagePlaybackControlDialog will get connected to the cameraView window as a result
    cameraViewClick();
    onCalibrateClick();

    // Connects etc.
    connect(selectedCamera, SIGNAL(onNewGrabResult(CameraImage)), signalPubSubHandler, SIGNAL (onNewGrabResult(CameraImage)));
    connect(selectedCamera, SIGNAL(fps(double)), signalPubSubHandler, SIGNAL(cameraFPS(double)));
    assert(connect(selectedCamera, SIGNAL(framecount(int)), signalPubSubHandler, SIGNAL(cameraFramecount(int))));

    if(selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE) {
        connect(dynamic_cast<FileCamera*>(selectedCamera)->getCameraCalibration(), SIGNAL (finishedCalibration()), this, SLOT (onCameraCalibrationEnabled()));
        connect(dynamic_cast<FileCamera*>(selectedCamera)->getCameraCalibration(), SIGNAL (unavailableCalibration()), this, SLOT (onCameraCalibrationDisabled()));

        int pmSingle = applicationSettings->value("PupilDetectionSettingsDialog.singleCam.procMode", ProcMode::SINGLE_IMAGE_ONE_PUPIL).toInt();
        if( pmSingle != ProcMode::SINGLE_IMAGE_ONE_PUPIL &&
            pmSingle != ProcMode::SINGLE_IMAGE_TWO_PUPIL // &&
            // pmSingle != ProcMode::MIRR_IMAGE_ONE_PUPIL
                )
            pmSingle = ProcMode::SINGLE_IMAGE_ONE_PUPIL;
        pupilDetectionWorker->setCurrentProcMode(pmSingle);
        // this line below is to ensure if an erroneous value was found in the QSettings ini, a good one gets in place
        applicationSettings->setValue("PupilDetectionSettingsDialog.singleCam.procMode", pmSingle);

    } else if(selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE) {
        connect(dynamic_cast<FileCamera*>(selectedCamera)->getStereoCameraCalibration(), SIGNAL (finishedCalibration()), this, SLOT (onCameraCalibrationEnabled()));
        connect(dynamic_cast<FileCamera*>(selectedCamera)->getStereoCameraCalibration(), SIGNAL (unavailableCalibration()), this, SLOT (onCameraCalibrationDisabled()));

        int pmStereo = applicationSettings->value("PupilDetectionSettingsDialog.stereoCam.procMode", ProcMode::STEREO_IMAGE_ONE_PUPIL).toInt();
        if( pmStereo != ProcMode::STEREO_IMAGE_ONE_PUPIL &&
            pmStereo != ProcMode::STEREO_IMAGE_TWO_PUPIL )
            pmStereo = ProcMode::STEREO_IMAGE_ONE_PUPIL;
        pupilDetectionWorker->setCurrentProcMode(pmStereo);
        // this line below is to ensure if an erroneous value was found in the QSettings ini, a good one gets in place
        applicationSettings->setValue("PupilDetectionSettingsDialog.stereoCam.procMode", pmStereo);
    }
    this->cameraPlaying = false;

//    cameraSettingsAct->setEnabled(false);
//    cameraViewAct->setEnabled(true);
//    dataTableAct->setEnabled(true);

    // Basically only that pupilDetectionSettingsDialog knows which type of camera is connected
    pupilDetectionWorker->setCamera(selectedCamera);
    // NOTE: this line below calls loadSettings too
    // NOTE: importantly, this call must lead to calls in pupilDetectionSettingsDialog for
    // updateProcModeEnabled() and updateProcModeCompatibility()
    pupilDetectionSettingsDialog->onSettingsChange();

    // NOTE:
    // This must happen here, after cameraViewClick() call, because only then will a
    // singleCameraChildWidget or stereoCameraChildWidget exist in memory
    if(selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE) {
        connect(pupilDetectionSettingsDialog, SIGNAL (pupilDetectionProcModeChanged(int)), singleCameraChildWidget, SLOT (updateForPupilDetectionProcMode()));
    } else if(selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE) {
        connect(pupilDetectionSettingsDialog, SIGNAL (pupilDetectionProcModeChanged(int)), stereoCameraChildWidget, SLOT (updateForPupilDetectionProcMode()));
    }

    imagePlaybackControlDialog = new ImagePlaybackControlDialog(dynamic_cast<FileCamera*>(selectedCamera), pupilDetectionWorker, recEventTracker, this);
    RestorableQMdiSubWindow *imagePlaybackControlWindow = new RestorableQMdiSubWindow(imagePlaybackControlDialog, "ImagePlaybackControlDialog", this);
    imagePlaybackControlWindow->setWindowIcon(imagePlaybackControlIcon); // TODO: this somehow does not work
    mdiArea->addSubWindow(imagePlaybackControlWindow);
    //imagePlaybackControlWindow->resize(650, 230); // Min. size will set automatically anyways
    // No "X" button on this window
    imagePlaybackControlWindow->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowStaysOnTopHint);
    //imagePlaybackControlWindow->setWindowFlags(imagePlaybackControlWindow->windowFlags() & ~Qt::WindowCloseButtonHint);
    //imagePlaybackControlWindow->setWindowFlags( (Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint) & ~Qt::WindowCloseButtonHint );
    imagePlaybackControlWindow->show();
    //imagePlaybackControlWindow->restoreGeometry();

    //connect(selectedCamera, SIGNAL(finished()), imagePlaybackControlDialog, SLOT(onPlaybackFinished()));
    // GB: right now, this only gets called when playbackLoop is false, and we need to finish playing (with possible overhead)
    //connect(selectedCamera, SIGNAL(endReached()), imagePlaybackControlDialog, SLOT(onAutomaticFinish()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackStartInitiated()), this, SLOT(onPlaybackStartInitiated()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackPauseInitiated()), this, SLOT(onPlaybackPauseInitiated()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackStopInitiated()), this, SLOT(onPlaybackStopInitiated()));
    connect(this, SIGNAL(playbackStartApproved()), imagePlaybackControlDialog, SLOT(onPlaybackStartApproved()));
    connect(this, SIGNAL(playbackPauseApproved()), imagePlaybackControlDialog, SLOT(onPlaybackPauseApproved()));
    connect(this, SIGNAL(playbackStopApproved()), imagePlaybackControlDialog, SLOT(onPlaybackStopApproved()));

    // TODO: disconnects?
    assert( connect(imagePlaybackControlDialog, SIGNAL(exportAllowedToEnd()), this, SLOT(onExportAllowedToEnd())) );
    assert( connect(imagePlaybackControlDialog, SIGNAL(exportAllowedToStart()), recSectionExporter, SLOT(onExportAllowedToStart())) );
    assert( connect(imagePlaybackControlDialog, SIGNAL(exportAllowedToEnd()), recSectionExporter, SLOT(onExportAllowedToEnd())) );
    // starting the exportRecSection is done by call, only result is watched here in mainwindow

    /*
    // GB: (old comment) moved here. Had to ensure that proc mode is correctly set before creating camera view (as now it relies on pupilDetection instance too)
    // GB: (later comment) Had to move it more way down, here. As it internally calls connectCameraPlaybackChangedSlotsForCameraViews(); already, hence
    //  connecting signals of imagePlaybackController, which has to exist at this point
    cameraViewClick();
    onCalibrateClick();
    */
    connectCameraPlaybackChangedSlotsForCameraViews();

    playbackSynchroniser = new PlaybackSynchroniser();
    playbackSynchroniser->setCamera(selectedCamera);
    playbackSynchroniser->setPupilDetection(pupilDetectionWorker);


    connect(pupilDetectionWorker, SIGNAL(processingStarted()), playbackSynchroniser, SLOT(onPupilDetectionStarted()));
    connect(pupilDetectionWorker, SIGNAL(processingFinished()), playbackSynchroniser, SLOT(onPupilDetectionStopped()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackSafelyStarted()), playbackSynchroniser, SLOT(onPlaybackStarted()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackSafelyStopped()), playbackSynchroniser, SLOT(onPlaybackStopped()));
    connect(imagePlaybackControlDialog, SIGNAL(onPlaybackSafelyPaused()), playbackSynchroniser, SLOT(onPlaybackStopped()));


    // If everything went fine

    // NOTE: these are already dealt with, in resetStatus(true);
//    fileOpenAct->setEnabled(false);
//    exportRecSectionAct->setEnabled(true);

    currentStatusMessageLabel->setText("Image file source: " + SupportFunctions::shortenStringForDisplay(imageSource, 100));
    currentStatusMessageLabel->setToolTip(imageSource);
    // We also store the recent path in QSettings
    setRecentImageReadingDirectory(recordingParentLocation);

    resetStatus(true);

    // DEV: 2026.03.16
    updateRois();
    if(stereoCameraChildWidget && (selectedCamera->getType() == CameraImageType::LIVE_STEREO_CAMERA || selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE)) {
        stereoCameraChildWidget->updateForPupilDetectionProcMode();
//        stereoCameraChildWidget->update();
    } else if(singleCameraChildWidget && (selectedCamera->getType() == CameraImageType::LIVE_SINGLE_CAMERA || selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE)) {
        singleCameraChildWidget->updateForPupilDetectionProcMode();
//        singleCameraChildWidget->update();
    }

}

void MainWindow::onPlaybackStartInitiated() {
    bool syncRecordCsv = SupportFunctions::readBoolFromQSettings("syncRecordCsv", imagePlaybackControlDialog->getSyncRecordCsv(), applicationSettings);
    bool syncStream = SupportFunctions::readBoolFromQSettings("syncStream", imagePlaybackControlDialog->getSyncStream(), applicationSettings);

    if(syncRecordCsv && trackingOn && !dataRecordingOutputTarget.isEmpty() && !recordOn) {
        onRecordClick();
    }

    if(syncStream && trackingOn && !streamOn) {
        onStreamClick();
    }
    emit playbackStartApproved();
}

void MainWindow::onPlaybackPauseInitiated() {
    bool syncRecordCsv = SupportFunctions::readBoolFromQSettings("syncRecordCsv", imagePlaybackControlDialog->getSyncRecordCsv(), applicationSettings);
    bool syncStream = SupportFunctions::readBoolFromQSettings("syncStream", imagePlaybackControlDialog->getSyncStream(), applicationSettings);

    if(syncRecordCsv && trackingOn && recordOn) {
        onRecordClick();
    }
    if(syncStream && trackingOn && streamOn) {
        onStreamClick();
    }
    emit playbackPauseApproved();
}

// yet this is the same as onPlaybackPauseInitiated(), except the signal emitted
void MainWindow::onPlaybackStopInitiated() {
    bool syncRecordCsv = SupportFunctions::readBoolFromQSettings("syncRecordCsv", imagePlaybackControlDialog->getSyncRecordCsv(), applicationSettings);
    bool syncStream = SupportFunctions::readBoolFromQSettings("syncStream", imagePlaybackControlDialog->getSyncStream(), applicationSettings);

    if(syncRecordCsv && trackingOn && recordOn) {
        onRecordClick();
    }
    if(syncStream && trackingOn && streamOn) {
        onStreamClick();
    }
    emit playbackStopApproved();
}

// GB: onPlayImageDirectoryClick() and onStopImageDirectoryClick() and onPlayImageDirectoryFinished()
// were here, but their functionality has been moved to ImagePlaybackControlDialog

void MainWindow::onSerialConnect() {
    const QIcon offlineIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/emblems/22/vcs-normal.svg"), applicationSettings);
    serialStatusIcon->setPixmap(offlineIcon.pixmap(12, 12));
    serialStatusIcon->setToolTip("Microcontroller connection is live");
}

void MainWindow::onSerialDisconnect() {
    const QIcon offlineIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/actions/22/media-record.svg"), applicationSettings);
    serialStatusIcon->setPixmap(offlineIcon.pixmap(16, 16));
    serialStatusIcon->setToolTip("Microcontroller connection is not established");
}

void MainWindow::onHwTriggerEnable() {
    hwTriggerOn = true;
    const QIcon offlineIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/emblems/22/vcs-normal.svg"), applicationSettings);
    hwTriggerStatusIcon->setPixmap(offlineIcon.pixmap(12, 12));
    hwTriggerStatusIcon->setToolTip("Hardware triggering is running");
}

void MainWindow::onHwTriggerDisable() {
    hwTriggerOn = false;
    const QIcon offlineIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/actions/22/media-record.svg"), applicationSettings);
    hwTriggerStatusIcon->setPixmap(offlineIcon.pixmap(16, 16));
    hwTriggerStatusIcon->setToolTip("Hardware triggering is not running");
}

void MainWindow::onDeviceWarmupHasDeltaTimeData() {
    warmedUpStatusIcon->setEnabled(true);
}

void MainWindow::onDeviceWarmedUp() {
    warmedUpStatusIcon->setEnabled(true);
    const QIcon warmupIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/emblems/22/vcs-normal.svg"), applicationSettings);
    warmedUpStatusIcon->setPixmap(warmupIcon.pixmap(12, 12));
    warmedUpStatusIcon->setToolTip("Device is warmed up");
}

void MainWindow::onDeviceWarmUpReadingsInvalid() {
    warmedUpStatusIcon->setEnabled(true);
    const QIcon warmupIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/emblems/22/vcs-conflicting.svg"), applicationSettings);
    warmedUpStatusIcon->setPixmap(warmupIcon.pixmap(12, 12));
    warmedUpStatusIcon->setToolTip("Device temperature readings are invalid");
}

void MainWindow::onDeviceWarmUpReadingsUnavailable() {
    warmedUpStatusIcon->setEnabled(true);
    const QIcon warmupIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/vcs-question.svg"), applicationSettings);
    warmedUpStatusIcon->setPixmap(warmupIcon.pixmap(12, 12));
    warmedUpStatusIcon->setToolTip("Device temperature readings are not available");
}

void MainWindow::onDeviceWarmedUpReset() {
    const QIcon warmupIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/actions/22/media-record.svg"), applicationSettings);
    warmedUpStatusIcon->setPixmap(warmupIcon.pixmap(16, 16));
    warmedUpStatusIcon->setEnabled(false);
    warmedUpStatusIcon->setToolTip("Warmup indication will appear here");
}

void MainWindow::onCameraCalibrationEnabled() {
    const QIcon calibIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/emblems/22/vcs-normal.svg"), applicationSettings);
    calibrationStatusIcon->setPixmap(calibIcon.pixmap(12, 12));
    calibrationStatusIcon->setToolTip("Camera calibration is loaded");
}

void MainWindow::onCameraCalibrationDisabled() {
    const QIcon calibIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/actions/22/media-record.svg"), applicationSettings);
    calibrationStatusIcon->setPixmap(calibIcon.pixmap(16, 16));
    calibrationStatusIcon->setToolTip("Camera calibration is not loaded");
}


void MainWindow::onGeneralSettingsChange() {

    this->repaint();

    if(pupilDetectionSettingsDialog)
        pupilDetectionSettingsDialog->repaint();

    //if(setupGeometryDialog)
    //    setupGeometryDialog->repaint();

    if(singleCameraSettingsDialog)
        singleCameraSettingsDialog->repaint();

    if(stereoCameraSettingsDialog)
        stereoCameraSettingsDialog->repaint();
}

void MainWindow::onPupilDetectionProcModeChange(int procMode) {
    // Close dataTable instance if exists, and any graphPlot instances if exist
    QList<QMdiSubWindow *> windows = mdiArea->subWindowList();
    for(auto mdiSubWindow : windows) {
        // NOTE: The .mid(...) is necessary because we systematically name these plots, all of them begins
        // with the same string "Graph Plot: " (12 characters) and continues with the plotted value DataType key name
        if(mdiSubWindow->windowTitle().mid(0,12) == "Graph Plot: ") {
            mdiSubWindow->close();
            mdiSubWindow->deleteLater();
        } else if(mdiSubWindow->windowTitle() == "Data Table") {
            mdiSubWindow->close();
            mdiSubWindow->deleteLater();
            dataTableWindow = nullptr;
        }
    }
}

/*
void MainWindow::onSubjectsSettingsChange(QString subject) {

    subjectConfigurationLabel->setText("Current configuration: " + subject);
}
*/

/*
void MainWindow::onSubjectsClick() {
    const QIcon subjectsSelectedIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/im-user-online.svg"), applicationSettings); //QIcon::fromTheme("camera-video");
    //subjectsAct->setIcon(subjectsSelectedIcon);

    subjectSelectionDialog->show();
}
*/

void MainWindow::resetGeometry() {
    this->showMaximized();
    toggleFullscreenAct->setChecked(this->isMaximized());

    QList<QMdiSubWindow *> windows = mdiArea->subWindowList();
    for(auto mdiSubWindow : windows) {
        auto *child = dynamic_cast<RestorableQMdiSubWindow *>(mdiSubWindow);

        child->resetGeometry();
    }
}

//void MainWindow::closeActiveSubWindow() {
//    if(imagePlaybackControlDialog && mdiArea->activeSubWindow() == imagePlaybackControlDialog->parent())
//        return;
//
//    mdiArea->closeActiveSubWindow();
//}
//
//void MainWindow::closeAllSubWindows() {
//    QList<QMdiSubWindow *> windows = mdiArea->subWindowList();
//    for(auto mdiSubWindow : windows) {
//        auto *child = dynamic_cast<RestorableQMdiSubWindow *>(mdiSubWindow);
//
//        if(imagePlaybackControlDialog && child == imagePlaybackControlDialog->parent())
//            continue;
//
//        child->close();
//    }
//}

// only to be called from GUI
void MainWindow::incrementTrialCounter() {
    // This function is to be called only from GUI interaction.
    quint64 timestamp  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    incrementTrialCounter(timestamp);
}

void MainWindow::incrementTrialCounter(const quint64 &timestamp) {
    if(!selectedCamera || (selectedCamera && (selectedCamera->getType()==SINGLE_IMAGE_FILE || selectedCamera->getType()==STEREO_IMAGE_FILE)) || !recEventTracker )
        return;
//    trialCounter->incrementTrial();
//    if(offlineEventLogWriter)
//        offlineEventLogWriter->incrementTrial();
    // if a camera connected exists, there shoudl always be a non-nullptr recEventTracker
    //recEventTracker->addTrialIncrement(timestamp);
    emit commitTrialCounterIncrement(timestamp);
    updateCurrentTrialLabel();
}

// TODO: let the user place a message from GUI too
void MainWindow::logRemoteMessage(const quint64 &timestamp, const QString &str) {
    if(!selectedCamera || (selectedCamera && (selectedCamera->getType()==SINGLE_IMAGE_FILE || selectedCamera->getType()==STEREO_IMAGE_FILE)) || !recEventTracker )
        return;
    emit commitRemoteMessage(timestamp, str);
    updateCurrentMessageLabel();
}

void MainWindow::updateCurrentTrialLabel() {
    if(recEventTracker) {
        unsigned int num = recEventTracker->getLastCommissionedTrialNumber();
        //currentTrialLabel->setText(QString::number(trialCounter->value()));
        currentTrialLabel->setText(QString::number(num));
        // It is important to emphasize the trial counter value by colour, as they can instantly catch the attention of
        // the experimenter if trigger signals are not arriving from the experiment computer
        if(num%2==0) {
            trialWidget->setStyleSheet("background-color: #ebd234; color: #000000; border: 1px #000000;");
        } else if(num==1) {
            trialWidget->setStyleSheet("background-color: #ffffff; color: #000000; border: 1px #000000;");
        } else {
            trialWidget->setStyleSheet("background-color: #19fac2; color: #000000; border: 1px #000000;");
        }
        // NOTE: currently the label doe not show the trial number associated with the displayed image,
        // (can be lagging due to high fps) but the trial number that will be associated with the 
        // currently grabbed frames
    } else {
        currentTrialLabel->setText("-");
        trialWidget->setStyleSheet(styleSheet());
    }
}

void MainWindow::updateCurrentMessageLabel() {
    if(recEventTracker) {
        QString str = recEventTracker->getLastMessage();
        currentMessageLabel->setText(SupportFunctions::shortenStringForDisplay(str,20));
    } else {
        currentMessageLabel->setText("-");
    }
}

// Only called when someone wants to start a new recording.
// This function is to be called only from GUI interaction.
void MainWindow::safelyResetTrialCounter() {
    quint64 timestamp  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    safelyResetTrialCounter(timestamp);
}

// Only called when someone wants to start a new recording.
// This function is to be called only from GUI interaction.
void MainWindow::safelyResetMessageRegister() {
    quint64 timestamp  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    safelyResetMessageRegister(timestamp);
}

// Only called when someone wants to start a new recording.
void MainWindow::safelyResetTrialCounter(const quint64 &timestamp) {
    if(streamOn || recordOn || !recEventTracker)
        return;
    //recEventTracker->resetBufferTrialCounter(timestamp);
    emit commitTrialCounterReset(timestamp);
    updateCurrentTrialLabel();
}

// Only called when someone wants to start a new recording.
void MainWindow::safelyResetMessageRegister(const quint64 &timestamp) {
    if(streamOn || recordOn || !recEventTracker)
        return;
    //recEventTracker->resetBufferMessageRegister(timestamp);
    emit commitMessageRegisterReset(timestamp);
    updateCurrentMessageLabel();
}

void MainWindow::forceResetTrialCounter() {
    quint64 timestamp  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    forceResetTrialCounter(timestamp);
}

// Can be used when e.g. there is a streaming going on, but the experiment PC sends a remote control command
// to start a new csv recording. As streaming is on, trial counter would not automatically reset,
// But if we call this method, it will.
// This also can be called from general settings
void MainWindow::forceResetTrialCounter(const quint64 &timestamp) {
    if(!recEventTracker)
        return;
    //recEventTracker->resetBufferTrialCounter(timestamp);
    emit commitTrialCounterReset(timestamp);
    updateCurrentTrialLabel();
}

void MainWindow::forceResetMessageRegister() {
    quint64 timestamp  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    forceResetMessageRegister(timestamp);
}

void MainWindow::forceResetMessageRegister(const quint64 &timestamp) {
    if(!recEventTracker)
        return;
    //recEventTracker->resetBufferMessageRegister(timestamp);
    emit commitMessageRegisterReset(timestamp);
    updateCurrentMessageLabel();
}

void MainWindow::onStreamingUDPConnect() {
    // TODO: minek volt ez itt? A streamer elindítása a stream gomb feladata.
    //  Azt pedig elvégzi vagy a valós kattintás vagy a PRG command, a connectet pedig
    //  belső signal megoldja a streamingsettingsdialog-on belül. Ez nem kell
    //if(dataStreamer) {
    //    dataStreamer->startUDPStreamer(
    //            streamingSettingsDialog->getConnPoolUDPIndex(),
    //            applicationSettings->value("StreamingSettings.UDP.sampleRate", 30).toInt(),
    //            streamingSettingsDialog->getDataContainerUDP() );
    //    streamingSettingsDialog->setLimitationsWhileStreamingUDP(true);
    //}
    streamAct->setEnabled(trackingOn && !exportingRecSection);
}

void MainWindow::onStreamingUDPDisconnect() {
    if(dataStreamer) {
        dataStreamer->stopUDPStreamer();
        streamingSettingsDialog->setLimitationsWhileStreamingUDP(false);
        if(dataStreamer->getNumActiveStreamers() == 0) {
            onStreamClick(); // close streamer
            streamAct->setEnabled(false);
        }
    }

    streamAct->setEnabled(trackingOn && streamingSettingsDialog && streamingSettingsDialog->isAnyConnected() && !exportingRecSection);
}

void MainWindow::onStreamingCOMConnect() {
    // TODO: minek volt ez itt? A streamer elindítása a stream gomb feladata.
    //  Azt pedig elvégzi vagy a valós kattintás vagy a PRG command, a connectet pedig
    //  belső signal megoldja a streamingsettingsdialog-on belül. Ez nem kell
    //if(dataStreamer) {
    //    dataStreamer->startCOMStreamer(
    //            streamingSettingsDialog->getConnPoolCOMIndex(),
    //            applicationSettings->value("StreamingSettings.COM.sampleRate", 30).toInt(),
    //            streamingSettingsDialog->getDataContainerCOM() );
    //    streamingSettingsDialog->setLimitationsWhileStreamingCOM(true);
    //}
    streamAct->setEnabled(trackingOn && !exportingRecSection);
}

void MainWindow::onStreamingCOMDisconnect() {
    if(dataStreamer) {
        dataStreamer->stopCOMStreamer();
        streamingSettingsDialog->setLimitationsWhileStreamingCOM(false);
        if(dataStreamer->getNumActiveStreamers() == 0) {
            onStreamClick(); // close streamer
            streamAct->setEnabled(false);
        }
    }

    streamAct->setEnabled(trackingOn && streamingSettingsDialog && streamingSettingsDialog->isAnyConnected() && !exportingRecSection);
}

#ifdef USE_LSL
void MainWindow::onStreamingLSLConnect() {
    // TODO: minek volt ez itt? A streamer elindítása a stream gomb feladata.
    //  Azt pedig elvégzi vagy a valós kattintás vagy a PRG command, a connectet pedig
    //  belső signal megoldja a streamingsettingsdialog-on belül. Ez nem kell
    //if(dataStreamer) {
    //    dataStreamer->startLSLStreamer(
    //            applicationSettings->value("StreamingSettings.LSL.sampleRate", 30).toInt(),
    //            streamingSettingsDialog->getDataContainerLSL(),
    //            pupilDetectionWorker->getCurrentProcMode()
    //            );
    //    streamingSettingsDialog->setLimitationsWhileStreamingLSL(true);
    //}
    streamAct->setEnabled(trackingOn && !exportingRecSection);
}

void MainWindow::onStreamingLSLDisconnect() {
    if(dataStreamer) {
        dataStreamer->stopLSLStreamer();
        streamingSettingsDialog->setLimitationsWhileStreamingLSL(false);
        if(dataStreamer->getNumActiveStreamers() == 0) {
            onStreamClick(); // close streamer
            streamAct->setEnabled(false);
        }
    }

    streamAct->setEnabled(trackingOn && streamingSettingsDialog && streamingSettingsDialog->isAnyConnected() && !exportingRecSection);
}
#endif

void MainWindow::onRemoteConnStateChanged() {
    if(remoteCCDialog->isAnyConnected()) {
        const QIcon offlineIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/emblems/22/vcs-normal.svg"), applicationSettings);
        remoteStatusIcon->setPixmap(offlineIcon.pixmap(12, 12));
    } else {
        const QIcon offlineIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":icons/Breeze/actions/22/media-record.svg"), applicationSettings);
        remoteStatusIcon->setPixmap(offlineIcon.pixmap(16, 16));
    }
}

void MainWindow::loadCalibrationWindow(){
    QWidget *calibrationDialog;
    QString calibrationDialogName;
    if(selectedCamera && selectedCamera->getType() == CameraImageType::LIVE_SINGLE_CAMERA) {
        calibrationDialog = new SingleCameraCalibrationView(dynamic_cast<SingleCamera*>(selectedCamera), this);
        calibrationDialogName = "SingleCameraCalibrationView";
    } else if(selectedCamera && selectedCamera->getType() == CameraImageType::LIVE_SINGLE_WEBCAM) {
        calibrationDialog = new SingleWebcamCalibrationView(dynamic_cast<SingleWebcam*>(selectedCamera), this);
        calibrationDialogName = "SingleWebcamCalibrationView";
    } else if(selectedCamera && selectedCamera->getType() == CameraImageType::LIVE_STEREO_CAMERA) {
        calibrationDialog = new StereoCameraCalibrationView(dynamic_cast<StereoCamera*>(selectedCamera), this);
        calibrationDialogName = "StereoCameraCalibrationView";
    } else if(selectedCamera && selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE) {
        calibrationDialog = new SingleFileCameraCalibrationView(dynamic_cast<FileCamera*>(selectedCamera), this);
        calibrationDialogName = "SingleFileCameraCalibrationView";
    } else if(selectedCamera && selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE) {
        calibrationDialog = new StereoFileCameraCalibrationView(dynamic_cast<FileCamera*>(selectedCamera), this);
        calibrationDialogName = "StereoFileCameraCalibrationView";
    }

    RestorableQMdiSubWindow *child = new RestorableQMdiSubWindow(calibrationDialog, calibrationDialogName, this);
    mdiArea->addSubWindow(child);
    child->setWindowTitle("Calibration view");
    child->show();
    child->restoreGeometry();
    connect(child, SIGNAL (onCloseSubWindow()), this, SLOT (updateWindowMenu()));
    connect(child, &RestorableQMdiSubWindow::onCloseSubWindow, this, [this]() { calibrationWindow->deleteLater(); calibrationWindow=nullptr; });
    calibrationWindow = child;
    calibrationWindow->setWindowIcon(calibrateIcon);
}

void MainWindow::loadSharpnessWindow(){
    if(selectedCamera && (
    selectedCamera->getType() == CameraImageType::LIVE_SINGLE_CAMERA  // ||
    // selectedCamera->getType() == CameraImageType::LIVE_SINGLE_WEBCAM
    )) {
        RestorableQMdiSubWindow *child = new RestorableQMdiSubWindow(new SingleCameraSharpnessView(dynamic_cast<SingleCamera*>(selectedCamera), this), "SingleCameraSharpnessView", this);
        mdiArea->addSubWindow(child);
        child->show();
        child->restoreGeometry();
        sharpnessWindow = child;
        sharpnessWindow->setWindowIcon(sharpnessIcon);
        connect(child, SIGNAL (onCloseSubWindow()), this, SLOT (updateWindowMenu()));
        connect(child, &RestorableQMdiSubWindow::onCloseSubWindow, this, [this]() { sharpnessWindow->deleteLater(); sharpnessWindow=nullptr; });
    }
//    else if(selectedCamera && selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE) {
//        RestorableQMdiSubWindow *child = new RestorableQMdiSubWindow(new SingleCameraSharpnessView(dynamic_cast<FileCamera*>(selectedCamera), this), "SingleCameraSharpnessView", this);
//        mdiArea->addSubWindow(child);
//        child->show();
//        child->restoreGeometry();
//        connect(child, SIGNAL (onCloseSubWindow()), this, SLOT (updateWindowMenu()));
//    }
}

void MainWindow::resetStatus(bool isConnect)
{
    bool realCameraSelected = (selectedCamera && (selectedCamera->getType() != CameraImageType::SINGLE_IMAGE_FILE && selectedCamera->getType() != CameraImageType::STEREO_IMAGE_FILE));

    if (isConnect){
        cameraAct->setEnabled(false);
        cameraSettingsAct->setEnabled(realCameraSelected);
        cameraActDisconnectAct->setEnabled(true);
        calibrateAct->setEnabled(true);
        sharpnessAct->setEnabled(selectedCamera && selectedCamera->getType() == CameraImageType::LIVE_SINGLE_CAMERA);
// //       subjectsAct->setEnabled(true);
//        subjectsAct->setEnabled(false);
        trackAct->setEnabled(true);
        logFileAct->setEnabled(true);

        // NOTE: streaming can only be enabled if the underlying connection is established, and tracking is on
        streamAct->setEnabled(trackingOn && streamingSettingsDialog && streamingSettingsDialog->isAnyConnected() && !exportingRecSection);

//        cameraSettingsAct->setEnabled(false);
        cameraViewAct->setEnabled(true); // In the View menu
        dataTableAct->setEnabled(true); // In the View menu

        imageRecordingOutputAct->setEnabled(realCameraSelected);
        recordImagesAct->setEnabled(realCameraSelected && !imageRecordingOutputTarget.isEmpty());
        forceResetTrialAct->setEnabled(realCameraSelected);
        manualIncTrialAct->setEnabled(realCameraSelected);
        forceResetMessageAct->setEnabled(realCameraSelected);

        fileOpenAct->setEnabled(false);
        exportRecSectionAct->setEnabled(selectedCamera && (selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE || selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE));

//        streamingSettingsAct->setEnabled(true);
        trialWidget->setVisible(realCameraSelected);
        trialWidgetLayoutSep->setVisible(realCameraSelected);
        messageWidget->setVisible(realCameraSelected);
        messageWidgetLayoutSep->setVisible(realCameraSelected);

        recordAct->setEnabled(!exportingRecSection);
    }
    else {
        cameraAct->setEnabled(true);
        cameraSettingsAct->setEnabled(false);
        cameraActDisconnectAct->setEnabled(false);
        calibrateAct->setEnabled(false);
        sharpnessAct->setEnabled(false);
// //        subjectsAct->setEnabled(false);
        // Currently it could mess up the application if QSettings is changed when there is any connection on, so make sure they are not
        // TODO: reform the subjects selection system, and merge it with meta snapshot functionality
//        subjectsAct->setEnabled(
//                remoteCCDialog && !remoteCCDialog->isAnyConnected() &&
//                streamingSettingsDialog && !streamingSettingsDialog->isAnyConnected() &&
//                MCUSettingsDialogInst && !MCUSettingsDialogInst->isConnected());
        trackAct->setEnabled(false);
        logFileAct->setEnabled(false);
        streamAct->setEnabled(false);

        cameraViewAct->setEnabled(false); // In the View menu
        dataTableAct->setEnabled(false); // In the View menu

        imageRecordingOutputAct->setEnabled(false);
        recordImagesAct->setEnabled(false);
        forceResetTrialAct->setEnabled(false);
        manualIncTrialAct->setEnabled(false);
        forceResetMessageAct->setEnabled(false);

        fileOpenAct->setEnabled(true);
        exportRecSectionAct->setEnabled(false);

//        streamingSettingsAct->setEnabled(false); / This should be enabled even if disconnected from camera
        trialWidget->setVisible(false);
        trialWidgetLayoutSep->setVisible(false);
        messageWidget->setVisible(false);
        messageWidgetLayoutSep->setVisible(false);

        // NOTE: az alábbi 2-nek nincs ellenpárja. Jó ez így?
        recordAct->setEnabled(false); //
        cameraPlaying = true; //
        }
}

void MainWindow::onImagesSkipped() {
    bool se = SupportFunctions::readBoolFromQSettings("ignoreFrameSkipWarnings", false, applicationSettings);
    if(imagesSkippedMsgBox != nullptr || se ) {
        return;
    }
    imagesSkippedMsgBox = new QMessageBox(this);
    imagesSkippedMsgBox->setWindowTitle("Camera image frames skipped");
    imagesSkippedMsgBox->setText("At least one image frame was skipped due to unstable connection or interface failure.\n\nPlease check camera connection. Be sure to use a power-supply backed (active) cable for long distances, and clean electrical contacts with appropriate materials if necessary.\n\nCameras can consume considerable power during frame grabbing, thus should you also ensure that your power supply has compatible amperage rating for your camera device.");
    imagesSkippedMsgBox->setMinimumSize(330,240);
    imagesSkippedMsgBox->setIcon(QMessageBox::Warning);
    imagesSkippedMsgBox->setModal(false);
    connect(imagesSkippedMsgBox, SIGNAL(accepted()), this, SLOT(onImagesSkippedMsgClose()));
    //connect(imagesSkippedMsgBox,SIGNAL(accepted()),this,SLOT(onImagesSkippedMsgClose()));
    //connect(imagesSkippedMsgBox,SIGNAL(rejected()),this,SLOT(onImagesSkippedMsgClose()));
    imagesSkippedMsgBox->show();
}

void MainWindow::onImagesSkippedMsgClose() {
    disconnect(imagesSkippedMsgBox, SIGNAL(accepted()), this, SLOT(onImagesSkippedMsgClose()));
    imagesSkippedMsgBox->deleteLater();
    imagesSkippedMsgBox = nullptr;
}

void MainWindow::onCameraUnexpectedlyDisconnected() {

    QMessageBox *msgBox = new QMessageBox(this);
    msgBox->setWindowTitle("Camera unexpectedly disconnected");
    msgBox->setText("At least one camera in use was unexpectedly disconnected. Recordings are stopped for saving.\n\nPlease check camera connection. Be sure to use a power-supply backed (active) cable for long distances, and clean electrical contacts with appropriate materials if necessary.\n\nCameras can consume considerable power during frame grabbing, thus should you also ensure that your power supply has compatible amperage rating for your camera device.");
    msgBox->setMinimumSize(330,240);
    msgBox->setIcon(QMessageBox::Warning);
    msgBox->setModal(false);
    msgBox->show();

    onCameraDisconnectClick();

}

void MainWindow::onDeviceWasReset() {
    if(deviceWasResetMsgBox != nullptr) {
        return;
    }
    deviceWasResetMsgBox = new QMessageBox(this);
    deviceWasResetMsgBox->setWindowTitle("Camera device was reset");
    deviceWasResetMsgBox->setText("The camera device encountered an error and had to be reset. If a recording was active, a portion of it was not recorded.\n\nPlease check camera connection and power supply if necessary. If possible, check the camera with the software suite provided by its manufacturer.");
    deviceWasResetMsgBox->setMinimumSize(330,240);
    deviceWasResetMsgBox->setIcon(QMessageBox::Warning);
    deviceWasResetMsgBox->setModal(false);
    connect(deviceWasResetMsgBox, SIGNAL(accepted()), this, SLOT(onDeviceWasResetMsgClose()));
    deviceWasResetMsgBox->show();
}

void MainWindow::onDeviceWasResetMsgClose() {
    disconnect(deviceWasResetMsgBox, SIGNAL(accepted()), this, SLOT(onDeviceWasResetMsgClose()));
    deviceWasResetMsgBox->deleteLater();
    deviceWasResetMsgBox = nullptr;
}

void MainWindow::onManualDeviceResetNecessary() {

    QMessageBox *msgBox = new QMessageBox(this);
    msgBox->setWindowTitle("Manual device reset is necessary");
    msgBox->setText("The camera encountered an unrecoverable error, and it was disconnected. Recordings are stopped for saving.\n\nPlease manually power cycle (cold reset) the camera device.");
    msgBox->setMinimumSize(330,240);
    msgBox->setIcon(QMessageBox::Warning);
    msgBox->setModal(false);
    msgBox->show();

    onCameraDisconnectClick();

}

void MainWindow::connectCameraPlaybackChangedSlotsForCameraViews()
{
    /*    if (imagePlaybackControlDialog && cameraViewWindow){
        connect(imagePlaybackControlDialog, &ImagePlaybackControlDialog::cameraPlaybackChanged, cameraViewWindow, &SingleCameraView::onCameraPlaybackChanged);
        connect(cameraViewWindow, &SingleCameraView::cameraPlaybackChanged, imagePlaybackControlDialog, &ImagePlaybackControlDialog::onCameraPlaybackChanged);        
    }
    else if (imagePlaybackControlDialog){
        connect(this, cameraPlaybackChanged, imagePlaybackControlDialog, &ImagePlaybackControlDialog::onCameraPlaybackChanged);
        connect(imagePlaybackControlDialog, &ImagePlaybackControlDialog::cameraPlaybackChanged, this, onCameraPlaybackChanged);
        connect(imagePlaybackControlDialog, &ImagePlaybackControlDialog::cameraPlaybackChanged, imagePlaybackControlDialog, &ImagePlaybackControlDialog::onCameraPlaybackChanged);
    }
    else if (cameraViewWindow){
        connect(this, cameraPlaybackChanged, cameraViewWindow, &SingleCameraView::onCameraPlaybackChanged);
        connect(cameraViewWindow, &SingleCameraView::cameraPlaybackChanged, this, onCameraPlaybackChanged);
        connect(cameraViewWindow, &SingleCameraView::cameraPlaybackChanged, cameraViewWindow, &SingleCameraView::onCameraPlaybackChanged);
    }
    connect(this, SIGNAL(cameraPlaybackChanged()), this, SLOT(onCameraPlaybackChanged()));*/

    if (imagePlaybackControlDialog != nullptr && singleCameraChildWidget != nullptr){
        connect(imagePlaybackControlDialog, SIGNAL(cameraPlaybackChanged()), singleCameraChildWidget, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
        connect(singleCameraChildWidget, SIGNAL(cameraPlaybackChanged()), imagePlaybackControlDialog, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);        
    }
    if (imagePlaybackControlDialog != nullptr && stereoCameraChildWidget != nullptr){
        connect(imagePlaybackControlDialog, SIGNAL(cameraPlaybackChanged()), stereoCameraChildWidget, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
        connect(stereoCameraChildWidget, SIGNAL(cameraPlaybackChanged()), imagePlaybackControlDialog, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);        
    }

    if (imagePlaybackControlDialog != nullptr){
        connect(this, SIGNAL(cameraPlaybackChanged()), imagePlaybackControlDialog, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
        connect(imagePlaybackControlDialog, SIGNAL(cameraPlaybackChanged()), this, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
        connect(imagePlaybackControlDialog, SIGNAL(cameraPlaybackChanged()), imagePlaybackControlDialog, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
    }

    if (singleCameraChildWidget != nullptr){
        connect(this, SIGNAL(cameraPlaybackChanged()), singleCameraChildWidget, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
        connect(singleCameraChildWidget, SIGNAL(cameraPlaybackChanged()), this, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
        connect(singleCameraChildWidget, SIGNAL(cameraPlaybackChanged()), singleCameraChildWidget, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
    }

    if (stereoCameraChildWidget != nullptr){
        connect(this, SIGNAL(cameraPlaybackChanged()), stereoCameraChildWidget, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
        connect(stereoCameraChildWidget, SIGNAL(cameraPlaybackChanged()), this, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
        connect(stereoCameraChildWidget, SIGNAL(cameraPlaybackChanged()), stereoCameraChildWidget, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
    }
    connect(this, SIGNAL(cameraPlaybackChanged()), this, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);

    // I could have done this in a way that the camera child widgets only receive a cv::Mat to display..
    // but we are actually not displaying anything else in the views, just fileCamera frames,
    // so I dedicated separate functions for them, which only take the frameNumber,
    // implemented for both single and stereo camera views. This is ok too
    if(selectedCamera->getType() == CameraImageType::SINGLE_IMAGE_FILE && singleCameraChildWidget && imagePlaybackControlDialog) {
        connect(imagePlaybackControlDialog, SIGNAL(stillImageChange(int)), singleCameraChildWidget, SLOT(displayFileCameraFrame(int)));
    } else if(selectedCamera->getType() == CameraImageType::STEREO_IMAGE_FILE && stereoCameraChildWidget && imagePlaybackControlDialog) {
        connect(imagePlaybackControlDialog, SIGNAL(stillImageChange(int)), stereoCameraChildWidget, SLOT(displayFileCameraFrame(int)));
    }
}

void MainWindow::connectCameraPlaybackChangedSlotsForCameraSettings() {
    if (singleCameraSettingsDialog != nullptr){
        connect(singleCameraSettingsDialog, SIGNAL(cameraPlaybackChanged()), this, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
        // This is needed, for the case if someone presses F in a camera settings window, so that the tickmark gets updated in camera view window
        connect(singleCameraSettingsDialog, SIGNAL(cameraPlaybackChanged()), singleCameraChildWidget, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
    }
    if (stereoCameraSettingsDialog != nullptr){
        connect(stereoCameraSettingsDialog, SIGNAL(cameraPlaybackChanged()), this, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
        // This is needed, for the case if someone presses F in a camera settings window, so that the tickmark gets updated in camera view window
        connect(stereoCameraSettingsDialog, SIGNAL(cameraPlaybackChanged()), stereoCameraChildWidget, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
    }
    if (singleWebcamSettingsDialog != nullptr){
        connect(singleWebcamSettingsDialog, SIGNAL(cameraPlaybackChanged()), this, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
        // This is needed, for the case if someone presses F in a camera settings window, so that the tickmark gets updated in camera view window
        connect(singleWebcamSettingsDialog, SIGNAL(cameraPlaybackChanged()), singleCameraChildWidget, SLOT(onCameraPlaybackChanged()), Qt::UniqueConnection);
    }
    // Note that we make no connect INTO any camera settings dialog.
    //  The reason is that, it does not have any GUI element that would need to know if the image is freezed or not.
}


void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}
/*
void MainWindow::dragMoveEvent(QDragMoveEvent *e)
{
    // DEV: not sure if the check is needed here too
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}
*/
void MainWindow::dropEvent(QDropEvent* e)
{
    QStringList pathList;
    QList<QUrl> urlList = e->mimeData()->urls();

    QString thingToOpen;
    if(!urlList.empty())
        thingToOpen = urlList.at(0).toLocalFile();

    QFileInfo fileInfo(thingToOpen);
    if(!fileInfo.isReadable()) {
        QMessageBox MsgBox;
        MsgBox.setText(QString::fromStdString("The folder you are trying to open is not readable. Please ensure sufficient permission of your user account and/or PupilEXT, and the availability of the location to be read."));
        MsgBox.exec();
    }

    if(fileInfo.isDir()) {
        if(selectedCamera && selectedCamera->isOpen()) {
            onCameraDisconnectClick();
        }
        qDebug() << "Attempting to open: " << fileInfo.filePath();
        openImageFileSource(fileInfo.filePath());
    } else if(fileInfo.isFile()) {
        // TODO: shorter, cleaner, better
        if(fileInfo.completeSuffix() == "tiff" || fileInfo.completeSuffix() == "tif" || fileInfo.completeSuffix() == "png"  ||
            fileInfo.completeSuffix() == "bmp" || fileInfo.completeSuffix() == "jpeg" || fileInfo.completeSuffix() == "jpg" ||
            fileInfo.completeSuffix() == "jpe" ||  fileInfo.completeSuffix() == "jp2" ||  fileInfo.completeSuffix() == "webp" ||
            fileInfo.completeSuffix() == "pgm" ||
            fileInfo.fileName() == "imagerec_meta.xml" || fileInfo.fileName() == "offline_event_log.xml" ||
            fileInfo.fileName() == "imagerec-meta.xml" || fileInfo.fileName() == "offline-event-log.xml" ) {

            if(selectedCamera && selectedCamera->isOpen()) {
                onCameraDisconnectClick();
            }
            qDebug() << "Attempting to open: " << fileInfo.filePath().chopped(fileInfo.fileName().length());
            openImageFileSource(fileInfo.filePath().chopped(fileInfo.fileName().length()));
        }
    }

    // TODO: add further checks and event handling
    e->acceptProposedAction();
}


void MainWindow::onStereoCamerasOpened() {
    resetStatus(true);
    pupilDetectionSettingsDialog->onSettingsChange();
}

void MainWindow::onStereoCamerasClosed() {
    // first disable everything, as if we really closed the camera
    resetStatus(false);

    // Then...
    // These are exclusive for stereo camera right now
    cameraAct->setEnabled(false);
    cameraSettingsAct->setEnabled(true);
    cameraActDisconnectAct->setEnabled(true);
    cameraViewAct->setEnabled(true);
    // TODO: a nicer way to make these GUI changes?

    pupilDetectionSettingsDialog->onSettingsChange();
}

void MainWindow::createCamTempMonitor() {

    if(!selectedCamera->isTemperatureReadingSupported()) {
        onDeviceWarmUpReadingsUnavailable();
        return;
    }

    QThread *tempMonitorThread = new QThread();
    camTempMonitor = new CamTempMonitor(selectedCamera);
    connect(tempMonitorThread, &QThread::started, camTempMonitor, &CamTempMonitor::run);
    camTempMonitor->moveToThread(tempMonitorThread);
    //connect(tempMonitorThread, SIGNAL (finished()), tempMonitorThread, SLOT (deleteLater()));
    tempMonitorThread->start();
    tempMonitorThread->setPriority(QThread::LowPriority);
    connect(camTempMonitor, SIGNAL(camTempChecked(std::vector<double>)), recEventTracker, SLOT(addTemperatureCheck(std::vector<double>)));

    // NOTE: This is just the first demo version. The final version should support checking for illuminator temperature via the MCU
    connect(camTempMonitor, SIGNAL(cameraWarmedUp()), this, SLOT(onDeviceWarmedUp()));
    connect(camTempMonitor, SIGNAL(cameraWarmUpReadingsInvalid()), this, SLOT(onDeviceWarmUpReadingsInvalid()));
    connect(camTempMonitor, SIGNAL(cameraWarmupHasDeltaTimeData()), this, SLOT(onDeviceWarmupHasDeltaTimeData()));
}

void MainWindow::destroyCamTempMonitor() {
    if(!camTempMonitor) {
        onDeviceWarmedUpReset();
        return;
    }

    if(recEventTracker)
        disconnect(camTempMonitor, SIGNAL(camTempChecked(std::vector<double>)), recEventTracker, SLOT(addTemperatureCheck(std::vector<double>)));

    camTempMonitor->setRunning(false);
    //camTempMonitor->thread()->deleteLater();
    camTempMonitor->deleteLater();
    camTempMonitor = nullptr;

    onDeviceWarmedUpReset();
}

/*
void MainWindow::setRecentPath(QString path) {
    qDebug() << "Set recent path: " << path;
    recentPath = path;
    applicationSettings->setValue("RecentOutputPath", recentPath);
}
 */

void MainWindow::setRecentImageReadingDirectory(QString path) {
    qDebug() << "Set recentImageReadingDirectory: " << path;
    recentImageReadingDirectory = path;
    applicationSettings->setValue("RecentImageReadingDirectory", recentImageReadingDirectory);
}
void MainWindow::setRecentImageWritingDirectory(QString path) {
    qDebug() << "Set recentImageWritingDirectory: " << path;
    recentImageWritingDirectory = path;
    applicationSettings->setValue("RecentImageWritingDirectory", recentImageWritingDirectory);
}
void MainWindow::setRecentDataWritingDirectory(QString path) {
    qDebug() << "Set recentDataWritingDirectory: " << path;
    recentDataWritingDirectory = path;
    applicationSettings->setValue("RecentDataWritingDirectory", recentDataWritingDirectory);
}

void MainWindow::offerResetApplicationSettings() {
    QMessageBox *resetAppSettingsMsgBox = new QMessageBox(
            QMessageBox::Question,
            tr("Reset application settings"),
            tr("Are you sure you want to reset application settings?\nThis will reset all settings, and restart the application."),
            QMessageBox::Yes | QMessageBox::No,
            this);
    resetAppSettingsMsgBox->setMinimumSize(330,240);
    resetAppSettingsMsgBox->setIcon(QMessageBox::Warning);
    resetAppSettingsMsgBox->setButtonText(QMessageBox::Yes, tr("Yes"));
    resetAppSettingsMsgBox->setButtonText(QMessageBox::No, tr("No"));
//    resetAppSettingsMsgBox->setModal(false);
//    resetAppSettingsMsgBox->show();
    resetAppSettingsMsgBox->exec();
    if(resetAppSettingsMsgBox->result() == QMessageBox::Yes) {
        onCameraDisconnectClick();
        applicationSettings->clear();
        qApp->exit(EXIT_CODE_REBOOT);
    }
}

void MainWindow::offerRestartApplication() {
    QMessageBox *resetAppSettingsMsgBox = new QMessageBox(
            QMessageBox::Question,
            tr("Restart application"),
            tr("Would you like to restart the application now?"),
            QMessageBox::Yes | QMessageBox::No,
            this);
    resetAppSettingsMsgBox->setMinimumSize(330,240);
//    resetAppSettingsMsgBox->setIcon(QMessageBox::Warning);
    resetAppSettingsMsgBox->setButtonText(QMessageBox::Yes, tr("Yes"));
    resetAppSettingsMsgBox->setButtonText(QMessageBox::No, tr("No"));
//    resetAppSettingsMsgBox->setModal(false);
//    resetAppSettingsMsgBox->show();
    resetAppSettingsMsgBox->exec();
    if(resetAppSettingsMsgBox->result() == QMessageBox::Yes) {
        onCameraDisconnectClick();
        qApp->exit(EXIT_CODE_REBOOT);
    }
}

