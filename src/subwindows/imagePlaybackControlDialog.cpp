#include <QtWidgets>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/qformlayout.h>
#include <QtWidgets/QSpinBox>
#include <iostream>
#include "imagePlaybackControlDialog.h"
#include "timestampSpinBox.h"
#include "../SVGIconColorAdjuster.h"


// TODO: readSettings and updateForm calls necessary when someone changes settings through remote control command, via QSettings

// Create a settings dialog for the general software settings
// Settings are read upon creation from the QT application settings if existing
ImagePlaybackControlDialog::ImagePlaybackControlDialog(FileCamera *fileCamera, PupilDetection *pupilDetection, RecEventTracker *recEventTracker, QWidget *parent) :
        QWidget(parent),
        fileCamera(fileCamera),
        pupilDetection(pupilDetection),
        recEventTracker(recEventTracker),
        applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), this)) {
 

    //this->setMinimumSize(500, 400); 
    this->setMinimumSize(750, 280);
    this->setWindowTitle("Image Playback Control");

    readSettings();

    // These lines are very important here, DO NOT REMOVE
    tempSyncRecordCsv = syncRecordCsv;
    tempSyncStream = syncStream;

    createForm();

    numImagesTotal = fileCamera->getNumImagesTotal();
    numImagesTotalLabel->setText(" / " + QString::number(numImagesTotal));
    timeTotal = QTime::fromMSecsSinceStartOfDay(fileCamera->getRecordingDuration());
    lastTimestamp = 0;
    startTimestamp = 0;
    setPlaybackLoop(playbackLoop);


    updateInfoInternal(0);
    //emit stillImageChange(0); // cannot do it right here, because signals and slots aren't yet connected out in mainwindow code

}

ImagePlaybackControlDialog::~ImagePlaybackControlDialog() {
}

