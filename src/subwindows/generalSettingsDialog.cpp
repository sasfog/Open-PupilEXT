
#include <QtWidgets>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/qformlayout.h>
#include <QtWidgets/QSpinBox>
#include <iostream>
#include "generalSettingsDialog.h"
#include "../supportFunctions.h"

// Create a settings dialog for the general software settings
// Settings are read upon creation from the QT application settings if existing
GeneralSettingsDialog::GeneralSettingsDialog(QWidget *parent) :
        QDialog(parent),
        //playbackSpeed(30),
        //iwImageSeqFormat("tiff"),
        //imageWriterDataRule("ask"),
        //dataWriterDelimiter(","),
        //dataWriterDataRule("ask"),
        applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), parent)) {

    //////this->setMinimumSize(200, 330);
    ////this->setMinimumSize(380, 580);
    //this->setMinimumSize(380, 600);
    this->setMinimumSize(730, 600);
    this->setWindowTitle("Settings");

    readSettings();
    createForm();

    updateForm();

    connect(iwImageSeqFormatBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onImageWriterImageSequenceFormatChange(int)));
    connect(iwImageSeqDataRuleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onImageWriterDataRuleChange(int)));

    connect(iwImageSeqPngCompressionBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onImageWriterImageSequencePngCompressionChange(int)));
    connect(iwImageSeqJpegQualityBox, SIGNAL(valueChanged(int)), this, SLOT(onImageWriterImageSequenceJpegQualityChange(int)));
    connect(iwImageSeqWebpQualityBox, SIGNAL(valueChanged(int)), this, SLOT(onImageWriterImageSequenceWebpQualityChange(int)));


    connect(iwVideoCodecBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onImageWriterVideoCodecChange(int)));

    //connect(iwVideoPngCompressionBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onImageWriterVideoPngCompressionChange(int)));
    connect(iwVideoMJpegQualityBox, SIGNAL(valueChanged(int)), this, SLOT(onImageWriterVideoMJpegQualityChange(int)));
    connect(iwVideoMPEG4QualityBox, SIGNAL(valueChanged(int)), this, SLOT(onImageWriterVideoMPEG4QualityChange(int)));
    connect(iwVideoProResQualityBox, SIGNAL(valueChanged(int)), this, SLOT(onImageWriterVideoProResQualityChange(int)));
    connect(iwVideoFFV1CoderBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onImageWriterVideoFFV1CoderChange(int)));
    connect(iwVideoFFV1ContextBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onImageWriterVideoFFV1ContextChange(int)));

    connect(dataWriterDelimiterBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onDataWriterDelimiterChange(int)));
    connect(dataWriterDataStyleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onDataWriterDataStyleChange(int)));
    connect(dataWriterDataRuleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onDataWriterDataRuleChange(int)));

    connect(darkAdaptBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onDarkAdaptChange(int)));
    connect(alwaysOnTopBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(setAlwaysOnTop(Qt::CheckState)));
    connect(adminWarningBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(setAdminWarning(Qt::CheckState)));

    connect(ignoreFrameSkipBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(setIgnoreFrameSkip(Qt::CheckState)));

    connect(applyButton, &QPushButton::clicked, this, &GeneralSettingsDialog::apply);
    connect(cancelButton, &QPushButton::clicked, this, &GeneralSettingsDialog::cancel);
}

