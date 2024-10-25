
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/qformlayout.h>
#include <QtWidgets/QLabel>
#include <QtWidgets/QtWidgets>
#include "setupGeometryDialog.h"
#include "../SVGIconColorAdjuster.h"

// Create the pupil detection settings dialog
// Given a pupil detection object to communicate to the detection algorithm objects there
SetupGeometryDialog::SetupGeometryDialog(QWidget *parent) :
        QDialog(parent),
        applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), parent)) {

    //this->setMinimumSize(800, 500);
    this->setMinimumSize(980, 600);
    this->setWindowTitle("Pupil Detection Settings");

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
    qtOpenGlViewer->setFixedSize(700,300);
    mainLayoutInnerCol1->addWidget(qtOpenGlViewer);

    aGroup->setLayout(aLayout);
    mainLayoutInnerCol1->addWidget(aGroup);

    // TODO: make code fit in 1/6th of the actual line count...

    // For each algorithm, a special widget is implemented that contains all algorithm specific parameters

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