void ImagePlaybackControlDialog::createForm() {
    QGridLayout *layout = new QGridLayout();

    QFormLayout *infoLayout = new QFormLayout();
    infoGroup = new QGroupBox("Playback info");

    QHBoxLayout *timestampRowLayout = new QHBoxLayout();
    timestampRowLayout->setContentsMargins(0,0,0,0);
    QLabel *timestampLabel = new QLabel(tr("Timestamp [ms]:"));
    timestampLabel->setFixedWidth(120);
    //timestampVal = new QLineEdit();
    timestampVal = new TimestampSpinBox(fileCamera);
    timestampVal->setReadOnly(false);
    timestampVal->setFixedWidth(140);
    timestampVal->setMinimum(0);
    uint64_t timestampMax = fileCamera->getTimestampForFrameNumber(fileCamera->getNumImagesTotal()-1);
    timestampVal->setMaximum(timestampMax);
    timestampVal->setWrapping(true);
    timestampRowLayout->addWidget(timestampLabel);
    timestampRowLayout->addWidget(timestampVal);
    timestampRowLayout->addSpacerItem(new QSpacerItem(10, 20, QSizePolicy::Expanding));
    infoLayout->addRow(timestampRowLayout);

    QHBoxLayout *selectedFrameRowLayout = new QHBoxLayout();
    QLabel *frameLabel = new QLabel(tr("Frame:"));
    frameLabel->setFixedWidth(120);
    selectedFrameBox = new QSpinBox();
    selectedFrameBox->setReadOnly(false);
    selectedFrameBox->setMaximumWidth(70);
    selectedFrameBox->setMinimum(1);
    selectedFrameBox->setMaximum(fileCamera->getNumImagesTotal());
    selectedFrameBox->setValue(selectedFrameVal);
    selectedFrameBox->setWrapping(true);
    selectedFrameRowLayout->addWidget(selectedFrameBox);

    numImagesTotalLabel = new QLabel("-");
    selectedFrameRowLayout->addWidget(numImagesTotalLabel);
    infoLayout->addRow(frameLabel, selectedFrameRowLayout);

    QLabel *timestampHumanLabel = new QLabel(tr("Date and time:"));
    timestampHumanValLabel = new QLabel("-");
    infoLayout->addRow(timestampHumanLabel, timestampHumanValLabel);


    // GB: number of (so far) skipped images:
    // Sounds cool, but it is complicated to implement correctly.
    // As there can be quite a delay sometimes between the image reading and the moment 
    // the processed image arrives here (at updateInfo), we either need to keep track of
    // numSkipped in each processed CameraImage or call imageReader to retrieve this number
    // from a list that it keeps track of, for each image... 
    // Both are unnecessarily complex for this simple purpose
    // // gets reset on stop button click
    //QLabel *skippedLabel = new QLabel(tr("Skipped frames:")); 
    //skippedValLabel = new QLabel(tr("0"));
    //infoLayout->addRow(skippedLabel, skippedValLabel);

    QLabel *elapsedTimeLabel = new QLabel(tr("Elapsed time:"));
    elapsedTimeValLabel = new QLabel("-");
    infoLayout->addRow(elapsedTimeLabel, elapsedTimeValLabel);

    QLabel *acqFPSLabel = new QLabel(tr("Acquisition FPS:"));
    acqFPSValLabel = new QLabel("-");
    infoLayout->addRow(acqFPSLabel, acqFPSValLabel);

    QLabel *percentLabel = new QLabel(tr("Playback at [%]:"));
    percentValLabel = new QLabel("-");
    infoLayout->addRow(percentLabel, percentValLabel);

    QLabel *trialLabel = new QLabel(tr("Trial:"));
    trialValLabel = new QLabel("-");
    infoLayout->addRow(trialLabel, trialValLabel);

    QLabel *messageLabel = new QLabel(tr("Message:"));
    messageValLabel = new QLabel("-");
    infoLayout->addRow(messageLabel, messageValLabel);

    infoGroup->setLayout(infoLayout);
    layout->addWidget(infoGroup, 0, 0, 1, 1);

    QGridLayout *controlLayout = new QGridLayout();
    QGroupBox *controlGroup = new QGroupBox("Control");

    slider = new PlaybackSlider();
    slider->setOrientation(Qt::Horizontal);
    slider->setFocusPolicy(Qt::StrongFocus);
    slider->setTickPosition(QSlider::TicksAbove);
    slider->setMinimumWidth(200);
    slider->setTickInterval(10); //max value is 100, and every 10th value gets a visual mark
    slider->setSingleStep(1);
    slider->setMaximum(99);
    slider->setMinimum(0);


    dial = new PlaybackDial();
    dial->setFocusPolicy(Qt::StrongFocus);
    connect(dial, SIGNAL(incremented()), this, SLOT(onDialForward()));
    connect(dial, SIGNAL(decremented()), this, SLOT(onDialBackward()));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(onSliderValueChanged(int)));

    connect(dial, SIGNAL(incremented()), this, SIGNAL(cameraPlaybackPositionChanged()));
    connect(dial, SIGNAL(decremented()), this, SIGNAL(cameraPlaybackPositionChanged()));
    connect(slider, SIGNAL(valueChanged(int)), this, SIGNAL(cameraPlaybackPositionChanged()));
    
    controlLayout->addWidget(slider, 0, 0, 1, 3);
    controlLayout->addWidget(dial, 1, 2, 6, 1);

    const QIcon startIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/media-playback-start.svg"), applicationSettings);
    startPauseButton = new QPushButton();
    startPauseButton->setToolTip("Start/Pause image playback");
    startPauseButton->setIcon(startIcon);

    const QIcon stopIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/media-playback-stop.svg"), applicationSettings);
    stopButton = new QPushButton();
    stopButton->setToolTip("Stop image playback");
    stopButton->setIcon(stopIcon);

    controlLayout->addWidget(startPauseButton, 2, 0, 1, 1);
    controlLayout->addWidget(stopButton, 2, 1, 1, 1);

    QLabel *playbackFPSLabel = new QLabel(tr("Playback FPS:"));
    playbackFPSLabel->setAccessibleDescription("Speed with which offline recordings are played. Speed of 0 will make it play as fast as possible.");
    playbackFPSVal = new QSpinBox();
    playbackFPSVal->setMinimum(0);
    playbackFPSVal->setMaximum(999);
    playbackFPSVal->setSingleStep(1);
    playbackFPSVal->setValue(playbackSpeed);
    controlLayout->addWidget(playbackFPSLabel, 3, 0, 1, 1);
    controlLayout->addWidget(playbackFPSVal, 3, 1, 1, 1);

    loopBox = new QCheckBox("Loop playback when end is reached");
    loopBox->setChecked(playbackLoop);
    controlLayout->addWidget(loopBox, 4, 0, 1, 2);

    syncRecordCsvBox = new QCheckBox("Start/pause Data Recording in sync");
    syncRecordCsvBox->setChecked(syncRecordCsv);
    controlLayout->addWidget(syncRecordCsvBox, 5, 0, 1, 2);

    syncStreamBox = new QCheckBox("Start/pause Data Streaming in sync");
    syncStreamBox->setChecked(syncStream);
    controlLayout->addWidget(syncStreamBox, 6, 0, 1, 2);

    controlGroup->setLayout(controlLayout);
    //
    layout->addWidget(controlGroup, 0, 1, 1, 1);

    setLayout(layout);


    connect(startPauseButton, SIGNAL(clicked()), this, SLOT(onStartPauseButtonClick()));
    connect(stopButton, SIGNAL(clicked()), this, SLOT(onStopButtonClick()));

    //bool succeeded = connect(pupilDetection, SIGNAL(processedImageLowFPS(CameraImage)), this, SLOT(updateInfo(CameraImage)));
    //connect(fileCamera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(updateInfo(CameraImage)));
    //connect(fileCamera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(updateSliderColorTick(CameraImage)));
    connect(pupilDetection, SIGNAL(processedImageLowFPS(CameraImage)), this, SLOT(updateInfo(CameraImage)));
    //connect(pupilDetection, SIGNAL(processedImageLowFPS(CameraImage)), this, SLOT(updateSliderColorTick(CameraImage)));
    // BREAKPOINT
    //connect(pupilDetection, SIGNAL(processingStarted()), this, SLOT(onPupilDetectionStart()));
    //connect(pupilDetection, SIGNAL(processingFinished()), this, SLOT(onPupilDetectionStop()));

    connect(playbackFPSVal, SIGNAL(valueChanged(int)), this, SLOT(setPlaybackSpeed(int)));
    connect(loopBox, SIGNAL(toggled(bool)), this, SLOT(setPlaybackLoop(bool)));
    connect(syncRecordCsvBox, SIGNAL(toggled(bool)), this, SLOT(setSyncRecordCsv(bool)));
    connect(syncStreamBox, SIGNAL(toggled(bool)), this, SLOT(setSyncStream(bool)));
    connect(selectedFrameBox, SIGNAL(valueChanged(int)), this, SLOT(onFrameSelected(int)));
    connect(timestampVal, SIGNAL(valueChanged(double)), this, SLOT(onTimestampSelected(double)));
    connect(fileCamera, SIGNAL(endReached()), this, SLOT(onEndReached()));
    connect(fileCamera, SIGNAL(finished()), this, SLOT(onFinished()));
}

