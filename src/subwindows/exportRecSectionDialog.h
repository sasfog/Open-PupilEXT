#pragma once

/**
    @author Gabor Benyei
*/

#include <QtWidgets>
#include <QDialog>
#include <QCheckBox>
#include <QDebug>
#include <QLabel>
#include <QVBoxLayout>
#include "devices/fileCamera.h"
#include "custom-widgets/timestampSpinBox.h"

class ExportRecSectionDialog : public QDialog {
Q_OBJECT

    FileCamera *fileCamera;

public:

    enum ExportRecSectionResponse {PERFORM = 1};

    explicit ExportRecSectionDialog(
            const QString windowTitle,
            FileCamera* fileCamera,
            QWidget *parent = nullptr) :
            fileCamera(fileCamera),
            applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), this)) {

        this->setWindowTitle(windowTitle);
        this->setMinimumSize(200,180);

        if(fileCamera->getNumImagesTotal() < 2)
            close();

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        // NOTE: A cached system with any ever-opened recording could be cool as well. But way overkill

        uint64_t fromTimestampMax = fileCamera->getTimestampForFrameNumber(fileCamera->getNumImagesTotal()-2);
        int fromFrameMax = fileCamera->getNumImagesTotal()-1;
        uint64_t toTimestampMax = fileCamera->getTimestampForFrameNumber(fileCamera->getNumImagesTotal()-1);
        int toFrameMax = fileCamera->getNumImagesTotal();

        QString destDirectory;
        //QString recordingName;
        int fromFrameVal = 1;
        int toFrameVal = fileCamera->getNumImagesTotal();

        QString openRecordingFullPath = fileCamera->getRecordingFullPath();

        QStringList lst = openRecordingFullPath.split('/');
        QString openRecordingName = lst[lst.count()-1];
        QString openRecordingParentLocation = openRecordingFullPath.chopped(openRecordingName.length());
        QString openRecordingNameBase = openRecordingName;
        while(openRecordingNameBase.endsWith(".zip") && openRecordingNameBase.length() > 4)
            openRecordingNameBase = openRecordingName.mid(0, openRecordingName.lastIndexOf('.'));
        //QString suggestedPathAndName = recentRecParentLocation + '/' + openRecordingNameBase + ".gif";

        // TODO: add directory picker dialog button (where to save gif)

        // ez nem jó. csak akkor kéne ezeknek újraíródnia, vagy legalábbis
        //  a fájlnévnek csak, ha új recordingot nyitunk meg
        QString recentDestFile = applicationSettings->value("ExportRecSection.DestFile", (openRecordingNameBase+".gif")).toString();
        if(!recentDestFile.isEmpty() && recentDestFile == (fileCamera->getRecordingName() + ".gif")) {
            destDirectory = applicationSettings->value("ExportRecSection.DestDirectory", openRecordingParentLocation).toString();
            fromFrameVal = applicationSettings->value("ExportRecSection.FromFrameNumber", 1).toInt();
            toFrameVal = applicationSettings->value("ExportRecSection.ToFrameNumber", toFrameMax).toInt();
        } else {
            recentDestFile = (openRecordingNameBase+".gif");
            destDirectory = openRecordingParentLocation;
            applicationSettings->setValue("ExportRecSection.FromFrameNumber", 1);
            applicationSettings->setValue("ExportRecSection.ToFrameNumber", toFrameMax);
        }
        //recordingName = recentRecName;

        QLabel *destDirectoryLabel = new QLabel(tr("Destination directory:"));
        //destDirectoryLabel->setFixedWidth(120);
        auto destDirectoryBox = new QLineEdit();
        destDirectoryBox->setReadOnly(false);
        destDirectoryBox->home(true); // align to content right
        //destDirectoryBox->setMaximumWidth(70);
        destDirectoryBox->setText(destDirectory);

        mainLayout->addWidget(destDirectoryLabel);
        mainLayout->addWidget(destDirectoryBox);

        QLabel *destFileLabel = new QLabel(tr("Destination file:"));
        //destFileLabel->setFixedWidth(120);
        auto destFileBox = new QLineEdit();
        destFileBox->setReadOnly(false);
        //destFileBox->home(true); // align to content right
        //destFileBox->setMaximumWidth(70);
        destFileBox->setText(recentDestFile);

        mainLayout->addWidget(destFileLabel);
        mainLayout->addWidget(destFileBox);

        /////////////////////////////////////////////////
        QHBoxLayout *layoutRow5 = new QHBoxLayout;
        //layoutRow5->setMargin(0);
        layoutRow5->setContentsMargins(0,0,0,0);
        QFrame *line1 = new QFrame();
        line1->setFrameShape(QFrame::HLine);
        line1->setFrameShadow(QFrame::Raised);
        layoutRow5->addWidget(line1);
        mainLayout->addLayout(layoutRow5);

        QHBoxLayout *fromTimestampRowLayout = new QHBoxLayout();
        fromTimestampRowLayout->setContentsMargins(0,0,0,0);
        QLabel *fromTimestampLabel = new QLabel(tr("From timestamp [ms]:"));
        fromTimestampLabel->setFixedWidth(120);
        //fromTimestampBox = new QLineEdit();
        fromTimestampBox = new TimestampSpinBox(fileCamera);
        fromTimestampBox->setReadOnly(false);
        fromTimestampBox->setFixedWidth(140);
        fromTimestampBox->setMinimum(0);
        fromTimestampBox->setMaximum(fromTimestampMax);
        //fromFrameBox->setValue(fromTimestampVal);
        //fromTimestampBox->setWrapping(true);
        fromTimestampRowLayout->addWidget(fromTimestampLabel);
        fromTimestampRowLayout->addWidget(fromTimestampBox);
        fromTimestampRowLayout->addSpacerItem(new QSpacerItem(10, 20, QSizePolicy::Expanding));

        mainLayout->addLayout(fromTimestampRowLayout);

        QHBoxLayout *fromFrameRowLayout = new QHBoxLayout();
        QLabel *fromFrameLabel = new QLabel(tr("From frame number:"));
        fromFrameLabel->setFixedWidth(120);
        fromFrameBox = new QSpinBox();
        fromFrameBox->setReadOnly(false);
        fromFrameBox->setMaximumWidth(70);
        fromFrameBox->setMinimum(1);
        fromFrameBox->setMaximum(fromFrameMax);
        fromFrameBox->setValue(fromFrameVal);
        //fromFrameBox->setWrapping(true);
        fromFrameRowLayout->addWidget(fromFrameLabel);
        fromFrameRowLayout->addWidget(fromFrameBox);

        mainLayout->addLayout(fromFrameRowLayout);

        /////////////////////////////////////////////////
        QHBoxLayout *layoutRow6 = new QHBoxLayout;
        //layoutRow6->setMargin(0);
        layoutRow6->setContentsMargins(0,0,0,0);
        QFrame *line2 = new QFrame();
        line2->setFrameShape(QFrame::HLine);
        line2->setFrameShadow(QFrame::Raised);
        layoutRow6->addWidget(line2);
        mainLayout->addLayout(layoutRow6);

        QHBoxLayout *toTimestampRowLayout = new QHBoxLayout();
        toTimestampRowLayout->setContentsMargins(0,0,0,0);
        QLabel *toTimestampLabel = new QLabel(tr("To timestamp [ms]:"));
        toTimestampLabel->setFixedWidth(120);
        //toTimestampBox = new QLineEdit();
        toTimestampBox = new TimestampSpinBox(fileCamera);
        toTimestampBox->setReadOnly(false);
        toTimestampBox->setFixedWidth(140);
        toTimestampBox->setMinimum(1);
        toTimestampBox->setMaximum(toTimestampMax);
        //toFrameBox->setValue(fromTimestampVal);
        //toTimestampBox->setWrapping(true);
        toTimestampRowLayout->addWidget(toTimestampLabel);
        toTimestampRowLayout->addWidget(toTimestampBox);
        toTimestampRowLayout->addSpacerItem(new QSpacerItem(10, 20, QSizePolicy::Expanding));

        mainLayout->addLayout(toTimestampRowLayout);

        QHBoxLayout *toFrameRowLayout = new QHBoxLayout();
        QLabel *toFrameLabel = new QLabel(tr("To frame number:"));
        toFrameLabel->setFixedWidth(120);
        toFrameBox = new QSpinBox();
        toFrameBox->setReadOnly(false);
        toFrameBox->setMaximumWidth(70);
        toFrameBox->setMinimum(2);
        toFrameBox->setMaximum(toFrameMax);
        toFrameBox->setValue(toFrameVal);
        //toFrameBox->setWrapping(true);
        toFrameRowLayout->addWidget(toFrameLabel);
        toFrameRowLayout->addWidget(toFrameBox);

        mainLayout->addLayout(toFrameRowLayout);

        /////////////////////////////////////////////////
        QHBoxLayout *layoutRow7 = new QHBoxLayout;
        //layoutRow6->setMargin(0);
        layoutRow7->setContentsMargins(0,0,0,0);
        QFrame *line3 = new QFrame();
        line3->setFrameShape(QFrame::HLine);
        line3->setFrameShadow(QFrame::Raised);
        layoutRow7->addWidget(line3);
        mainLayout->addLayout(layoutRow7);

        // TODO: make usable
        includeShownOverlaysBox = new QCheckBox("Include shown overlays");
        includeShownOverlaysBox->setStyle(QStyleFactory::create("Fusion")); // Since upgrade to Qt 6.8.3 this is needed
        includeShownOverlaysBox->setChecked(
                SupportFunctions::readBoolFromQSettings("ExportRecSection.IncludeShownOverlays", true, applicationSettings)
                );
        //mainLayout->addWidget(includeShownOverlaysBox);

        cropToPDROIBox = new QCheckBox("Crop to Pupil Detection ROI (tracking only)");
        cropToPDROIBox->setStyle(QStyleFactory::create("Fusion")); // Since upgrade to Qt 6.8.3 this is needed
        cropToPDROIBox->setChecked(
                SupportFunctions::readBoolFromQSettings("ExportRecSection.CropToPDROI", false, applicationSettings)
                );
        // TODO
        //mainLayout->addWidget(cropToPDROIBox);

        embedFrameInfoBox = new QCheckBox("Embed frame information");
        embedFrameInfoBox->setStyle(QStyleFactory::create("Fusion")); // Since upgrade to Qt 6.8.3 this is needed
        embedFrameInfoBox->setChecked(
                SupportFunctions::readBoolFromQSettings("ExportRecSection.EmbedFrameInfo", true, applicationSettings)
        );
        mainLayout->addWidget(embedFrameInfoBox);

        uniformPortableImageSizeBox = new QCheckBox("Reduce image size for portability");
        uniformPortableImageSizeBox->setStyle(QStyleFactory::create("Fusion")); // Since upgrade to Qt 6.8.3 this is needed
        uniformPortableImageSizeBox->setChecked(
                SupportFunctions::readBoolFromQSettings("ExportRecSection.UniformPortableImageSize", true, applicationSettings)
        );
        // TODO
        //mainLayout->addWidget(uniformPortableImageSizeBox);

        // frame number
        // timestamp
        // trial
        // message
        // camera identity
        // eye identity
        // Progress bar at bottom in 1 mm height

        // NOTE: each camera view will be saved separately

        /////////////////////////////////////////////////
        QHBoxLayout *layoutRow8 = new QHBoxLayout;
        //layoutRow8->setMargin(0);
        layoutRow8->setContentsMargins(0,0,0,0);
        QFrame *line4 = new QFrame();
        line4->setFrameShape(QFrame::HLine);
        line4->setFrameShadow(QFrame::Raised);
        layoutRow8->addWidget(line4);
        mainLayout->addLayout(layoutRow8);

        QLabel *noteLabel2 = new QLabel(tr(
                "Export will stop any ongoing data recording and streaming.\n"
                "Certain functions will be unavailable during export.\n"
                "Multiple cameras or perspectives will be unified in one file.\n"
                "Result will have a frame rate of 30 FPS."
                ));
        SupportFunctions::setSmallerLabelFontSize(noteLabel2);
        mainLayout->addWidget(noteLabel2);


        QFormLayout *buttonsLayout = new QFormLayout();
        buttonPerform = new QPushButton("Perform export");
        // buttonOption1->setFixedWidth(option1Width); // 210
        // //buttonOption2->setFixedWidth(option2Width); // 200
        buttonsLayout->addWidget(buttonPerform);
        mainLayout->addLayout(buttonsLayout);

        buttonPerform->setDefault(false);

        setLayout(mainLayout);

        fromTimestampBox->setValue(fromFrameBox->value());
        toTimestampBox->setValue(toFrameBox->value());

        applicationSettings->setValue("ExportRecSection.DestName", destFileBox->text());
        applicationSettings->setValue("ExportRecSection.DestDirectory", destDirectoryBox->text());
        applicationSettings->setValue("ExportRecSection.FromFrameNumber", fromFrameBox->value());
        applicationSettings->setValue("ExportRecSection.ToFrameNumber", toFrameBox->value());

        connect(destDirectoryBox, SIGNAL(textChanged(QString)), this, SLOT(onDestDirectoryBoxTextChanged(QString)));
        connect(destFileBox, SIGNAL(textChanged(QString)), this, SLOT(onDestFileBoxTextChanged(QString)));

        connect(fromFrameBox, SIGNAL(valueChanged(int)), this, SLOT(onFromFrameSelected(int)));
        connect(fromTimestampBox, SIGNAL(valueChanged(double)), this, SLOT(onFromTimestampSelected(double)));
        connect(toFrameBox, SIGNAL(valueChanged(int)), this, SLOT(onToFrameSelected(int)));
        connect(toTimestampBox, SIGNAL(valueChanged(double)), this, SLOT(onToTimestampSelected(double)));
        connect(buttonPerform, &QPushButton::clicked, this, &ExportRecSectionDialog::onPerformClicked);

        connect(includeShownOverlaysBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(onIncludeShownOverlaysChanged(Qt::CheckState)));
        connect(cropToPDROIBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(onCropToPDROIChanged(Qt::CheckState)));
        connect(embedFrameInfoBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(onEmbedFrameInfoChanged(Qt::CheckState)));
        connect(uniformPortableImageSizeBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(onUniformPortableImageSizeChanged(Qt::CheckState)));

    };
    ~ExportRecSectionDialog() override = default;
    ExportRecSectionResponse getResponse() {
        return exportRecSectionResponse;
    };

