
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/qformlayout.h>
#include <QtWidgets/QLabel>
#include <QtWidgets/QtWidgets>
#include "setupGeometryDialog.h"
#include "../SVGIconColorAdjuster.h"
#include "../remoteSetupModel.h"

// Create the pupil detection settings dialog
// Given a pupil detection object to communicate to the detection algorithm objects there
SetupGeometryDialog::SetupGeometryDialog(QWidget *parent) :
        QDialog(parent),
        applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), parent)) {

    //this->setMinimumSize(800, 500);
    this->setMinimumSize(1100, 600);
    this->resize(1300, 800);
    this->setWindowTitle("Setup Geometry");

    //helplensIcon1 = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/lens-help-1.svg"), applicationSettings);
    helpLensIcon1 = QIcon(":/icons/help-lens-2.png");
    helpHeadIcon1 = QIcon(":/icons/help-head-1.png");

    createForm();

    connect(applyButton, SIGNAL(clicked()), this, SLOT(applyButtonClick()));
    connect(applyCloseButton, SIGNAL(clicked()), this, SLOT(applyCloseButtonClick()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(cancelButtonClick()));

    //connect(algorithmBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onAlgorithmSelection(int)));

    //onProcModeSelection(procModeBox->currentIndex());
    //onAlgorithmSelection(algorithmBox->currentIndex());
    loadSettings();

    //updateProcModeEnabled();
}

SetupGeometryDialog::~SetupGeometryDialog() {

}

void SetupGeometryDialog::createForm() {

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setMargin(10);
    mainLayout->setContentsMargins(10,10,10,10);
    QHBoxLayout *mainLayoutInner = new QHBoxLayout();
    mainLayoutInner->setMargin(0);
    mainLayoutInner->setContentsMargins(0,0,0,0);
    QVBoxLayout *mainLayoutInnerCol1 = new QVBoxLayout();
    mainLayoutInnerCol1->setMargin(0);
    mainLayoutInnerCol1->setContentsMargins(0,5,0,5);
    QVBoxLayout *mainLayoutInnerCol2 = new QVBoxLayout();
    mainLayoutInnerCol2->setMargin(0);
    mainLayoutInnerCol2->setContentsMargins(0,5,0,5);

    aGroup = new QGroupBox("Test group");

    //QFormLayout *procModeBoxLayout = new QFormLayout();
    //QVBoxLayout *algorithmLayout = new QVBoxLayout();
    //QHBoxLayout *algoBoxLayout = new QHBoxLayout();


    //...
    //mainLayoutInnerCol1->addWidget(procModeGroup);

    //algorithmGroup->setLayout(algorithmLayout);
    //mainLayoutInnerCol1->addWidget(algorithmGroup);

    QFormLayout *aLayout = new QFormLayout();

    QLabel *comboLabel = new QLabel(tr("Test label:"));
    comboBox = new QComboBox();
    comboBox->addItem(QString::fromStdString("1"));
    comboBox->addItem(QString::fromStdString("2"));
    comboBox->addItem(QString::fromStdString("3"));
    comboBox->addItem(QString::fromStdString("4"));
    comboBox->addItem(QString::fromStdString("5"));
    aLayout->addWidget(comboBox);

    QLabel *a1Label = new QLabel(tr("a1"));
    a1Box = new QCheckBox();
    //a1Box->setChecked(pupilDetection->isROIPreProcessingEnabled());
    aLayout->addRow(a1Label, a1Box);

    QLabel *a2Label = new QLabel(tr("a2"));
    a2Box = new QCheckBox();
    //a2Box->setChecked(pupilDetection->isOutlineConfidenceEnabled());
    aLayout->addRow(a2Label, a2Box);

    QLabel *a3Label = new QLabel(tr("a3"));
    a3Box = new QCheckBox();
    //a2Box->setChecked(pupilDetection->isOutlineConfidenceEnabled());
    aLayout->addRow(a3Label, a2Box);

    //connect(pupilUndistortionBox, SIGNAL(stateChanged(int)), this, SLOT(onPupilUndistortionClick(int)));

    qtOpenGlViewer = new QtOpenGLViewer();
    qtOpenGlViewer->setFixedSize(700,500);
    qtOpenGlViewer->setBackgroundColor(QColor::fromRgb(20,31,33));
    qtOpenGlViewer->setFocusPolicy(Qt::FocusPolicy::ClickFocus); // let it catch keypresses
    mainLayoutInnerCol1->addWidget(qtOpenGlViewer);


    helpBoxLayout = new QVBoxLayout(this);
    helpBoxLayout->setContentsMargins(0,0,0,0);

    // NOTE: This hardcoded 20px is the width of the vertical scrollbar, as we need to count it in as well
    int safeWidth = qtOpenGlViewer->size().width();
    QSize safeSize = QSize(safeWidth, safeWidth/13*12);

    helpContentImage->setFlat(true);
    helpContentImage->setAttribute(Qt::WA_NoSystemBackground, true);
    helpContentImage->setAttribute(Qt::WA_TranslucentBackground, true);
    helpContentImage->setStyleSheet("QPushButton { background-color: transparent; border: 0px }");
    //helpContentImage->setIcon(SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/status/22/dialog-information.svg"), applicationSettings));
    //helpContentImage->setFixedSize(QSize(32,32));
    helpContentImage->setFixedSize(safeSize);
    //helpContentImage->setIconSize(QSize(32,32));
    helpContentImage->setIconSize(safeSize);
    helpBoxLayout->addWidget(helpContentImage);

    helpContentText = new QLabel();
    helpContentText->setContentsMargins(20,0,0,20);
    helpBoxLayout->addWidget(helpContentText);

    helpBox = new QWidget();

    helpBoxSca = new QScrollArea();
    helpBoxSca->setWidget(helpBox);
    helpBoxSca->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    helpBoxSca->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    //helpBox->setContentsMargins(20,0,0,0);
    helpBox->setStyleSheet("QWidget { color: white; background-color: rgb(20,31,33); border: 0px }");
    helpBox->setLayout(helpBoxLayout);
    helpBox->setFixedWidth(safeWidth);
    helpBox->setMinimumHeight(safeSize.height());
    helpBox->setMaximumHeight(1000);
    //helpBoxSca->setFixedWidth(qtOpenGlViewer->size().width());
    helpBoxSca->setFixedSize(qtOpenGlViewer->size());
    mainLayoutInnerCol1->addWidget(helpBoxSca);
    helpBoxSca->hide();


    QPushButton *testHelpRotateButton = new QPushButton("Test help");
    connect(testHelpRotateButton, SIGNAL(clicked()), this, SLOT(testHelpRotateButtonClicked()));
    mainLayoutInnerCol1->addWidget(testHelpRotateButton);


    aGroup->setLayout(aLayout);
    mainLayoutInnerCol1->addWidget(aGroup);



    QFormLayout *componentsLayout = new QFormLayout();

    QLabel *setupModelTreeLabel = new QLabel(tr("Setup model components tree"));
    QTreeView *setupModelTree = new QTreeView();


    RemoteSetupModel *setupModel = new RemoteSetupModel();

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////

    setupModel->resetModel();

    // these could be in a loop that reads model components from a json file
    Unit *cameraUnit = new Unit(this);
    CameraComponent *cameraComponent = new CameraComponent(this);
    cameraComponent->componentVendor = "Basler";
    cameraComponent->componentType = "acA1300-200um";
    //
    SensorComponent *sensorComponent = new SensorComponent(this);
    sensorComponent->componentVendor = "Sony";
    sensorComponent->componentType = "IMX 392";
    sensorComponent->resolutionX = 1920; // px
    sensorComponent->resolutionY = 1200; // px
    sensorComponent->pixelSize = 3.4f; // um
    cameraComponent->components.push_back(sensorComponent);
    //
    cameraUnit->components.push_back(cameraComponent);

    LensComponent *lensComponent = new LensComponent(this);
    lensComponent->componentVendor = "Ricoh";
    lensComponent->componentType;
    lensComponent->focalDistanceMin = 25.0; // mm
    lensComponent->focalDistanceMax = 25.0; // mm
    lensComponent->focalDistanceActual = 25.0; // mm
    lensComponent->fValueMin = 16.0;
    lensComponent->fValueMax = 1.4;
    lensComponent->fValueActual = 1.4;
    lensComponent->sholuderToFirstSurfaceDistance = 42; // mm
    lensComponent->outerDiameter = 36; // mm // when there are no adjustment screws attached
    lensComponent->frontFilterDiameter = 35; // mm
    cameraUnit->components.push_back(lensComponent);

    FilterComponent *filterComponent = new FilterComponent(this);
    filterComponent->componentVendor = "MidOpt";
    filterComponent->componentType = "LP720";
    filterComponent->mountingType = "screw-on"; // screw-on, screw-in, embedded
    filterComponent->opticalBehaviour = "lowpass"; // lowpass, highpass, bandpass
    filterComponent->lowpassCuton = 720; // nm
    filterComponent->filterDiameter = 35; // mm
    cameraUnit->components.push_back(filterComponent);

    setupModel->cameraUnits.push_back(cameraUnit);

    Unit *illuminatorUnit = new Unit(this);
    IlluminatorComponent *illuminatorComponent = new IlluminatorComponent(this);
    illuminatorComponent->atomicComponentVendor = "ams Osram";
    illuminatorComponent->atomicComponentType = "SFH 4717";
    illuminatorComponent->numAtomicComponents = 4;
    illuminatorComponent->radiationAngle = 50; // of the cone in which most light is emitted
    illuminatorComponent->emissionCentroid = 850; // centroid wavelength, nm
    illuminatorComponent->driverIsConstantCurrent = true; // constantCurrent, constantVoltage
    illuminatorComponent->driverIsVariable = false;
    illuminatorUnit->components.push_back(illuminatorComponent);
    setupModel->illuminatorUnits.push_back(illuminatorUnit);

    Unit *screenUnit = new Unit(this);
    ScreenComponent *screenComponent = new ScreenComponent(this);
    screenComponent->physicalSizeX = 400; // mm
    screenComponent->physicalSizeY = 320; // mm
    screenComponent->resolutionX = 1280;
    screenComponent->resolutionY = 1024;
    screenUnit->components.push_back(screenComponent);
    setupModel->screenUnits.push_back(screenUnit);

    Unit *head = new Unit(this);
    CvTargetComponent *foreheadTarget = new CvTargetComponent(this);
    EyeballComponent *leftEyeball = new EyeballComponent(this);
    EyeballComponent *rightEyeball = new EyeballComponent(this);
    leftEyeball->eyeballDiameter = 24.0f;
    rightEyeball->eyeballDiameter = 24.0f;
    foreheadTarget->outerRingDiameter = 10.0f;
    head->components.push_back(foreheadTarget);
    head->components.push_back(leftEyeball);
    head->components.push_back(rightEyeball);
    setupModel->heads.push_back(head);

    // TODO: some of these could be done under the hood in setupModel by setters (?)
    cameraComponent->dimX = 29.0; // These are just the sizes for a random Basler camera model
    cameraComponent->dimY = 29.0;
    cameraComponent->dimZ = 48.0;
    cameraComponent->locX = 15.0;
    cameraComponent->locY = 5.0;
    cameraComponent->locZ = 10.0;
    cameraComponent->rotX = -15;
    cameraComponent->rotY = 0;
    cameraComponent->rotZ = 0;
    //
    sensorComponent->dimX = sensorComponent->effectiveSizeX();
    sensorComponent->dimY = sensorComponent->effectiveSizeY();
    sensorComponent->dimZ = 1.0;
    sensorComponent->locX = cameraComponent->locX;
    sensorComponent->locY = cameraComponent->locY;
    sensorComponent->locZ = cameraComponent->locZ + cameraComponent->dimZ/2.0f - cameraComponent->flangeDistance;
    sensorComponent->rotX = cameraComponent->rotX;
    sensorComponent->rotY = cameraComponent->rotY;
    sensorComponent->rotZ = cameraComponent->rotZ;
    //
    lensComponent->dimX = lensComponent->outerDiameter;
    lensComponent->dimY = lensComponent->outerDiameter;
    lensComponent->dimZ = lensComponent->sholuderToFirstSurfaceDistance;
    lensComponent->locX = cameraComponent->locX;
    lensComponent->locY = cameraComponent->locY;
    lensComponent->locZ = cameraComponent->locZ + cameraComponent->dimZ/2.0f + lensComponent->sholuderToFirstSurfaceDistance /2.0f;
    lensComponent->rotX = cameraComponent->rotX;
    lensComponent->rotY = cameraComponent->rotY;
    lensComponent->rotZ = cameraComponent->rotZ;
    //
    filterComponent->dimX = lensComponent->outerDiameter;
    filterComponent->dimY = lensComponent->outerDiameter;
    filterComponent->dimZ = 2.0; // assuming that 1 mm is okay for the half of a display thickness
    filterComponent->locX = cameraComponent->locX;
    filterComponent->locY = cameraComponent->locY;
    filterComponent->locZ = cameraComponent->locZ + cameraComponent->dimZ/2.0f + lensComponent->sholuderToFirstSurfaceDistance + filterComponent->dimZ/2.0f;
    filterComponent->rotX = cameraComponent->rotX;
    filterComponent->rotY = cameraComponent->rotY;
    filterComponent->rotZ = cameraComponent->rotZ;
    //
    illuminatorComponent->dimX = 40.0f;
    illuminatorComponent->dimY = 40.0f;
    illuminatorComponent->dimZ = 2.0;
    illuminatorComponent->locX = cameraComponent->locX + 100.0f;
    illuminatorComponent->locY = cameraComponent->locY;
    illuminatorComponent->locZ = cameraComponent->locZ;
    illuminatorComponent->rotX = cameraComponent->rotX;
    illuminatorComponent->rotY = cameraComponent->rotY - 10.0;
    illuminatorComponent->rotZ = cameraComponent->rotZ;
    //
    screenComponent->dimX = screenComponent->physicalSizeX;
    screenComponent->dimY = screenComponent->physicalSizeY;
    screenComponent->dimZ = 5.0f;
    screenComponent->locX = cameraComponent->locX;
    screenComponent->locY = cameraComponent->locY + cameraComponent->dimY + 20.0f + screenComponent->physicalSizeY/2.0f;
    screenComponent->locZ = cameraComponent->locZ;
    screenComponent->rotX = 0.0f;
    screenComponent->rotY = 0.0f;
    screenComponent->rotZ = 0.0f;
    //
    leftEyeball->dimX = leftEyeball->eyeballDiameter/2.0f;
    leftEyeball->dimY = leftEyeball->eyeballDiameter/2.0f;
    leftEyeball->dimZ = leftEyeball->eyeballDiameter/2.0f;
    leftEyeball->locX = 0 - 60.0f/2.0f; // 60 mm is an average human intercanthal distance
    leftEyeball->locY = screenComponent->locY + 10.0f;
    leftEyeball->locZ = 580.0f;
    //leftEyeball->rotX = 90.0f; // to be looking at the screen now // TODO: valamiért úgy tűnik a forgatást a translate után teszi rá, ezért elkerül tök messze a szem
    leftEyeball->rotY = 0.0f;
    leftEyeball->rotZ = 0.0f;
    //
    rightEyeball->dimX = rightEyeball->eyeballDiameter/2.0f;
    rightEyeball->dimY = rightEyeball->eyeballDiameter/2.0f;
    rightEyeball->dimZ = rightEyeball->eyeballDiameter/2.0f;
    rightEyeball->locX = 0 + 60.0f/2.0f; // 60 mm is an average human intercanthal distance
    rightEyeball->locY = screenComponent->locY + 10.0f;
    rightEyeball->locZ = 580.0f;
    //rightEyeball->rotX = 90.0f; // to be looking at the screen now // TODO: valamiért úgy tűnik a forgatást a translate után teszi rá, ezért elkerül tök messze a szem
    rightEyeball->rotY = 0.0f;
    rightEyeball->rotZ = 0.0f;
    //
    foreheadTarget->dimX = foreheadTarget->outerRingDiameter;
    foreheadTarget->dimY = foreheadTarget->outerRingDiameter;
    foreheadTarget->dimZ = 1.0;
    foreheadTarget->locX = 0; // 60 mm is an average human intercanthal distance
    foreheadTarget->locY = screenComponent->locY + 10.0f + 30.0f;
    foreheadTarget->locZ = 575.0f;
    foreheadTarget->rotX = 0.0f;
    foreheadTarget->rotY = 0.0f;
    foreheadTarget->rotZ = 0.0f;

    // TODO: rot and loc is the right order of mentioning everywhere, as the rotation is first only made with respect to
    //      the component's own axes, and then translated is the component somewhere else.
    //      Especially, head should have easy setters that will make eyeballs move and rotate when the head moves.

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //SetupModelTreeModel setupModelTreeModel(QString::fromUtf8(file.readAll()));
    //file.close();
    SetupModelTreeModel setupModelTreeModel(setupModel);

    setupModelTree->setModel(&setupModelTreeModel);
    setupModelTree->setWindowTitle(SetupModelTreeModel::tr("Simple Tree Model"));
    for (int c = 0; c < setupModelTreeModel.columnCount(); ++c)
        setupModelTree->resizeColumnToContents(c);
    setupModelTree->expandAll();
    const auto screenSize = setupModelTree->screen()->availableSize();
    setupModelTree->resize({screenSize.width() / 2, screenSize.height() * 2 / 3});
    setupModelTree->show();
    //
    qtOpenGlViewer->setSetupModel(setupModel);
    qtOpenGlViewer->update();

    // WE NEED OUR TREE MODEL HERE that accesses the mSetup object
    // ...

    //a2Box->setChecked(pupilDetection->isOutlineConfidenceEnabled());
    componentsLayout->addRow(setupModelTreeLabel);
    componentsLayout->addRow(setupModelTree);

    //connect(pupilUndistortionBox, SIGNAL(stateChanged(int)), this, SLOT(onPupilUndistortionClick(int)));

    mainLayoutInnerCol2->addLayout(componentsLayout);



    QHBoxLayout *buttonsLayout = new QHBoxLayout();

    applyButton = new QPushButton(tr("Apply"));
    applyCloseButton = new QPushButton(tr("Apply and Close"));
    cancelButton = new QPushButton(tr("Cancel"));

    buttonsLayout->addWidget(applyButton);
    buttonsLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding));
    buttonsLayout->addWidget(applyCloseButton);
    buttonsLayout->addWidget(cancelButton);

    mainLayoutInner->addLayout(mainLayoutInnerCol1);
    mainLayoutInner->addLayout(mainLayoutInnerCol2);
    mainLayout->addLayout(mainLayoutInner);
    mainLayout->addLayout(buttonsLayout);

    setLayout(mainLayout);
}

