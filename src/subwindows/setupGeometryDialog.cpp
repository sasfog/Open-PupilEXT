
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
    //mainLayout->setMargin(10);
    mainLayout->setContentsMargins(10,10,10,10);
    QHBoxLayout *mainLayoutInner = new QHBoxLayout();
    //mainLayoutInner->setMargin(0);
    mainLayoutInner->setContentsMargins(0,0,0,0);
    QVBoxLayout *mainLayoutInnerCol1 = new QVBoxLayout();
    //mainLayoutInnerCol1->setMargin(0);
    mainLayoutInnerCol1->setContentsMargins(0,5,0,5);
    QVBoxLayout *mainLayoutInnerCol2 = new QVBoxLayout();
    //mainLayoutInnerCol2->setMargin(0);
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
    /*
    QTreeView *setupModelTree = new QTreeView();
     */

    // ":/" prefix needed, as the file is accessed from the Qt resources
    setupModel = new RemoteSetupModel(":/H3DModel.json", qtOpenGlViewer);

    //QJsonModel * model1 = new QJsonModel;
    setupModelTree = new QTreeView;
    setupModelTree->setModel(setupModel->rep);
    setupModelTree->setSelectionMode(QAbstractItemView::SingleSelection);
//    setupModelTree->setItemDelegateForRow()
    //model1->load("example.json");
    setupModelTree->expandAll();
    //setupModelTree->setFirstColumnSpanned()
    // hide all rows that show dim, loc, rot
    QList<QJsonTreeItem*> itemsToHide;
    // TODO NOTE: MODELINDEX IS JUST THE QJSONTREEITEM POINTER, CAST DIFFERENTLY
    itemsToHide.append(setupModel->findItemsByKey("Dim"));
    itemsToHide.append(setupModel->findItemsByKey("Loc"));
    itemsToHide.append(setupModel->findItemsByKey("Rot"));
    for(int k = 0; k < itemsToHide.size(); k++) {
        QJsonTreeItem* ith = itemsToHide[k];
        QModelIndex ithmi = setupModel->rep->parentIndexByItem(ith);
        setupModelTree->setRowHidden(ith->row(), ithmi, true);
    }


    QList<QJsonTreeItem*> aa;
    aa = setupModel->findItemsByKeyAndParentKey("Resolution X", "Screen");
    if(~aa.empty()) {
        aa[0]->setEditable(true);
        setupModel->subscribeOutbound(this, SLOT(screenResolutionXChanged(QVariant)),aa[0]);
    }

    aa = setupModel->findItemsByKeyAndParentKey("Resolution Y", "Screen");
    if(~aa.empty()) {
        aa[0]->setEditable(true);
        setupModel->subscribeOutbound(this, SLOT(screenResolutionYChanged(QVariant)),aa[0]);
    }

    aa = setupModel->findItemsByKeyAndParentKey("Physical Size X", "Screen");
    if(~aa.empty()) {
        aa[0]->setEditable(true);
        setupModel->subscribeOutbound(this, SLOT(screenPhysicalSizeXChanged(QVariant)),aa[0]);
    }

    aa = setupModel->findItemsByKeyAndParentKey("Physical Size Y", "Screen");
    if(~aa.empty()) {
        aa[0]->setEditable(true);
        setupModel->subscribeOutbound(this, SLOT(screenPhysicalSizeXChanged(QVariant)),aa[0]);
    }
    // TODO: ne abc rendben legyenek a json nodeok, hanem mindig a components legyen legalul
    //
    // TODO: JSOn beolvasó ne menjen tönkre ha véletlenül egy 2 objektumot elválasztó vessző hiányzik a fájlból, vagy véletlenül a legutolsó elem után még van egy vessző de nincs utsó elem
    //
    // TODO: link screen 1 Physical Size X and Y, and resolution X and Y to signals into eyetracking class for refresh
    // TODO: and also physical setup-dependent size-change refresh function(s) that substitute grometrical constraints in our simplistic geometry model
    // TODO: and also trigger GLView and treeview/model refresh.
    // TODO: And also set these values editable, but all others (DPI, etc. non-editable. Those will be updated however in the tree view by out setup-dependent refresh function(s)
    //
    // TODO: do the same for illuminator component:
    //     all values are read only
    //     illuminator properties can be automatically filled and changed if we choose a different illuminator from a list (TBD later)
    //      operating current and others can be updated by program only
    //
    // TODO: eyeball properties, and CVTarget properties are also read only
    //
    // TODO: sensor properties are all read only, filled out automatically by program, depending on camera selection
    //
    // TODO: lens properties are read only
    //
    // TODO: filter auto filled by program from selection, read only. TBD later for screw-on/screw-in
    //
    // TODO: camera all properties are filled by program yet
    //
    // TODO: the properties for camera, etc will either be auto filled by program, from another JSon... TBD later: custom item addition, and editing to those
    setupModelTree->header()->setStretchLastSection(false);
    setupModelTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    setupModelTree->setMouseTracking(true);
    setupModelTree->setColumnWidth(0, 100);
    //setupModelTree->setColumnWidth(1, 100);
    //setupModelTree->setColumnWidth(2, 100);

//    setupModelTree->setB

    //setupModel->rep->find
    connect(setupModelTree->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(onGeomSelectionChanged(QItemSelection, QItemSelection)));
    //connect(setupModelTree->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(onGeomSelectionChanged(QItemSelection, QItemSelection)));

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////

    qtOpenGlViewer->update();
    qtOpenGlViewer->goToDefaultView(4);

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

void SetupGeometryDialog::onGeomSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected) {

    dynamic_cast<QJsonModel *>(setupModelTree->model())->unhighlightAllSilently();

    // Normally this should never happen, but still we need to check
    if(selected.indexes().isEmpty()) {
        return;
    }

    QJsonTreeItem *selectedItem = static_cast<QJsonTreeItem *>(selected.indexes()[0].internalPointer());
    if(selectedItem->childCount() == 0) {
        qtOpenGlViewer->highlightGeom(selectedItem->parent());
        setupModelTree->model()->setData(selected.indexes()[0].parent(), true, Qt::BackgroundRole);
    } else {
        qtOpenGlViewer->highlightGeom(selectedItem);
    }
    qtOpenGlViewer->update();
    setupModelTree->model()->setData(selected.indexes()[0], true, Qt::BackgroundRole);

}