// Reads the settings from the QT application setting, if the entries were found
void GeneralSettingsDialog::readSettings() {

    const QString m_imageWriterSeparateImagesFormat = applicationSettings->value("imageWriter.imageSequence.chosenFormat", "tiff").toString();
    if (!m_imageWriterSeparateImagesFormat.isEmpty()) {
        iwImageSeqFormat = m_imageWriterSeparateImagesFormat;
    }
    iwImageSeqPngCompression = applicationSettings->value("imageWriter.imageSequence.png.compression", "0").toInt();
    iwImageSeqJpegQuality = applicationSettings->value("imageWriter.imageSequence.jpeg.quality", "100").toInt();
    iwImageSeqWebpQuality = applicationSettings->value("imageWriter.imageSequence.webp.quality", "100").toInt();

    const QString m_iwVideoCodec = applicationSettings->value("imageWriter.video.chosenCodec", "AV_CODEC_ID_FFV1").toString();
    if (!m_iwVideoCodec.isEmpty()) {
        iwVideoCodec = m_iwVideoCodec;
    }
    //iwVideoPngCompression = applicationSettings->value("imageWriter.video.png.compression", "0").toInt();
    iwVideoMJpegQuality = applicationSettings->value("imageWriter.video.mjpeg.quality", "2").toInt();
    iwVideoMPEG4Quality = applicationSettings->value("imageWriter.video.mpeg4.quality", "2").toInt();
    iwVideoProResQuality = applicationSettings->value("imageWriter.video.prores.quality", "2").toInt();
    iwVideoFFV1Coder = applicationSettings->value("imageWriter.video.ffv1.coder", "1").toInt();
    iwVideoFFV1Context = applicationSettings->value("imageWriter.video.ffv1.context", "1").toInt();

    const QString m_iwDataRule = applicationSettings->value("imageWriterDataRule", "ask").toString();
    if (!m_iwDataRule.isEmpty()) {
        iwImageSeqDataRule = m_iwDataRule;
    }

    const QString m_dataWriterDelimiter = applicationSettings->value("dataWriterDelimiter", ",").toString();
    if (!m_dataWriterDelimiter.isEmpty()) {
        dataWriterDelimiter = m_dataWriterDelimiter;
    }

    const QString m_dataWriterDataStyle = applicationSettings->value("dataWriterDataStyle", "DATASTYLE_V3").toString();
    if (!m_dataWriterDataStyle.isEmpty()) {
        dataWriterDataStyle = m_dataWriterDataStyle;
    }

    const QString m_dataWriterDataRule = applicationSettings->value("dataWriterDataRule", "ask").toString();
    if (!m_dataWriterDataRule.isEmpty()) {
        dataWriterDataRule = m_dataWriterDataRule;
    }

    alwaysOnTop = SupportFunctions::readBoolFromQSettings("alwaysOnTop", false, applicationSettings);
    adminWarning = SupportFunctions::readBoolFromQSettings("adminWarning", true, applicationSettings);

    ignoreFrameSkip = SupportFunctions::readBoolFromQSettings("ignoreFrameSkipWarnings", false, applicationSettings);

    darkAdaptMode = applicationSettings->value("GUIDarkAdaptMode", "2").toInt();
    // GUIDarkAdaptMode: 0 = no, 1 = yes, 2 = let PupilEXT guess

}

void GeneralSettingsDialog::updateForm() {

    iwImageSeqFormatBox->setCurrentIndex(iwImageSeqFormatBox->findData(iwImageSeqFormat));
    //
    iwImageSeqPngCompressionBox->setCurrentIndex(iwImageSeqPngCompression);
    iwImageSeqJpegQualityBox->setValue(iwImageSeqJpegQuality);
    iwImageSeqWebpQualityBox->setValue(iwImageSeqWebpQuality);
    //
    iwImageSeqPngCompressionWidget->setVisible(iwImageSeqFormat == "png");
    iwImageSeqJpegQualityWidget->setVisible(iwImageSeqFormat == "jpeg");
    formatWebpQualityWidget->setVisible(iwImageSeqFormat == "webp");

    iwVideoCodecBox->setCurrentIndex(iwVideoCodecBox->findData(iwVideoCodec));
    //
    //iwVideoPngCompressionBox->setCurrentIndex(iwVideoPngCompression);
    iwVideoMJpegQualityBox->setValue(iwVideoMJpegQuality);
    iwVideoMPEG4QualityBox->setValue(iwVideoMPEG4Quality);
    iwVideoProResQualityBox->setValue(iwVideoProResQuality);
    iwVideoFFV1CoderBox->setCurrentIndex(iwVideoFFV1CoderBox->findData(iwVideoFFV1Coder));
    iwVideoFFV1ContextBox->setCurrentIndex(iwVideoFFV1ContextBox->findData(iwVideoFFV1Context));
    //
    //iwVideoPngCompressionWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_PNG");
    iwVideoMJpegQualityWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_MJPEG");
    iwVideoMPEG4QualityWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_MPEG4");
    iwVideoProResQualityWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_PRORES");
    iwVideoFFV1CoderWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_FFV1");
    iwVideoFFV1ContextWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_FFV1");

    if(iwImageSeqDataRule == "ask")
        iwImageSeqDataRuleBox->setCurrentIndex(0);
    else if(iwImageSeqDataRule == "append")
        iwImageSeqDataRuleBox->setCurrentIndex(1);
    else // if(iwImageSeqDataRule == "new")
        iwImageSeqDataRuleBox->setCurrentIndex(2);

    //dataWriterDelimiterBox->setCurrentText(delimiterToUse);
    if(dataWriterDelimiter == ";")
        dataWriterDelimiterBox->setCurrentIndex(1);
    else if(dataWriterDelimiter == "\t")
        dataWriterDelimiterBox->setCurrentIndex(2);
    else //if(dataWriterDelimiter == ",")
        dataWriterDelimiterBox->setCurrentIndex(0);
//    qDebug() << "Data writer delimiter read as: " << dataWriterDelimiter << "\n";

    //if(dataWriterDataStyle == "PupilEXT-0-1-1")
        dataWriterDataStyleBox->setCurrentIndex(0);
    //else // if(dataWriterDataStyle == "PupilEXT-0-1-2")
    //    dataWriterDataStyleBox->setCurrentIndex(1);

    if(dataWriterDataRule == "ask")
        dataWriterDataRuleBox->setCurrentIndex(0);
    else if(dataWriterDataRule == "append")
        dataWriterDataRuleBox->setCurrentIndex(1);
    else // if(dataWriterDataRule == "new")
        dataWriterDataRuleBox->setCurrentIndex(2);

    darkAdaptBox->setCurrentIndex(darkAdaptMode);

    alwaysOnTopBox->setChecked(alwaysOnTop);
    adminWarningBox->setChecked(adminWarning);

    ignoreFrameSkipBox->setChecked(ignoreFrameSkip);
}