private slots:
    void onPerformClicked() {
        //rememberChoice = rememberChoiceBox->isChecked();
        exportRecSectionResponse = ExportRecSectionResponse::PERFORM;
        accept();
    };
    void onDestDirectoryBoxTextChanged(QString val) {
        applicationSettings->setValue("ExportRecSection.DestDirectory", val);
    };
    void onDestFileBoxTextChanged(QString val) {
        applicationSettings->setValue("ExportRecSection.DestFile", val);
    };
    void onFromTimestampSelected(double frameNumber) {
        fromFrameBox->setValue(frameNumber);
    };
    void onToTimestampSelected(double frameNumber) {
        toFrameBox->setValue(frameNumber);
    };
    void onFromFrameSelected(int val) {

        fromTimestampBox->blockSignals(true);
        // TODO: FRAME NUMBER IS AN INDEX (from 0) EVERYWHERE, BUT ON GUI IT BEGINS FROM 1. NAMING IS INCONSISTENT. CLEAR EVERYWHERE
        fromTimestampBox->setValue(val);
        toFrameBox->setMinimum(val+1);
        // TODO: MIN MAX SETTING HERE IS BUGGY, yet commented out
        //toTimestampBox->setMinimum(fileCamera->getTimestampForFrameNumber(val+1)); // TODO: this should also work with frame number
        fromTimestampBox->blockSignals(false);

        applicationSettings->setValue("ExportRecSection.FromFrameNumber", val);
    };
    void onToFrameSelected(int val) {

        toTimestampBox->blockSignals(true);
        // TODO: FRAME NUMBER IS AN INDEX (from 0) EVERYWHERE, BUT ON GUI IT BEGINS FROM 1. NAMING IS INCONSISTENT. CLEAR EVERYWHERE
        toTimestampBox->setValue(val);
        fromFrameBox->setMaximum(val-1);
        // TODO: MIN MAX SETTING HERE IS BUGGY, yet commented out
        //fromTimestampBox->setMaximum(fileCamera->getTimestampForFrameNumber(val)); // TODO: this should also work with frame number
        toTimestampBox->blockSignals(false);

        applicationSettings->setValue("ExportRecSection.ToFrameNumber", val);
    };
    void onIncludeShownOverlaysChanged(Qt::CheckState val) {
        applicationSettings->setValue("ExportRecSection.IncludeShownOverlays", (bool)val );
    };
    void onCropToPDROIChanged(Qt::CheckState val) {
        applicationSettings->setValue("ExportRecSection.CropToPDROI", (bool)val );
    };
    void onEmbedFrameInfoChanged(Qt::CheckState val) {
        applicationSettings->setValue("ExportRecSection.EmbedFrameInfo", (bool)val );
    };
    void onUniformPortableImageSizeChanged(Qt::CheckState val) {
        applicationSettings->setValue("ExportRecSection.UniformPortableImageSize", (bool)val );
    };

private:

    QSettings *applicationSettings;

    ExportRecSectionResponse exportRecSectionResponse;


    QLineEdit *destDirectoryBox;
    QLineEdit *destFileBox;
    TimestampSpinBox *fromTimestampBox;
    QSpinBox *fromFrameBox;
    TimestampSpinBox *toTimestampBox;
    QSpinBox *toFrameBox;

    QCheckBox *includeShownOverlaysBox;
    QCheckBox *cropToPDROIBox;
    QCheckBox *embedFrameInfoBox;
    QCheckBox *uniformPortableImageSizeBox;

    QPushButton *buttonPerform;
};
