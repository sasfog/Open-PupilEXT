
#ifndef PUPILEXT_PUPILMETHODSETTING_H
#define PUPILEXT_PUPILMETHODSETTING_H

/**
    @authors Moritz Lode, Gábor Bényei
*/

#include <QtWidgets/QWidget>




/**
    Abstract class representing the pupil detection algorithm's individual parameters, this widget will be integrated into the pupil detection settings configuration windows
*/
class PupilMethodSetting : public QWidget {
    Q_OBJECT

public:

    QWidget *infoBox;
    QSettings *applicationSettings;

    enum class Settings {
            DEFAULT,
            ROI_0_3_OPTIMIZED,
            ROI_0_6_OPTIMIZED,
            FULL_IMAGE_OPTIMIZED,
            AUTOMATIC_PARAMETRIZATION,
            CUSTOM
    };
    Q_ENUM(Settings);
    void setSettings(Settings settings);
    Settings settings() const;


    // GB: a
    explicit PupilMethodSetting(QString settingsConfigParametersName, QString settingsConfigParametersIndexName, QWidget *parent=0) : 
             settingsConfigParametersName(settingsConfigParametersName), settingsConfigParametersIndexName(settingsConfigParametersIndexName), QWidget(parent),
             applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), parent)) {

        infoBox = new QWidget();
    }

    explicit PupilMethodSetting(QWidget *parent=0) : QWidget(parent),
             applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), parent)) {

        infoBox = new QWidget();
    }

    QMap<QString, Settings> settingsMap{
            { "Default", Settings::DEFAULT },
            { "ROI 0.3 Optimized", Settings::ROI_0_3_OPTIMIZED },
            { "ROI 0.6 Optimized", Settings::ROI_0_6_OPTIMIZED },
            { "Full Image Optimized", Settings::FULL_IMAGE_OPTIMIZED },
            { "Automatic Parametrization", Settings::AUTOMATIC_PARAMETRIZATION } 
    };

    //virtual void addSecondary(PupilDetectionMethod *method) = 0;

        QMap<Settings, QList<float>> getParameter() {
        return configParameters;
    }

    void setParameter(QMap<Settings, QList<float>> params) {
        if(defaultParameters.size() == params.size())
            configParameters = params;
    }

    void reset() {
        configParameters = defaultParameters;
    }

    // GB modified begin
    virtual bool isAutoParamEnabled() {
        return (parameterConfigs->currentText()=="Automatic Parametrization");
    }
    // GB modified end

protected:
    QMap<Settings, QList<float>> defaultParameters;
    QMap<Settings, QList<float>> configParameters;
    Settings configIndex;
    QPushButton *resetButton;
    QComboBox *parameterConfigs;
    QPushButton *fileButton;

    QString settingsConfigParametersName;
    QString settingsConfigParametersIndexName;

public slots:

    virtual void loadSettings() {
        configParameters = applicationSettings->value(settingsConfigParametersName, QVariant::fromValue(configParameters)).value<QMap<Settings, QList<float>>>();
        configIndex = applicationSettings->value(settingsConfigParametersIndexName, QVariant::fromValue(PupilMethodSetting::Settings::DEFAULT)).value<PupilMethodSetting::Settings>();
    
        if(parameterConfigs->findText(QVariant::fromValue(configIndex).toString()) < 0) {
            qDebug() << "Did not found config: " << QVariant::fromValue(configIndex);
            parameterConfigs->setCurrentText("Default");
        } else {
            parameterConfigs->setCurrentText(QVariant::fromValue(configIndex).toString());
        }
    }

    virtual void updateSettings(){
        applicationSettings->setValue(settingsConfigParametersName, QVariant::fromValue(configParameters));
        applicationSettings->setValue(settingsConfigParametersIndexName, parameterConfigs->currentText());
    }

    void onResetClick(){
        QString configKey = parameterConfigs->itemText(parameterConfigs->currentIndex());
        Settings config = settingsMap[configKey];
        configParameters[config] = defaultParameters.value(config);
        onParameterConfigSelection(configKey);
    }

    virtual void onParameterConfigSelection(QString configKey);

    void onLoadFileClick() {
        QString filename = QFileDialog::getOpenFileName(this, tr("Open Algorithm Parameter File"), "", tr("JSON files (*.json)"));

        if(!filename.isEmpty()) {

            try {
                loadSettingsFromFile(filename);
            } catch(...) {
                QMessageBox msgBox;
                msgBox.setText("Error while loading parameter file. \nCorrect format and algorithm?");
                msgBox.exec();
            }
        }
    }

    virtual void loadSettingsFromFile(QString filename);

signals:

    void onConfigChange(QString configText);

};



#endif //PUPILEXT_PUPILMETHODSETTING_H