// Saved the settings selected in the dialog to the QT application settings
void GeneralSettingsDialog::saveSettings() {
    applicationSettings->setValue("imageWriter.imageSequence.chosenFormat", iwImageSeqFormat);
    applicationSettings->setValue("imageWriter.imageSequence.png.compression", iwImageSeqPngCompression);
    applicationSettings->setValue("imageWriter.imageSequence.jpeg.quality", iwImageSeqJpegQuality);
    applicationSettings->setValue("imageWriter.imageSequence.webp.quality", iwImageSeqWebpQuality);
    applicationSettings->setValue("imageWriter.video.chosenCodec", iwVideoCodec);
    //applicationSettings->setValue("imageWriter.video.png.compression", iwVideoPngCompression);
    applicationSettings->setValue("imageWriter.video.mjpeg.quality", iwVideoMJpegQuality);
    applicationSettings->setValue("imageWriter.video.mpeg4.quality", iwVideoMPEG4Quality);
    applicationSettings->setValue("imageWriter.video.prores.quality", iwVideoProResQuality);
    applicationSettings->setValue("imageWriter.video.ffv1.coder", iwVideoFFV1Coder);
    applicationSettings->setValue("imageWriter.video.ffv1.context", iwVideoFFV1Context);
    applicationSettings->setValue("imageWriterDataRule", iwImageSeqDataRule);
    applicationSettings->setValue("dataWriterDelimiter", dataWriterDelimiter );
    applicationSettings->setValue("dataWriterDataStyle", dataWriterDataStyle );
    applicationSettings->setValue("dataWriterDataRule", dataWriterDataRule);
    applicationSettings->setValue("GUIDarkAdaptMode", darkAdaptMode );
    applicationSettings->setValue("alwaysOnTop", alwaysOnTop );
    applicationSettings->setValue("adminWarning", adminWarning );
    applicationSettings->setValue("ignoreFrameSkipWarnings", ignoreFrameSkip );
}