void ImagePlaybackControlDialog::updateSliderColorTick(const CameraImage &cimg) {
    // GB: even though this gets called really often, the slider's setter doesnt invalidate the widget, 
    // so even fast updating does not slow the GUI thread
    //if(numImagesTotal>=1)
        //slider->setColorTickPos((cimg.frameNumber+1)/(float)numImagesTotal);
}


//void ImagePlaybackControlDialog::onPupilDetectionStart() {
//    disconnect(fileCamera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(updateInfo(CameraImage)));
//    connect(pupilDetection, SIGNAL(processedImageLowFPS(CameraImage)), this, SLOT(updateInfo(CameraImage)));
//}
//
//void ImagePlaybackControlDialog::onPupilDetectionStop() {
//    disconnect(pupilDetection, SIGNAL(processedImageLowFPS(CameraImage)), this, SLOT(updateInfo(CameraImage)));
//    connect(fileCamera, SIGNAL(onNewGrabResult(CameraImage)), this, SLOT(updateInfo(CameraImage)));
//}

// when someone seeks, but there is no playback going on
void ImagePlaybackControlDialog::updateInfoInternal(int frameNumber) {
    uint64_t currTimestamp = fileCamera->getTimestampForFrameNumber(frameNumber);

    QDateTime date = QDateTime::fromMSecsSinceEpoch(currTimestamp);
    //date.toString("yyyy-MMM-dd hh:mm:ss");

    acqFPS = 0.0;
    if(lastTimestamp != 0 && lastPlayedFrame != 0) {
        int frameNumberDiff = abs(lastPlayedFrame - frameNumber);
        acqFPS = 1 / (float) ((currTimestamp - lastTimestamp) / static_cast<float>(frameNumberDiff)) * 1000; //timestamps are in millisecond
    }

    elapsedMs = 0.0;
    if(startTimestamp != 0)
        elapsedMs = (currTimestamp-startTimestamp);

    QTime timeElapsed = QTime::fromMSecsSinceStartOfDay(elapsedMs);

    //timestampValLabel->setText(QString::number(img.timestamp));
    timestampVal->setValue(frameNumber+1);
    timestampHumanValLabel->setText(date.toString("yyyy. MMM dd. hh:mm:ss"));
    //timestampHumanValLabel->setText(QLocale::system().toString(date));
    acqFPSValLabel->setText("-");
    //elapsedTimeValLabel->setText(QString::number(elapsedMs));
    elapsedTimeValLabel->setText(timeElapsed.toString("hh:mm:ss") + "\t/ " + timeTotal.toString("hh:mm:ss"));
    percentValLabel->setText(QString::number((float)(frameNumber+1)/(float)numImagesTotal*(float)100,'f',1)); 
    if(recEventTracker) {
        trialValLabel->setText(QString::number(recEventTracker->getTrialIncrement(currTimestamp).trialNumber));
        messageValLabel->setText(SupportFunctions::shortenStringForDisplay(recEventTracker->getMessage(currTimestamp).messageString,20));
    }

    lastTimestamp = currTimestamp;
    lastPlayedFrame = frameNumber;
    if(startTimestamp == 0)
        startTimestamp = currTimestamp;

}

