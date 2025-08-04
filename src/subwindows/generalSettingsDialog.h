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

//    QString getImageWriterFormat() const;
//    QString getImageWriterDataRule() const;
//    QString getDataWriterDataRule() const;
//    QString getDataWriterDataStyle() const;
    bool getMetaSnapshotsEnabled() const;
    bool getSaveOfflineEventLog() const;
    bool getAlwaysOnTop() const;

    bool getIgnoreFrameSkip() const;

private:

    QSettings *applicationSettings;

    QGroupBox *dataWriterGroup;
    QGroupBox *imageWriterGroup;
    QString imageWriterFormat;
    QString imageWriterDataRule;

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



    QWidget *formatPngCompressionWidget;
    QComboBox *formatPngCompressionBox;
    QWidget *formatJpegQualityWidget;
    QSpinBox *formatJpegQualityBox;
    QWidget *formatWebpQualityWidget;
    QSpinBox *formatWebpQualityBox;

    int imageWriterFormatPngCompression;
    int imageWriterFormatJpegQuality;
    int imageWriterFormatWebpQuality;

    QPushButton *applyButton;
    QPushButton *cancelButton;

    QComboBox *imageWriterFormatBox;
    QComboBox *imageWriterDataRuleBox;
    QSpinBox *playbackSpeedInputBox;
    QCheckBox *playbackLoopBox;

    bool metaSnapshotsEnabled;
    bool saveOfflineEventLog;
    bool alwaysOnTop;

    bool ignoreFrameSkip;

    QString dataWriterDelimiter;
    QString dataWriterDataStyle;
    QString dataWriterDataRule;
    QComboBox *dataWriterDelimiterBox;
    QComboBox *dataWriterDataStyleBox;
    QComboBox *dataWriterDataRuleBox;

    int darkAdaptMode;
    QComboBox *darkAdaptBox;
    QCheckBox *metaSnapshotBox;
    QCheckBox *saveOfflineEventLogBox;
    QCheckBox *alwaysOnTopBox;

    QCheckBox *ignoreFrameSkipBox;

    void createForm();
    void saveSettings();
    void updateForm();

public slots:

    void open() override;
    void apply();
    void cancel();
    void onImageWriterFormatChange(int index);
    void onImageWriterDataRuleChange(int index);
    void readSettings();
//    void setImageWriterFormat(const QString &imageWriterFormat);
//    void setImageWriterDataRule(const QString &imageWriterDataRule);
//    void setDataWriterDataRule(const QString &dataWriterDataRule);
//    void setDataWriterDataStyle(const QString &m_dataWriterDataStyle);

    void onDataWriterDelimiterChange(int index);
    void onDataWriterDataStyleChange(int index);
    void onDataWriterDataRuleChange(int index);
    void onDarkAdaptChange(int index);
    void setMetaSnapshotEnabled(int m_state);
    void setSaveOfflineEventLog(int m_state);
    void setAlwaysOnTop(int m_state);

    void setIgnoreFrameSkip(int m_state);

    void onImageWriterFormatPngCompressionChange(int index);
    void onImageWriterFormatJpegQualityChange(int value);
    void onImageWriterFormatWebpQualityChange(int value);

    void setLimitationsWhileImageWriting(bool state);
    void setLimitationsWhileDataWriting(bool state);
    void onSettingsChangedElsewhere();

signals:
    void onSettingsChange();
    void onSettingsChangeNeedingRestart();

};