void GeneralSettingsDialog::createForm() {

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


    dataWriterGroup = new QGroupBox("General Data Output");
    QFormLayout *dataOutLayout = new QFormLayout();

    QLabel *dataWriterDelimiterLabel = new QLabel(tr("Delimiter Character"));
    dataWriterDelimiterBox = new QComboBox();
    dataWriterDelimiterBox->addItem(QString("Comma [,]"), QString(","));
    dataWriterDelimiterBox->addItem(QString("Semicolon [;]"), QString(";"));
    dataWriterDelimiterBox->addItem(QString("Tabulation"), QString("\t"));
    dataWriterDelimiterBox->setCurrentText(dataWriterDelimiter);
    dataOutLayout->addRow(dataWriterDelimiterLabel, dataWriterDelimiterBox);

    QLabel *dataWriterDataStyleLabel = new QLabel(tr("Data Style*: "));
    dataWriterDataStyleBox = new QComboBox();
    //dataWriterDataStyleBox->addItem(QString("PupilEXT v0.1.1"), QString("PupilEXT-0-1-1")); // TODO: remove,
    dataWriterDataStyleBox->addItem(QString("v3 (PupilEXT v0.1.3)"), QString("DATASTYLE_V3"));
    dataWriterDataStyleBox->setCurrentText(dataWriterDataStyle);
    dataWriterDataStyleBox->setEnabled(false); // TODO: enable again if another style gets added
    dataOutLayout->addRow(dataWriterDataStyleLabel, dataWriterDataStyleBox);
    QLabel *dataWriterDataStyleWarnLabel = new QLabel(tr("*Older version will not save trial numbering."));
    SupportFunctions::setSmallerLabelFontSize(dataWriterDataStyleWarnLabel);
    dataWriterDataStyleWarnLabel->setAlignment(Qt::AlignRight);
    dataOutLayout->addRow(dataWriterDataStyleWarnLabel);


    QLabel *dataWriterDataRuleLabel = new QLabel(tr("Action when output recording already exists:"));
    dataOutLayout->addRow(dataWriterDataRuleLabel);

    dataWriterDataRuleBox = new QComboBox();
    dataWriterDataRuleBox->addItem(QString("Ask every time"), QString("ask"));
    dataWriterDataRuleBox->addItem(QString("Append to found recording"), QString("append"));
    dataWriterDataRuleBox->addItem(QString("Keep existing and save new one too"), QString("new"));
    dataWriterDataRuleBox->setCurrentText(dataWriterDataRule);
    dataOutLayout->addRow(dataWriterDataRuleBox);

    dataWriterGroup->setLayout(dataOutLayout);
    mainLayoutInnerCol1->addWidget(dataWriterGroup);



    iwImageSeqGroup = new QGroupBox("Image Writer (Directory or Zip archive)");
    QFormLayout *iwImageSeqLayout = new QFormLayout();

    QLabel *iwImageSeqFormatLabel = new QLabel(tr("Image Format**"));
    iwImageSeqFormatBox = new QComboBox();
    iwImageSeqFormatBox->addItem(QString("tiff [small files]"), QString("tiff"));
    iwImageSeqFormatBox->addItem(QString("png [configurable]"), QString("png"));
    iwImageSeqFormatBox->addItem(QString("bmp [large files]"), QString("bmp"));
    iwImageSeqFormatBox->addItem(QString("jpeg [configurable]"), QString("jpeg"));
    iwImageSeqFormatBox->addItem(QString("webp [configurable]"), QString("webp"));
    iwImageSeqFormatBox->addItem(QString("pgm"), QString("pgm"));
    //int hahaha = iwImageSeqFormatBox->findData(iwImageSeqFormat);
    iwImageSeqFormatBox->setCurrentIndex(iwImageSeqFormatBox->findData(iwImageSeqFormat));
    iwImageSeqLayout->addRow(iwImageSeqFormatLabel, iwImageSeqFormatBox);

    QLabel *formatNoteLabel = new QLabel(tr("**Please consider the file size vs. CPU load tradeoff!\nAlso, lossy formats are not recommended, unless at very high resolution."));
    SupportFunctions::setSmallerLabelFontSize(formatNoteLabel);
    iwImageSeqLayout->addRow(formatNoteLabel);

    iwImageSeqPngCompressionWidget = new QWidget();
    QHBoxLayout *iwImageSeqPngCompressionLayout = new QHBoxLayout();
    iwImageSeqPngCompressionLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *iwImageSeqPngCompressionLabel = new QLabel(tr("PNG compression level:"));
    iwImageSeqPngCompressionBox = new QComboBox();
    iwImageSeqPngCompressionBox->addItem(QString("0 (large files, fast)"), 0);
    iwImageSeqPngCompressionBox->addItem(QString("1"), 1);
    iwImageSeqPngCompressionBox->addItem(QString("2"), 2);
    iwImageSeqPngCompressionBox->addItem(QString("3"), 3);
    iwImageSeqPngCompressionBox->addItem(QString("4"), 4);
    iwImageSeqPngCompressionBox->addItem(QString("5"), 5);
    iwImageSeqPngCompressionBox->addItem(QString("6"), 6);
    iwImageSeqPngCompressionBox->addItem(QString("7"), 7);
    iwImageSeqPngCompressionBox->addItem(QString("8"), 8);
    iwImageSeqPngCompressionBox->addItem(QString("9 (small files, slow)"), 9);
    iwImageSeqPngCompressionBox->setCurrentIndex(iwImageSeqPngCompressionBox->findData(iwImageSeqPngCompression));
    iwImageSeqPngCompressionLayout->addWidget(iwImageSeqPngCompressionLabel);
    iwImageSeqPngCompressionLayout->addWidget(iwImageSeqPngCompressionBox);
    iwImageSeqPngCompressionWidget->setLayout(iwImageSeqPngCompressionLayout);
    iwImageSeqLayout->addRow(iwImageSeqPngCompressionWidget);

    iwImageSeqJpegQualityWidget = new QWidget();
    QHBoxLayout *iwImageSeqJpegQualityLayout = new QHBoxLayout();
    iwImageSeqJpegQualityLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *iwImageSeqJpegQualityLabel = new QLabel(tr("JPEG quality:"));
    iwImageSeqJpegQualityBox = new QSpinBox();
    iwImageSeqJpegQualityBox->setMinimum(50);
    iwImageSeqJpegQualityBox->setMaximum(100);
    iwImageSeqJpegQualityBox->setSingleStep(1);
    iwImageSeqJpegQualityBox->setValue(iwImageSeqJpegQuality);
    iwImageSeqJpegQualityLayout->addWidget(iwImageSeqJpegQualityLabel);
    iwImageSeqJpegQualityLayout->addWidget(iwImageSeqJpegQualityBox);
    iwImageSeqJpegQualityWidget->setLayout(iwImageSeqJpegQualityLayout);
    iwImageSeqLayout->addRow(iwImageSeqJpegQualityWidget);

    formatWebpQualityWidget = new QWidget();
    QHBoxLayout *iwImageSeqWebpQualityLayout = new QHBoxLayout();
    iwImageSeqWebpQualityLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *iwImageSeqWebpQualityLabel = new QLabel(tr("WEBP quality:"));
    iwImageSeqWebpQualityBox = new QSpinBox();
    iwImageSeqWebpQualityBox->setMinimum(50);
    iwImageSeqWebpQualityBox->setMaximum(100);
    iwImageSeqWebpQualityBox->setSingleStep(1);
    iwImageSeqWebpQualityBox->setValue(iwImageSeqWebpQuality);
    iwImageSeqWebpQualityLayout->addWidget(iwImageSeqWebpQualityLabel);
    iwImageSeqWebpQualityLayout->addWidget(iwImageSeqWebpQualityBox);
    formatWebpQualityWidget->setLayout(iwImageSeqWebpQualityLayout);
    iwImageSeqLayout->addRow(formatWebpQualityWidget);

    QLabel *iwImageSeqDataRuleLabel = new QLabel(tr("Action when output recording already exists:"));
    iwImageSeqLayout->addRow(iwImageSeqDataRuleLabel);

    iwImageSeqDataRuleBox = new QComboBox();
    iwImageSeqDataRuleBox->addItem(QString("Ask every time"), QString("ask"));
    iwImageSeqDataRuleBox->addItem(QString("Append to found recording"), QString("append"));
    iwImageSeqDataRuleBox->addItem(QString("Keep existing and save new one too"), QString("new"));
    iwImageSeqDataRuleBox->setCurrentText(iwImageSeqDataRule);
    iwImageSeqLayout->addRow(iwImageSeqDataRuleBox);

    iwImageSeqGroup->setLayout(iwImageSeqLayout);
    mainLayoutInnerCol1->addWidget(iwImageSeqGroup);


    ////////

    iwVideoGroup = new QGroupBox("Image Writer (Video file)");
    QFormLayout *iwVideoLayout = new QFormLayout();

    // TODO: check which codec is available (?)
    // AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    //if (!codec) throw std::runtime_error("PNG encoder not found");

    QLabel *iwVideoCodecLabel = new QLabel(tr("Video Codec**"));
    iwVideoCodecBox = new QComboBox();
    iwVideoCodecBox->addItem(QString("FFV1 [lossless]"), QString("AV_CODEC_ID_FFV1"));
    //iwVideoCodecBox->addItem(QString("PNG sequence [lossless]"), QString("AV_CODEC_ID_PNG")); // avcodec_open2() always fails with -22
    iwVideoCodecBox->addItem(QString("MJPEG [lossy]"), QString("AV_CODEC_ID_MJPEG"));
    iwVideoCodecBox->addItem(QString("MPEG-4 Part 2 [lossy]"), QString("AV_CODEC_ID_MPEG4"));
    iwVideoCodecBox->addItem(QString("ProRes [lossy]"), QString("AV_CODEC_ID_PRORES"));
    iwVideoCodecBox->setCurrentIndex(iwVideoCodecBox->findData(iwVideoCodec));
    iwVideoLayout->addRow(iwVideoCodecLabel, iwVideoCodecBox);

    QLabel *formatNoteLabel2 = new QLabel(tr("**Lossy formats are not recommended, unless at very high resolution."));
    SupportFunctions::setSmallerLabelFontSize(formatNoteLabel2);
    iwVideoLayout->addRow(formatNoteLabel2);

    //iwVideoPngCompressionWidget = new QWidget();
    //QHBoxLayout *iwVideoPngCompressionLayout = new QHBoxLayout();
    //iwVideoPngCompressionLayout->setContentsMargins(0, 0, 0, 0);
    //QLabel *iwVideoPngCompressionLabel = new QLabel(tr("PNG compression level:"));
    //iwVideoPngCompressionBox = new QComboBox();
    //iwVideoPngCompressionBox->addItem(QString("0 (large files, fast)"), 0);
    //iwVideoPngCompressionBox->addItem(QString("1"), 1);
    //iwVideoPngCompressionBox->addItem(QString("2"), 2);
    //iwVideoPngCompressionBox->addItem(QString("3"), 3);
    //iwVideoPngCompressionBox->addItem(QString("4"), 4);
    //iwVideoPngCompressionBox->addItem(QString("5"), 5);
    //iwVideoPngCompressionBox->addItem(QString("6"), 6);
    //iwVideoPngCompressionBox->addItem(QString("7"), 7);
    //iwVideoPngCompressionBox->addItem(QString("8"), 8);
    //iwVideoPngCompressionBox->addItem(QString("9 (small files, slow)"), 9);
    //iwVideoPngCompressionBox->setCurrentIndex(iwVideoPngCompressionBox->findData(iwVideoPngCompression));
    //iwVideoPngCompressionLayout->addWidget(iwVideoPngCompressionLabel);
    //iwVideoPngCompressionLayout->addWidget(iwVideoPngCompressionBox);
    //iwVideoPngCompressionWidget->setLayout(iwVideoPngCompressionLayout);
    //iwVideoLayout->addRow(iwVideoPngCompressionWidget);

    iwVideoFFV1CoderWidget = new QWidget();
    QHBoxLayout *iwVideoFFV1CoderLayout = new QHBoxLayout();
    iwVideoFFV1CoderLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *videoFFV1CoderLabel = new QLabel(tr("FFV1 coder:"));
    iwVideoFFV1CoderBox = new QComboBox();
    iwVideoFFV1CoderBox->addItem(QString("Golomb-Rice (faster, larger file)"), 0);
    iwVideoFFV1CoderBox->addItem(QString("Range coder (slower, smaller file)"), 1);
    iwVideoFFV1CoderBox->setCurrentIndex(iwVideoFFV1CoderBox->findData(iwVideoFFV1Coder));
    iwVideoFFV1CoderLayout->addWidget(videoFFV1CoderLabel);
    iwVideoFFV1CoderLayout->addWidget(iwVideoFFV1CoderBox);
    iwVideoFFV1CoderWidget->setLayout(iwVideoFFV1CoderLayout);
    iwVideoLayout->addRow(iwVideoFFV1CoderWidget);

    iwVideoFFV1ContextWidget = new QWidget();
    QHBoxLayout *iwVideoFFV1ContextLayout = new QHBoxLayout();
    iwVideoFFV1ContextLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *videoFFV1ContextLabel = new QLabel(tr("FFV1 context modeling:"));
    iwVideoFFV1ContextBox = new QComboBox();
    iwVideoFFV1ContextBox->addItem(QString("Off (faster, larger)"), 0);
    iwVideoFFV1ContextBox->addItem(QString("On (slower, smaller)"), 1);
    iwVideoFFV1ContextBox->setCurrentIndex(iwVideoFFV1ContextBox->findData(iwVideoFFV1Context));
    iwVideoFFV1ContextLayout->addWidget(videoFFV1ContextLabel);
    iwVideoFFV1ContextLayout->addWidget(iwVideoFFV1ContextBox);
    iwVideoFFV1ContextWidget->setLayout(iwVideoFFV1ContextLayout);
    iwVideoLayout->addRow(iwVideoFFV1ContextWidget);

    iwVideoMJpegQualityWidget = new QWidget();
    QHBoxLayout *iwVideoMJpegQualityLayout = new QHBoxLayout();
    iwVideoMJpegQualityLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *videoMJpegQualityLabel = new QLabel(tr("MJPEG quality (2=highest, 31=lowest):"));
    iwVideoMJpegQualityBox = new QSpinBox();
    iwVideoMJpegQualityBox->setMinimum(2);
    iwVideoMJpegQualityBox->setMaximum(31);
    iwVideoMJpegQualityBox->setSingleStep(1);
    iwVideoMJpegQualityBox->setValue(iwVideoMJpegQuality);
    iwVideoMJpegQualityLayout->addWidget(videoMJpegQualityLabel);
    iwVideoMJpegQualityLayout->addWidget(iwVideoMJpegQualityBox);
    iwVideoMJpegQualityWidget->setLayout(iwVideoMJpegQualityLayout);
    iwVideoLayout->addRow(iwVideoMJpegQualityWidget);

    iwVideoMPEG4QualityWidget = new QWidget();
    QHBoxLayout *iwVideoMPEG4QualityLayout = new QHBoxLayout();
    iwVideoMPEG4QualityLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *videoMPEG4QualityLabel = new QLabel(tr("MPEG4 quality (2=highest, 31=lowest):"));
    iwVideoMPEG4QualityBox = new QSpinBox();
    iwVideoMPEG4QualityBox->setMinimum(2);
    iwVideoMPEG4QualityBox->setMaximum(31);
    iwVideoMPEG4QualityBox->setSingleStep(1);
    iwVideoMPEG4QualityBox->setValue(iwVideoMPEG4Quality);
    iwVideoMPEG4QualityLayout->addWidget(videoMPEG4QualityLabel);
    iwVideoMPEG4QualityLayout->addWidget(iwVideoMPEG4QualityBox);
    iwVideoMPEG4QualityWidget->setLayout(iwVideoMPEG4QualityLayout);
    iwVideoLayout->addRow(iwVideoMPEG4QualityWidget);

    iwVideoProResQualityWidget = new QWidget();
    QHBoxLayout *iwVideoProResQualityLayout = new QHBoxLayout();
    iwVideoProResQualityLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *videoProResQualityLabel = new QLabel(tr("ProRes quality (2=highest, 31=lowest):"));
    iwVideoProResQualityBox = new QSpinBox();
    iwVideoProResQualityBox->setMinimum(2);
    iwVideoProResQualityBox->setMaximum(31);
    iwVideoProResQualityBox->setSingleStep(1);
    iwVideoProResQualityBox->setValue(iwVideoProResQuality);
    iwVideoProResQualityLayout->addWidget(videoProResQualityLabel);
    iwVideoProResQualityLayout->addWidget(iwVideoProResQualityBox);
    iwVideoProResQualityWidget->setLayout(iwVideoProResQualityLayout);
    iwVideoLayout->addRow(iwVideoProResQualityWidget);

    QLabel *iwVideoDataRuleLabel = new QLabel(tr("Action when output recording already exists:"));
    iwVideoLayout->addRow(iwVideoDataRuleLabel);

    QComboBox *iwVideoDataRuleBox = new QComboBox();
    //iwVideoDataRuleBox->addItem(QString("Ask every time"), QString("ask"));
    //iwVideoDataRuleBox->addItem(QString("Append to found recording"), QString("append"));
    iwVideoDataRuleBox->addItem(QString("Keep existing and save new one too"), QString("new"));
    //iwVideoDataRuleBox->setCurrentText(iwImageSeqDataRule);
    iwVideoDataRuleBox->setEnabled(false);
    iwVideoLayout->addRow(iwVideoDataRuleBox);

    iwVideoGroup->setLayout(iwVideoLayout);
#ifdef QT_DEBUG
    mainLayoutInnerCol1->addWidget(iwVideoGroup);
#endif

    /////////

    // GB NOTE: removed playback speed and playback loop settings, as these are yet in ImagePlaybackSettingsDialog

    QGroupBox *appearanceGroup = new QGroupBox("Appearance");
    QFormLayout *appearanceLayout = new QFormLayout();

    QLabel *darkAdaptLabel = new QLabel(tr("GUI dark mode (needs restart):"));

    darkAdaptBox = new QComboBox();
    darkAdaptBox->addItem(QString("Light"));
    darkAdaptBox->addItem(QString("Dark"));
    darkAdaptBox->addItem(QString("Auto-detect"));
    darkAdaptBox->setCurrentIndex(darkAdaptMode);
    appearanceLayout->addRow(darkAdaptLabel, darkAdaptBox);

    alwaysOnTopBox = new QCheckBox("Keep application always on top (needs restart)");
    alwaysOnTopBox->setChecked(getAlwaysOnTop());
    appearanceLayout->addRow(alwaysOnTopBox);

    adminWarningBox = new QCheckBox("Show warning on startup if admin privileges missing");
    adminWarningBox->setChecked(getAdminWarning());
    appearanceLayout->addRow(adminWarningBox);

    appearanceGroup->setLayout(appearanceLayout);
    mainLayoutInnerCol2->addWidget(appearanceGroup);


    QGroupBox *cameraInterfacingGroup = new QGroupBox("Camera Interfacing");
    QFormLayout *cameraInterfacingLayout = new QFormLayout();

    ignoreFrameSkipBox = new QCheckBox("Ignore frame skip warnings");
    ignoreFrameSkipBox->setChecked(getIgnoreFrameSkip());
    cameraInterfacingLayout->addRow(ignoreFrameSkipBox);

    cameraInterfacingGroup->setLayout(cameraInterfacingLayout);
    mainLayoutInnerCol2->addWidget(cameraInterfacingGroup);


    QHBoxLayout *buttonsLayout = new QHBoxLayout();

    applyButton = new QPushButton(tr("Apply and Close"));
    cancelButton = new QPushButton(tr("Cancel"));

    buttonsLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding));

    buttonsLayout->addWidget(applyButton);
    buttonsLayout->addWidget(cancelButton);

    mainLayoutInner->addLayout(mainLayoutInnerCol1);
    mainLayoutInner->addLayout(mainLayoutInnerCol2);
    mainLayout->addLayout(mainLayoutInner);
    mainLayout->addLayout(buttonsLayout);

    setLayout(mainLayout);
}

