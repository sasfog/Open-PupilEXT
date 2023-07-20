
#ifndef PUPILEXT_PUPILMETHODSETTING_H
#define PUPILEXT_PUPILMETHODSETTING_H

/**
    @authors Moritz Lode, Gábor Bényei
*/

#include <QtWidgets/QWidget>


enum class Settings {
            DEFAULT,
            ROI_0_3_OPTIMIZED,
            ROI_0_6_OPTIMIZED,
            FULL_IMAGE_OPTIMIZED,
            AUTOMATIC_PARAMETRIZATION
};

/**
    Abstract class representing the pupil detection algorithm's individual parameters, this widget will be integrated into the pupil detection settings configuration windows
*/
class PupilMethodSetting : public QWidget {
    Q_OBJECT

public:

    QWidget *infoBox;
    QSettings *applicationSettings;

    // GB: a
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

    // GB added begin
    virtual bool isAutoParamEnabled() {
        return false;
    }
    // GB added end

public slots:

    virtual void loadSettings() {

    }

    virtual void updateSettings() {

    }

signals:

    void onConfigChange(QString configText);

};



#endif //PUPILEXT_PUPILMETHODSETTING_H
