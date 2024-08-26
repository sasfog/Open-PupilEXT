#pragma once

/**
    @authors Moritz Lode, Gabor Benyei, Attila Boncser
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
            { "Default*", Settings::DEFAULT },
            { "ROI 0.3 Optimized*", Settings::ROI_0_3_OPTIMIZED },
            { "ROI 0.6 Optimized*", Settings::ROI_0_6_OPTIMIZED },
            { "Full Image Optimized*", Settings::FULL_IMAGE_OPTIMIZED },
            { "Automatic Parametrization", Settings::AUTOMATIC_PARAMETRIZATION },
            { "Custom", Settings::CUSTOM }
    };

    //virtual void addSecondary(PupilDetectionMethod *method) = 0;

    QList<float>& getParameter(QString configKey){
        Settings config = settingsMap[configKey.toUpper().replace("*","").replace(" ","_")];
        return configParameters[config];
    }

    QList<float>& getCurrentParameters(){
        return configParameters[configIndex];
    }

    void setConfigIndex(QString configKey){
        // NOTE: It is very important that the (.toUpper().replace("*","").replace(" ","_")) version of the
        // congifsBox widget selected item HAS TO MATCH the map key name
        configIndex = QVariant::fromValue(configKey.toUpper().replace("*","").replace(" ","_")).value<Settings>();
    }

    QMap<Settings, QList<float>>& getConfigParameters() {
        return configParameters;
    }

    void setConfigParameters(QMap<Settings, QList<float>> params) {
        if(defaultParameters.size() == params.size())
            configParameters = params;
    }

    void reset() {
        configParameters = defaultParameters;
    }

    virtual bool isAutoParamEnabled() {
        return (configsBox->currentText() == "Automatic Parametrization");
    }

protected:
    QMap<Settings, QList<float>> defaultParameters;
    QMap<Settings, QList<float>> configParameters;
    Settings configIndex;
    QPushButton *resetButton;
    QComboBox *configsBox;
    QPushButton *fileButton;

    QString settingsConfigParametersName;
    QString settingsConfigParametersIndexName;

    void setDefaultParameters(const QMap<Settings, QList<float>> &defaultParameters){
        this->defaultParameters = defaultParameters;
        this->configParameters = defaultParameters;
    }

    void insertCustomEntry(QList<float> customParameters){
        configParameters.insert(Settings::CUSTOM, customParameters);

        if(configsBox->findText(settingsMap.key(Settings::CUSTOM)) < 0) {
            configsBox->addItem(settingsMap.key(Settings::CUSTOM));
        }
        configsBox->setCurrentText(settingsMap.key(Settings::CUSTOM));
    }

public slots:

    virtual void loadSettings() {


        QVariant parametersConf = applicationSettings->value(settingsConfigParametersName);
        QMap<QString, QList<float>> confToRead;
        if (parametersConf.isValid()){
            confToRead = parametersConf.value<QMap<QString, QList<float>>>();
            for (QMap<QString, QList<float>>::const_iterator cit = confToRead.cbegin(); cit != confToRead.cend(); cit++)
            {
                configParameters.insert(QVariant::fromValue(cit.key().toUpper()).value<Settings>(), cit.value());
            }
        }
        else {
            for (QMap<Settings, QList<float>>::const_iterator cit = defaultParameters.cbegin(); cit != defaultParameters.cend(); cit++)
            {
                configParameters.insert(cit.key(), cit.value());
            }        
        }
        
        QVariant indexConf = applicationSettings->value(settingsConfigParametersIndexName);
        if (parametersConf.isValid()){
            configIndex = indexConf.value<Settings>();

        }
        else{
//            configIndex = Settings::DEFAULT;
            configIndex = Settings::AUTOMATIC_PARAMETRIZATION;
        }
    }

    virtual void applyAndSaveSpecificSettings(){
        saveSpecificSettings();
    }

    virtual void applySpecificSettings(){}

    virtual void saveSpecificSettings() {
        QMap<QString, QList<float>> confToWrite;
        QString indexToWrite = QVariant::fromValue(configIndex).toString();

        for (QMap<Settings, QList<float>>::const_iterator cit = configParameters.cbegin(); cit != configParameters.cend(); cit++)
        {
            confToWrite.insert(QVariant::fromValue(cit.key()).toString(), cit.value());
        }
        //applicationSettings->setValue(settingsConfigParametersName, parameters);
        applicationSettings->setValue(settingsConfigParametersName, QVariant::fromValue(confToWrite));
        applicationSettings->setValue(settingsConfigParametersIndexName, indexToWrite);
    }

virtual void onParameterConfigSelection(QString configKey){}

    void onResetClick(){
        QString configKey = configsBox->itemText(configsBox->currentIndex());
        Settings config = settingsMap[configKey];
        configParameters[config] = defaultParameters.value(config);
        onParameterConfigSelection(configKey);
    }

    
    virtual void loadSettingsFromFile(QString filename){}

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

signals:

    void onConfigChange(QString configText);

};