void GeneralSettingsDialog::setLimitationsWhileImageWriting(bool state) {
    readSettings();
    updateForm();
    iwImageSeqGroup->setDisabled(state);
}

void GeneralSettingsDialog::setLimitationsWhileDataWriting(bool state) {
    readSettings();
    updateForm();
    dataWriterGroup->setDisabled(state);
}

void GeneralSettingsDialog::open() {
    onSettingsChangedElsewhere();
}

void GeneralSettingsDialog::onSettingsChangedElsewhere() {
    readSettings();
    updateForm();
}

// On apply button click
// Save the settings, send signal that settings have changed
void GeneralSettingsDialog::apply() {

    bool alwaysOnTopBeforeSave = SupportFunctions::readBoolFromQSettings("alwaysOnTop", false, applicationSettings);
    int darkAdaptModeBeforeSave = applicationSettings->value("GUIDarkAdaptMode", "2").toInt();

    saveSettings();
    emit onSettingsChange();
    
    // this->parentWidget()->repaint(); // todo: maybe do this via receiving onSettingChange from mainwindow?

    // If there is any settings change that may need application restart to take effect,
    // tell MainWindow to offer the user a restart in a dialog
    if((alwaysOnTopBeforeSave != alwaysOnTop) || (darkAdaptModeBeforeSave != darkAdaptMode)) {
        emit onSettingsChangeNeedingRestart();
    }

    close();
}