void ImagePlaybackControlDialog::updateInfo(const CameraImage &img) {
    // NOTE: this img.frameNumber field existed already/originally in pupilEXT beta 0.1.1 and is used in calibration code
    // Pay attention, it is an INDEX, so it starts from 0

//    qDebug() << "New image arrived, framenumber: " << img.frameNumber;
    updateInfo((quint64)img.timestamp, (int)img.frameNumber);
}

// NOTE: now it works with the logic that: whatever gets read by imageReader, must arrive as a CameraImage once later
// no matter if the image is read for just displaying it, or for pupil detection. So if we check the
// timestamp of the last read image from imageReader, we can wait until the image with that timestamp comes
// down the pipeline and arrives at updateInfo method.
// IMPORTANT: Qt doesnt like uint64_t. Connection makes success, but signals never arrive. So I used quint64 instead
void ImagePlaybackControlDialog::updateInfo(quint64 timestamp, int frameNumber) {

    // qDebug() << "arrived frameNumber: " << frameNumber;
    // qDebug() << "arrived timestamp: " << timestamp;

    // NOTE: if it is the last frame in the whole playback, we need to draw it anyhow
//    if ((finished && endReached && selectedFrameVal < fileCamera->getNumImagesTotal()) || (!finished && !paused)){

        QDateTime date = QDateTime::fromMSecsSinceEpoch(timestamp);

        acqFPS = 0.0;
        if(lastTimestamp != 0 && lastPlayedFrame != 0) {
            int frameNumberDiff = abs(lastPlayedFrame - frameNumber);
            acqFPS = 1 / ((timestamp - lastTimestamp) / static_cast<float>(frameNumberDiff)) * 1000; // timestamps are in millisecond
        }
        elapsedMs = 0.0;
        if(startTimestamp != 0)
            elapsedMs = (timestamp-startTimestamp);

        QTime timeElapsed = QTime::fromMSecsSinceStartOfDay(elapsedMs);

        //timestampValLabel->setText(QString::number(img.timestamp));
        timestampVal->setValue(frameNumber+1);
        timestampHumanValLabel->setText(date.toString("yyyy. MMM dd. hh:mm:ss"));
        //timestampHumanValLabel->setText(QLocale::system().toString(date));
        selectedFrameBox->setValue(frameNumber + 1);
        if(acqFPS == 0)
            acqFPSValLabel->setText("-");
        else if(acqFPS > 0 && acqFPS < 1)
            acqFPSValLabel->setText(QString::number(acqFPS,'f',4));
        else
            acqFPSValLabel->setText(QString::number(round(acqFPS)));
        //elapsedTimeValLabel->setText(QString::number(elapsedMs));
        elapsedTimeValLabel->setText(timeElapsed.toString("hh:mm:ss") + "\t/ " + timeTotal.toString("hh:mm:ss"));
        percentValLabel->setText(QString::number((float)(frameNumber+1)/(float)numImagesTotal*(float)100,'f',1));
        if(recEventTracker) {
            trialValLabel->setText(QString::number(recEventTracker->getTrialIncrement(timestamp).trialNumber));
            messageValLabel->setText(SupportFunctions::shortenStringForDisplay(recEventTracker->getMessage(timestamp).messageString,20));
        }

        // NOTE: workaround to not emit valuechanged signals, so it gets emitted only if the user interacts with it
        slider->blockSignals(true);
        int gg = floor(99*((frameNumber)/(float)(numImagesTotal - 1)));
//        qDebug() << "Set slider to: " << gg;
        slider->setValue( gg );
        slider->blockSignals(false);
        //}

        lastTimestamp = timestamp;
        lastPlayedFrame = frameNumber;
        if(startTimestamp == 0)
            startTimestamp = timestamp;

//    }

    if (finished || paused){
//        resetState();
        emit onPlaybackStopInitiated();
    }
}

