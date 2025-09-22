#pragma once

/**
    @author Moritz Lode, Gabor Benyei, Attila Boncser
*/

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>
#include <QtCore/QSettings>

/**
    General settings window (dialog) containing all general settings of the pupilext software
*/

class GeneralSettingsDialog : public QDialog {
    Q_OBJECT

public:

    explicit GeneralSettingsDialog(QWidget *parent = nullptr);
    ~GeneralSettingsDialog() override;

    bool getAlwaysOnTop() const;
    bool getAdminWarning() const;

    bool getIgnoreFrameSkip() const;

private:

    QSettings *applicationSettings;

    QGroupBox *dataWriterGroup;
    QGroupBox *iwImageSeqGroup;
    QGroupBox *iwVideoGroup;
    QString iwImageSeqFormat;
    QString iwVideoCodec;
    QString iwImageSeqDataRule;

    QGroupBox *gazeTrackingGroup;
    // Determine ground truth by aggregating data before acceptance
    //  within temporal window of (max 10 sec) = gtw
    //  aggregated to centroid using
    //      - Arithmetic mean by x and y
    //      - Median by x and y
    //      - Bounding box center (xmin + xmax)/2, (ymin + ymax)/2
    //
    // Calibration by
    //  participant calibration (best)
    //  experimenter calibration
    //  automatic calibration
    //      - acceptance radius
    //      - time window average (has to be >=gtw)



    QWidget *iwImageSeqPngCompressionWidget;
    QComboBox *iwImageSeqPngCompressionBox;
    QWidget *iwImageSeqJpegQualityWidget;
    QSpinBox *iwImageSeqJpegQualityBox;
    QWidget *formatWebpQualityWidget;
    QSpinBox *iwImageSeqWebpQualityBox;

    // TODO: other settings for other codecs ?
    //QWidget *iwVideoPngCompressionWidget;
    //QComboBox *iwVideoPngCompressionBox;
    QWidget *iwVideoMJpegQualityWidget;
    QSpinBox *iwVideoMJpegQualityBox;
    QWidget *iwVideoMPEG4QualityWidget;
    QSpinBox *iwVideoMPEG4QualityBox;
    QWidget *iwVideoProResQualityWidget;
    QSpinBox *iwVideoProResQualityBox;
    QWidget *iwVideoFFV1CoderWidget;
    QComboBox *iwVideoFFV1CoderBox;
    QWidget *iwVideoFFV1ContextWidget;
    QComboBox *iwVideoFFV1ContextBox;

    int iwImageSeqPngCompression;
    int iwImageSeqJpegQuality;
    int iwImageSeqWebpQuality;

    //int iwVideoPngCompression;
    int iwVideoMJpegQuality;
    int iwVideoMPEG4Quality;
    int iwVideoProResQuality;
    int iwVideoFFV1Coder;
    int iwVideoFFV1Context;

    QPushButton *applyButton;
    QPushButton *cancelButton;

    QComboBox *iwImageSeqFormatBox;
    QComboBox *iwVideoCodecBox;
    QComboBox *iwImageSeqDataRuleBox;
    QSpinBox *playbackSpeedInputBox;
    QCheckBox *playbackLoopBox;

    bool alwaysOnTop;
    bool adminWarning;

    bool ignoreFrameSkip;

    QString dataWriterDelimiter;
    QString dataWriterDataStyle;
    QString dataWriterDataRule;
    QComboBox *dataWriterDelimiterBox;
    QComboBox *dataWriterDataStyleBox;
    QComboBox *dataWriterDataRuleBox;

    int darkAdaptMode;
    QComboBox *darkAdaptBox;
    QCheckBox *alwaysOnTopBox;
    QCheckBox *adminWarningBox;

    QCheckBox *ignoreFrameSkipBox;

    void createForm();
    void saveSettings();
    void updateForm();

public slots:

    void open() override;
    void apply();
    void cancel();
    void onImageWriterImageSequenceFormatChange(int index);
    void onImageWriterDataRuleChange(int index);
    void readSettings();

    void onDataWriterDelimiterChange(int index);
    void onDataWriterDataStyleChange(int index);
    void onDataWriterDataRuleChange(int index);
    void onDarkAdaptChange(int index);
    void setAlwaysOnTop(int m_state);
    void setAdminWarning(int m_state);

    void setIgnoreFrameSkip(int m_state);

    void onImageWriterImageSequencePngCompressionChange(int index);
    void onImageWriterImageSequenceJpegQualityChange(int value);
    void onImageWriterImageSequenceWebpQualityChange(int value);

    void onImageWriterVideoCodecChange(int index);

    //void onImageWriterVideoPngCompressionChange(int index);
    void onImageWriterVideoMJpegQualityChange(int value);
    void onImageWriterVideoMPEG4QualityChange(int value);
    void onImageWriterVideoProResQualityChange(int value);
    void onImageWriterVideoFFV1CoderChange(int index);
    void onImageWriterVideoFFV1ContextChange(int index);

    void setLimitationsWhileImageWriting(bool state);
    void setLimitationsWhileDataWriting(bool state);
    void onSettingsChangedElsewhere();

signals:
    void onSettingsChange();
    void onSettingsChangeNeedingRestart();

};