// On cancel button click and discards the settings and close the dialog
// Reset the settings so on the next dialog open the saved settings will be loaded again
void GeneralSettingsDialog::cancel() {

    readSettings();
    updateForm();

    close();
}

bool GeneralSettingsDialog::getAlwaysOnTop() const {
    return alwaysOnTop;
}
bool GeneralSettingsDialog::getAdminWarning() const {
    return adminWarning;
}
bool GeneralSettingsDialog::getIgnoreFrameSkip() const {
    return ignoreFrameSkip;
}

void GeneralSettingsDialog::setAlwaysOnTop(Qt::CheckState m_state) {
    alwaysOnTop = (bool) m_state;
}
void GeneralSettingsDialog::setAdminWarning(Qt::CheckState m_state) {
    adminWarning = (bool) m_state;
}
void GeneralSettingsDialog::setIgnoreFrameSkip(Qt::CheckState m_state) {
    ignoreFrameSkip = (bool) m_state;
}

void GeneralSettingsDialog::onImageWriterImageSequenceFormatChange(int index) {
    iwImageSeqFormat = iwImageSeqFormatBox->itemData(index).toString();

    iwImageSeqPngCompressionWidget->setVisible(iwImageSeqFormat == "png");
    iwImageSeqJpegQualityWidget->setVisible(iwImageSeqFormat == "jpeg");
    formatWebpQualityWidget->setVisible(iwImageSeqFormat == "webp");
}