void ImagePlaybackControlDialog::onStartPauseButtonClick() {
    emit cameraPlaybackChanged();
}

void ImagePlaybackControlDialog::onStopButtonClick() {
//    qDebug()<<"Stopping FileCamera Click";
    fileCamera->stop();

    if(paused)
        finished = true;
//    resetState();
    emit onPlaybackStopInitiated();
}

void ImagePlaybackControlDialog::onEndReached() {
    endReached = true;
    // NOTE: gets called whenever imageReader is finished with reading and sending images

}

void ImagePlaybackControlDialog::onAutomaticFinish() {
    onStopButtonClick();
}

void ImagePlaybackControlDialog::onDialForward() {
    lastTimestamp = 0;
    fileCamera->step1frameNext();
    selectedFrameVal+= 1;
    selectedFrameBox->setValue(selectedFrameVal);
}

void ImagePlaybackControlDialog::onDialBackward() {
    lastTimestamp = 0;
    fileCamera->step1framePrev();
    selectedFrameVal-= 1;
    selectedFrameBox->setValue(selectedFrameVal);
}

void ImagePlaybackControlDialog::onSliderValueChanged(int val) {
    int frameNumber = floor((float)(val)/(float)slider->maximum()*(float)(numImagesTotal-1));
    //qDebug() << "Seek to frame number (INDEX, starting from 0): " << frameNumber;
    fileCamera->seekToFrame(frameNumber);
    selectedFrameBox->setValue(frameNumber + 1);
    if(!playImagesOn) {
        updateInfoInternal(frameNumber);
        emit stillImageChange(frameNumber);
    }
}


void ImagePlaybackControlDialog::readSettings() {

    const QByteArray m_playbackSpeed = applicationSettings->value("playbackSpeed", QByteArray()).toByteArray();
    if (!m_playbackSpeed.isEmpty())
        setPlaybackSpeed(m_playbackSpeed.toInt());
    else
        setPlaybackSpeed(30);

    syncRecordCsv = SupportFunctions::readBoolFromQSettings("syncRecordCsv", false, applicationSettings);
    syncStream = SupportFunctions::readBoolFromQSettings("syncStream", false, applicationSettings);
    playbackLoop = SupportFunctions::readBoolFromQSettings("playbackLoop", true, applicationSettings);

}