// Update the dialog form from the settings configured in the pupil detection process
void SetupGeometryDialog::updateForm() {
    /*
    algorithmBox->setCurrentText(QString::fromStdString(pupilDetection->getCurrentMethod1()->title()));
    roiPreprocessingBox->setChecked(pupilDetection->isROIPreProcessingEnabled());
    outlineConfidenceBox->setChecked(pupilDetection->isOutlineConfidenceEnabled());

    pupilUndistortionBox->setChecked(pupilDetection->isPupilUndistortionEnabled());
    imageUndistortionBox->setChecked(pupilDetection->isImageUndistortionEnabled());
    */
}

void SetupGeometryDialog::reject() {
    QDialog::reject();
}

void SetupGeometryDialog::loadSettings() {

    /*
    pupilDetection->setAlgorithm(applicationSettings->value("SetupGeometryDialog.algorithm", algorithmBox->currentText()).toString());
//    pupilDetection->enableOutlineConfidence(SupportFunctions::readBoolFromQSettings("SetupGeometryDialog.outlineConfidence", outlineConfidenceBox->isChecked(), applicationSettings));
//    pupilDetection->enableROIPreProcessing(SupportFunctions::readBoolFromQSettings("SetupGeometryDialog.processROI", roiPreprocessingBox->isChecked(), applicationSettings));
    pupilDetection->enableOutlineConfidence(SupportFunctions::readBoolFromQSettings("SetupGeometryDialog.outlineConfidence", true, applicationSettings));
    pupilDetection->enableROIPreProcessing(SupportFunctions::readBoolFromQSettings("SetupGeometryDialog.processROI", true, applicationSettings));
    pupilDetection->enablePupilUndistortion(SupportFunctions::readBoolFromQSettings("SetupGeometryDialog.undistortPupilSize", pupilUndistortionBox->isChecked(), applicationSettings));
    pupilDetection->enableImageUndistortion(SupportFunctions::readBoolFromQSettings("SetupGeometryDialog.undistortImage", imageUndistortionBox->isChecked(), applicationSettings));
    */

    updateForm();
}