void GeneralSettingsDialog::onImageWriterVideoCodecChange(int index) {
    iwVideoCodec = iwVideoCodecBox->itemData(index).toString();

    //iwVideoPngCompressionWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_PNG");
    iwVideoMJpegQualityWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_MJPEG");
    iwVideoMPEG4QualityWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_MPEG4");
    iwVideoProResQualityWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_PRORES");
    iwVideoFFV1CoderWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_FFV1");
    iwVideoFFV1ContextWidget->setVisible(iwVideoCodec == "AV_CODEC_ID_FFV1");
}

void GeneralSettingsDialog::onImageWriterDataRuleChange(int index) {
    iwImageSeqDataRule = iwImageSeqDataRuleBox->itemData(index).toString();
}

void GeneralSettingsDialog::onImageWriterImageSequencePngCompressionChange(int index) {
    iwImageSeqPngCompression = iwImageSeqPngCompressionBox->itemData(index).toInt();
}

void GeneralSettingsDialog::onImageWriterImageSequenceJpegQualityChange(int value) {
    iwImageSeqJpegQuality = value;
}

void GeneralSettingsDialog::onImageWriterImageSequenceWebpQualityChange(int value) {
    iwImageSeqWebpQuality = value;
}

//void GeneralSettingsDialog::onImageWriterVideoPngCompressionChange(int index) {
//    iwVideoPngCompression = iwVideoPngCompressionBox->itemData(index).toInt();
//}