/*
// Save settings to QT application settings 
void ImagePlaybackControlDialog::saveUniversalSettings() {

    applicationSettings->setValue("playbackSpeed", playbackSpeed);
    applicationSettings->setValue("playbackLoop", playbackLoop);
    applicationSettings->setValue("syncRecordCsv", syncRecordCsv);
    applicationSettings->setValue("syncStream", syncStream);
}
*/

void ImagePlaybackControlDialog::updateForm() {
    playbackFPSVal->setValue(playbackSpeed);
    loopBox->setChecked(playbackLoop);
    syncRecordCsvBox->setChecked(syncRecordCsv);
    syncStreamBox->setChecked(syncStream);
    
    this->update();
}

// Set the playback speed in frames per second
void ImagePlaybackControlDialog::setPlaybackSpeed(int m_playbackSpeed) {
    playbackSpeed = m_playbackSpeed;
    applicationSettings->setValue("playbackSpeed", playbackSpeed);

    // NOTE: this was previously done in MainWindow:onGeneralSettingsChange();
    if(fileCamera->getPlaybackSpeed() != playbackSpeed) {
        fileCamera->setPlaybackSpeed(playbackSpeed);
    }
}

// Set that playback is looped infinitely
void ImagePlaybackControlDialog::setPlaybackLoop(bool m_state) {
    // If playbackLoop was false and we are setting to true, set temp variables and disable checkboxes
    if (m_state){
        tempSyncRecordCsv = syncRecordCsv;
        syncRecordCsv = false;
        tempSyncStream = syncStream;
        syncStream = false;

        syncRecordCsvBox->setDisabled(true);
        syncRecordCsvBox->setChecked(false);
        syncStreamBox->setDisabled(true);
        syncStreamBox->setChecked(false);
    }
    // If playbackLoop was true and we are setting to false, restore sync variables
    if (!m_state){
        syncRecordCsv = tempSyncRecordCsv;
        syncStream = tempSyncStream;
        syncRecordCsvBox->setEnabled(!playImagesOn);
        syncRecordCsvBox->setChecked(syncRecordCsv);
        syncStreamBox->setEnabled(!playImagesOn);
        syncStreamBox->setChecked(syncStream);
    }
    playbackLoop = m_state;
    applicationSettings->setValue("playbackLoop", playbackLoop);

    // NOTE: this was previously done in MainWindow:onGeneralSettingsChange();
    if(fileCamera->getPlaybackLoop() != static_cast<int>(playbackLoop)) {
        fileCamera->setPlaybackLoop(playbackLoop);

    }

    this->update();
}

bool ImagePlaybackControlDialog::getSyncRecordCsv() {
    return syncRecordCsv;
}

bool ImagePlaybackControlDialog::getSyncStream() {
    return syncStream;
}

bool ImagePlaybackControlDialog::getPlayImagesOn()
{
    return playImagesOn;
}

// Set whether we want csv recording to start/pause on playback start/(pause/stop)
void ImagePlaybackControlDialog::setSyncRecordCsv(bool m_state) {
    syncRecordCsv = m_state;
    applicationSettings->setValue("syncRecordCsv", syncRecordCsv);

    // TODO
}

// Set whether we want streaming to start/pause on playback start/(pause/stop)
void ImagePlaybackControlDialog::setSyncStream(bool m_state) {
    syncStream = m_state;
    applicationSettings->setValue("syncStream", syncStream);

    // TODO
}

void ImagePlaybackControlDialog::onCameraPlaybackChanged() {
    if(!paused) {
        emit onPlaybackPauseInitiated();
    } else {
        emit onPlaybackStartInitiated();
    }
}