/*
// Save the detection settings to the application settings which are persisted on disk
void SetupGeometryDialog::saveUniversalSettings() {
    if(!pupilDetection->isTrackingOn() && procModeBox->currentIndex()!=0) {
        if(!pupilDetection->isStereo()) {
            applicationSettings->setValue("SetupGeometryDialog.singleCam.procMode", procModeBox->currentIndex());
        } else {
            applicationSettings->setValue("SetupGeometryDialog.stereoCam.procMode", procModeBox->currentIndex());
        }
    }

    applicationSettings->setValue("SetupGeometryDialog.algorithm", algorithmBox->currentText());
    applicationSettings->setValue("SetupGeometryDialog.outlineConfidence", outlineConfidenceBox->isChecked());
    applicationSettings->setValue("SetupGeometryDialog.processROI", roiPreprocessingBox->isChecked());
    applicationSettings->setValue("SetupGeometryDialog.undistortPupilSize", pupilUndistortionBox->isChecked());
    applicationSettings->setValue("SetupGeometryDialog.undistortImage", imageUndistortionBox->isChecked());
}
*/

/*
// Show and hide the algorithm specific settings depending on the current algorithm selection
void SetupGeometryDialog::onAlgorithmSelection(int idx) {

    int i = 0;
    for(auto pms: pupilMethodSettings) {
        if(i==idx) {
            pms->show();
            pms->infoBox->show();
        } else {
            pms->hide();
            pms->infoBox->hide();
        }
        i++;
    }
//    this->update();
}
*/

