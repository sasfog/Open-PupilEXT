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

    int getPlaybackSpeed() const;
    bool getPlaybackLoop() const;

    QString getImageWriterFormat() const;
    QString getImageWriterDataRule() const;

    QString getDataWriterDataRule() const;
    QString getDataWriterDataStyle() const;
    bool getMetaSnapshotsEnabled() const;
    bool getSaveOfflineEventLog() const;
    bool getAlwaysOnTop() const;

private:

    QSettings *applicationSettings;

    QGroupBox *dataWriterGroup;
    QGroupBox *imageWriterGroup;
    QString imageWriterFormat;
    QString imageWriterDataRule;

    QPushButton *applyButton;
    QPushButton *cancelButton;

    QComboBox *imageWriterFormatBox;
    QComboBox *imageWriterDataRuleBox;
    QSpinBox *playbackSpeedInputBox;
    QCheckBox *playbackLoopBox;

    bool metaSnapshotsEnabled;
    bool saveOfflineEventLog;
    bool alwaysOnTop;

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
    //void setPlaybackSpeed(int playbackSpeed);
    void setImageWriterFormat(const QString &imageWriterFormat);
    void setImageWriterDataRule(const QString &imageWriterDataRule);
    void setDataWriterDataRule(const QString &dataWriterDataRule);
    void setDataWriterDataStyle(const QString &m_dataWriterDataStyle);
    //void setPlaybackLoop(int m_state);

    void onDataWriterDelimiterChange(int index);
    void onDataWriterDataStyleChange(int index);
    void onDataWriterDataRuleChange(int index);
    void onDarkAdaptChange(int index);
    void setMetaSnapshotEnabled(int m_state);
    void setSaveOfflineEventLog(int m_state);
    void setAlwaysOnTop(int m_state);

    void setLimitationsWhileImageWriting(bool state);
    void setLimitationsWhileDataWriting(bool state);
    void onSettingsChangedElsewhere();

signals:
    void onSettingsChange();

};