void GeneralSettingsDialog::onImageWriterVideoMJpegQualityChange(int value) {
    iwVideoMJpegQuality = value;
}

void GeneralSettingsDialog::onImageWriterVideoMPEG4QualityChange(int value) {
    iwVideoMPEG4Quality = value;
}

void GeneralSettingsDialog::onImageWriterVideoProResQualityChange(int value) {
    iwVideoProResQuality = value;
}

void GeneralSettingsDialog::onImageWriterVideoFFV1CoderChange(int index) {
    iwVideoFFV1Coder = iwVideoFFV1CoderBox->itemData(index).toInt();
}

void GeneralSettingsDialog::onImageWriterVideoFFV1ContextChange(int index) {
    iwVideoFFV1Context = iwVideoFFV1ContextBox->itemData(index).toInt();
}

// Event handler on the change of the combobox selection in the dialog
void GeneralSettingsDialog::onDataWriterDelimiterChange(int index) {
    dataWriterDelimiter = dataWriterDelimiterBox->itemData(index).toString();
}

void GeneralSettingsDialog::onDataWriterDataStyleChange(int index) {
    dataWriterDataStyle = dataWriterDataStyleBox->itemData(index).toString();
}

void GeneralSettingsDialog::onDataWriterDataRuleChange(int index) {
    dataWriterDataRule = dataWriterDataRuleBox->itemData(index).toString();
}

// Event handler on the change of the combobox selection in the dialog
void GeneralSettingsDialog::onDarkAdaptChange(int index) {
    darkAdaptMode = index;
}

GeneralSettingsDialog::~GeneralSettingsDialog() = default;