// Apply the settings to the pupil detection process and save them to the application settings
void SetupGeometryDialog::applyButtonClick() {



    //saveUniversalSettings();
}

void SetupGeometryDialog::cancelButtonClick() {
    close();
}

void SetupGeometryDialog::applyCloseButtonClick() {

    applyButtonClick();
    close();
}

void SetupGeometryDialog::onSettingsChange() {
    loadSettings();

    //updateProcModeEnabled();
    //updateProcModeCompatibility();
}

void SetupGeometryDialog::updateContents() {

}

void SetupGeometryDialog::testHelpRotateButtonClicked() {
    testHelp++;
    if(testHelp>4) {
        testHelp = 0;
    }

    if(testHelp == 0) {
        showSetupHelp(SetupHelp::SETUP_3D);
    } else if(testHelp == 1) {
        showSetupHelp(SetupHelp::COMPONENT_HELP_LENS);
    } else if(testHelp == 2) {
        showSetupHelp(SetupHelp::COMPONENT_HELP_CAMERA);
    } else if(testHelp == 3) {
        showSetupHelp(SetupHelp::COMPONENT_HELP_ILLUMINATOR);
    } else if(testHelp == 4) {
        showSetupHelp(SetupHelp::COMPONENT_HELP_HEAD);
    }
}

void SetupGeometryDialog::showSetupHelp(SetupHelp setupHelp) {
    if(setupHelp == SETUP_3D) {
        helpBoxSca->hide();
        qtOpenGlViewer->show();
        qtOpenGlViewer->resize(100,100);
        qtOpenGlViewer->repaint();
        qtOpenGlViewer->update();
        qtOpenGlViewer->updateGeometry();
    } else {
        qtOpenGlViewer->hide();
        helpBoxSca->show();
        if(setupHelp == COMPONENT_HELP_LENS) {
            helpContentImage->setIcon(helpLensIcon1);
            helpContentText->setText("Lens help");
        } else if(setupHelp == COMPONENT_HELP_CAMERA) {
            //helpContentImage->setIcon(helpLensIcon1);
            helpContentText->setText("Camera help");
        } else if(setupHelp == COMPONENT_HELP_ILLUMINATOR) {
            helpContentText->setText("Illuminator help");
        } else if(setupHelp == COMPONENT_HELP_HEAD) {
            helpContentImage->setIcon(helpHeadIcon1);
            helpContentText->setText("Head help");
        }
    }

}