void ImagePlaybackControlDialog::onPlaybackPauseApproved() {
    emit onPlaybackSafelyPaused();

//        qDebug()<<"Pausing FileCamera Click";
    fileCamera->pause();

    const QIcon icon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/media-playback-start.svg"), applicationSettings);
    startPauseButton->setIcon(icon);

    startPauseButton->setEnabled(true);
    stopButton->setEnabled(true);
    enableWidgets();
    paused = true;
    playImagesOn = false;
    //playImagesOn = false;

//        qDebug() << "Playback paused";

    this->update();
}

void ImagePlaybackControlDialog::onPlaybackStartApproved() {
    emit onPlaybackSafelyStarted();

//        qDebug()<<"Starting FileCamera Click";
    fileCamera->start();

    const QIcon icon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/media-playback-pause.svg"), applicationSettings);
    startPauseButton->setIcon(icon);
    playImagesOn = true;
    finished = false;
    paused = false;
    disableWidgets();

    this->update();
}

void ImagePlaybackControlDialog::onFrameSelected(int frameNumber){
//    qDebug() << "Frame number arrived: " << frameNumber;
    selectedFrameVal = frameNumber;
    if (!playImagesOn){

        slider->blockSignals(true);
        int gg = floor(99*((selectedFrameVal - 1)/(float)(numImagesTotal - 1)));
        //slider->setColorTickPos((selectedFrameVal)/(float)numImagesTotal);
//        qDebug() << "Set slider to: " << gg;
        slider->setValue( gg );
        slider->blockSignals(false);
        fileCamera->seekToFrame(selectedFrameVal -1);
        updateInfoInternal(selectedFrameVal - 1);
        emit stillImageChange(selectedFrameVal - 1);
    }
}

void ImagePlaybackControlDialog::enableWidgets(){
    selectedFrameBox->setReadOnly(false);
    selectedFrameBox->setDisabled(false);
    timestampVal->setReadOnly(false);
    timestampVal->setDisabled(false);
    dial->setDisabled(false);
    slider->setDisabled(false);
    infoGroup->setDisabled(false);
    loopBox->setDisabled(false);
    syncRecordCsvBox->setDisabled(playbackLoop);
    syncStreamBox->setDisabled(playbackLoop);
}

void ImagePlaybackControlDialog::disableWidgets(){
    selectedFrameBox->setReadOnly(true);
    selectedFrameBox->setDisabled(true);
    timestampVal->setReadOnly(true);
    timestampVal->setDisabled(true);
    dial->setDisabled(true);
    slider->setDisabled(true);
    infoGroup->setDisabled(true);
    loopBox->setDisabled(syncRecordCsv || syncStream);
    syncRecordCsvBox->setDisabled(true);
    syncStreamBox->setDisabled(true);
}

void ImagePlaybackControlDialog::onTimestampSelected(double frameNumber){
        if (!playImagesOn){
            selectedFrameBox->setValue(frameNumber);
        }
}

void ImagePlaybackControlDialog::onPlaybackStopApproved() {

    if (finished && endReached && fileCamera->getNumImagesTotal() == selectedFrameVal){
        const QIcon icon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/media-playback-start.svg"), applicationSettings);
        startPauseButton->setIcon(icon);

        startPauseButton->setEnabled(true);
        stopButton->setEnabled(true);
        enableWidgets();

        lastTimestamp = 0;
        startTimestamp = 0;
        playImagesOn = false;
        //finished = false;
        endReached = false;
        paused = true;
        lastPlayedFrame = 0;

        emit onPlaybackSafelyStopped();
    }
    else if (finished && !endReached){
        const QIcon icon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/media-playback-start.svg"), applicationSettings);
        startPauseButton->setIcon(icon);

        startPauseButton->setEnabled(true);
        stopButton->setEnabled(true);
        enableWidgets();

        lastTimestamp = 0;
        startTimestamp = 0;
        playImagesOn = false;
        paused = true;

//        qDebug() << "Playback stopped";
        //if(recEventTracker)
        //    recEventTracker->resetReplay();
        selectedFrameVal = 1;
        selectedFrameBox->setValue(selectedFrameVal);
        emit onPlaybackSafelyStopped();
    }

    this->update();
}

void ImagePlaybackControlDialog::onFinished() {
    finished = true;
//    resetState();
    emit onPlaybackStopInitiated();
}

