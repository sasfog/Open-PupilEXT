#pragma once

/**
    @authors Moritz Lode, Gabor Benyei, Attila Boncser
*/

#include "PupilMethodSetting.h"
#include "../../pupil-detection-methods/ExCuSe.h"
#include <QtWidgets/QWidget>
#include <QtWidgets/QtWidgets>
#include <QtWidgets/QLabel>
#include "../../SVGIconColorAdjuster.h"

#include "json.h"
#include <fstream>
// for convenience
using json = nlohmann::json;

/**
    Pupil Detection Algorithm setting for the ExCuSe algorithm, displayed in the pupil detection setting dialog
*/
class ExCuSeSettings : public PupilMethodSetting {
    Q_OBJECT

public:

    explicit ExCuSeSettings(PupilDetection * pupilDetection, ExCuSe *m_excuse, QWidget *parent=0) : 
        PupilMethodSetting("ExCuSeSettings.configParameters", "ExCuSeSettings.configIndex", parent), 
        p_excuse(m_excuse), 
        pupilDetection(pupilDetection) {

        PupilMethodSetting::setDefaultParameters(defaultParameters);
        createForm();
        configsBox->setCurrentText(settingsMap.key(configIndex));

        if(isAutoParamEnabled()) {
            maxRadiBox->setEnabled(false);
        } else {
            maxRadiBox->setEnabled(true);
        }

        QVBoxLayout *infoLayout = new QVBoxLayout(infoBox);
        QHBoxLayout *infoLayoutRow1 = new QHBoxLayout();
        QPushButton *iLabelFakeButton = new QPushButton();
        iLabelFakeButton->setFlat(true);
        iLabelFakeButton->setAttribute(Qt::WA_NoSystemBackground, true);
        iLabelFakeButton->setAttribute(Qt::WA_TranslucentBackground, true);
        iLabelFakeButton->setStyleSheet("QPushButton { background-color: transparent; border: 0px }");
        iLabelFakeButton->setIcon(SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/status/22/dialog-information.svg"), applicationSettings));
        iLabelFakeButton->setFixedSize(QSize(32,32));
        iLabelFakeButton->setIconSize(QSize(32,32));
        infoLayoutRow1->addWidget(iLabelFakeButton);

        QLabel *pLabel = new QLabel();
        pLabel->setWordWrap(true);
        pLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
        pLabel->setOpenExternalLinks(true);
        SupportFunctions::setSmallerLabelFontSize(pLabel);
        pLabel->setText("Wolfgang Fuhl, Thomas Kübler, Katrin Sippel, Wolfgang Rosenstiel, Enkelejda Kasneci, \"ExCuSe: Robust Pupil Detection in Real-World Scenarios.\", 2015<br/>Part of the <a href=\"https://www-ti.informatik.uni-tuebingen.de/santini/EyeRecToo\">EyeRecToo</a> software. Copyright (c) 2018, Thiago Santini / University of Tübingen");
        infoLayoutRow1->addWidget(pLabel);

        infoLayout->addLayout(infoLayoutRow1);

        QLabel *confLabel;
        if(p_excuse->hasConfidence())
            confLabel = new QLabel("Info: This method does provide its own confidence.");
        else
            confLabel = new QLabel("Info: This method does not provide its own confidence, use the outline confidence.");
        SupportFunctions::setSmallerLabelFontSize(confLabel);
        confLabel->setWordWrap(true);
        infoLayout->addWidget(confLabel);

        QLabel *infoLabel = new QLabel("CAUTION: Processing using this algorithm may be very slow, reduce the camera acquiring fps accordingly.");
        SupportFunctions::setSmallerLabelFontSize(infoLabel);
        infoLabel->setWordWrap(true);
        infoLabel->setStyleSheet(QStringLiteral("QLabel{color: red;}"));
        infoLayout->addWidget(infoLabel);
#ifdef QT_DEBUG
        QLabel *warnLabel = new QLabel("CAUTION: Debug build may perform very slow. Use release build or adjust processing speed to not risk memory overflow.");
        SupportFunctions::setSmallerLabelFontSize(warnLabel);
        warnLabel->setWordWrap(true);
        warnLabel->setStyleSheet(QStringLiteral("QLabel{color: red;}"));
        infoLayout->addWidget(warnLabel);
#endif
        infoBox->setLayout(infoLayout);
    }

    ~ExCuSeSettings() override = default;

    void add2(ExCuSe *s_excuse) {
        excuse2 = s_excuse;
    }
    void add3(ExCuSe *s_excuse) {
        excuse3 = s_excuse;
    }
    void add4(ExCuSe *s_excuse) {
        excuse4 = s_excuse;
    }

public slots:

    void loadSettings() override {
        PupilMethodSetting::loadSettings();

        if(isAutoParamEnabled()) {
            float autoParamPupSizePercent = applicationSettings->value("autoParamPupSizePercent", pupilDetection->getAutoParamPupSizePercent()).toFloat();
            pupilDetection->setAutoParamEnabled(true);
            pupilDetection->setAutoParamPupSizePercent(autoParamPupSizePercent);
            pupilDetection->setAutoParamScheduled(true);

            maxRadiBox->setEnabled(false);
        } else {
            pupilDetection->setAutoParamEnabled(false);
            maxRadiBox->setEnabled(true);
        }

//        QList<float> selectedParameter = configParameters.value(configIndex);
//
//        maxRadiBox->setValue(selectedParameter[0]);
//        ellipseThresholdBox->setValue(selectedParameter[1]);

        applySpecificSettings();
    }

    void applySpecificSettings() override {

        // First come the parameters roughly independent from ROI size and relative pupil size
        int imgSize = p_excuse->imgSize;
        cv::InterpolationFlags interMethod = p_excuse->interMethod;
        int good_ellipse_threshold = p_excuse->good_ellipse_threshold;

        imgSize = imgSizeBox->value();
        interMethod = (cv::InterpolationFlags)interMethodBox->itemData(interMethodBox->currentIndex()).toInt();
        good_ellipse_threshold = ellipseThresholdBox->value();

        p_excuse->imgSize = imgSize;
        p_excuse->defSize = imgSize; // TODO: check?
        p_excuse->interMethod = interMethod;
        p_excuse->good_ellipse_threshold = good_ellipse_threshold;

        QList<float>& currentParameters = getCurrentParameters();
        currentParameters[1] = good_ellipse_threshold;

        if(excuse2) {
            excuse2->imgSize = imgSize;
            excuse2->defSize = imgSize; // TODO: check?
            excuse2->interMethod = interMethod;
            excuse2->good_ellipse_threshold = good_ellipse_threshold;
        }
        if(excuse3) {
            excuse3->imgSize = imgSize;
            excuse3->defSize = imgSize; // TODO: check?
            excuse3->interMethod = interMethod;
            excuse3->good_ellipse_threshold = good_ellipse_threshold;
        }
        if(excuse4) {
            excuse4->imgSize = imgSize;
            excuse4->defSize = imgSize; // TODO: check?
            excuse4->interMethod = interMethod;
            excuse4->good_ellipse_threshold = good_ellipse_threshold;
        }

        // Then the specific ones that are set by autoParam
        int procMode = pupilDetection->getCurrentProcMode();
        if(isAutoParamEnabled()) {
            float autoParamPupSizePercent = applicationSettings->value("autoParamPupSizePercent", pupilDetection->getAutoParamPupSizePercent()).toFloat();
            pupilDetection->setAutoParamPupSizePercent(autoParamPupSizePercent);
            pupilDetection->setAutoParamScheduled(true);
            
        } else {
            int max_ellipse_radi = p_excuse->max_ellipse_radi;
            
            max_ellipse_radi = maxRadiBox->value();
            
            p_excuse->max_ellipse_radi = max_ellipse_radi;

            currentParameters[0] = max_ellipse_radi;

            if(excuse2) {
                excuse2->max_ellipse_radi = max_ellipse_radi;
            }
            if(excuse3) {
                excuse3->max_ellipse_radi = max_ellipse_radi;
            }
            if(excuse4) {
                excuse4->max_ellipse_radi = max_ellipse_radi;
            }
            
        }

        emit onConfigChange(configsBox->currentText());
    }

    void applyAndSaveSpecificSettings() override {
        applySpecificSettings();
        PupilMethodSetting::saveSpecificSettings();
    }

private:

    ExCuSe *p_excuse;
    ExCuSe *excuse2 = nullptr;
    ExCuSe *excuse3 = nullptr;
    ExCuSe *excuse4 = nullptr;

    PupilDetection *pupilDetection;

    QSpinBox *imgSizeBox;
    QComboBox *interMethodBox;
    QSpinBox *maxRadiBox;
    QSpinBox *ellipseThresholdBox;

    void createForm() {
        PupilMethodSetting::loadSettings();
        QList<float>& selectedParameter = getCurrentParameters();

        int imgSize = selectedParameter[2];
        cv::InterpolationFlags interMethod = (cv::InterpolationFlags)(int)selectedParameter[3];
        int max_ellipse_radi = selectedParameter[0];
        int good_ellipse_threshold = selectedParameter[1];

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QHBoxLayout *configsLayout = new QHBoxLayout();

        configsBox = new QComboBox();
        QLabel *parameterConfigsLabel = new QLabel(tr("Parameter configuration:"));
        configsBox->setFixedWidth(250);
        configsLayout->addWidget(parameterConfigsLabel);
        configsLayout->addWidget(configsBox);

                for (QMap<QString, Settings>::const_iterator cit = settingsMap.cbegin(); cit != settingsMap.cend(); cit++)
        {
            configsBox->addItem(cit.key());
        }

        connect(configsBox, SIGNAL(currentTextChanged(QString)), this, SLOT(onParameterConfigSelection(QString)));

        mainLayout->addLayout(configsLayout);

        QHBoxLayout *configsNoteLayout = new QHBoxLayout();
        QLabel* configsNoteLabel = new QLabel(tr("Note: Configurations marked with an asterisk (*) are recommended for Basler\nacA2040-120um (1/1.8\" sensor format) camera(s) equipped with f=50 mm 2/3\"\nnominal sensor format lens, using 4:3 aspect ratio pupil detection ROI(s)."));
        SupportFunctions::setSmallerLabelFontSize(configsNoteLabel);
        configsNoteLabel->setFixedHeight(60);
        configsNoteLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        configsNoteLayout->addWidget(configsNoteLabel);
        mainLayout->addLayout(configsNoteLayout);

        mainLayout->addSpacerItem(new QSpacerItem(40, 5, QSizePolicy::Fixed));

        QGroupBox *sizeGroup = new QGroupBox("Algorithm specific: Image Size (Downscaling)");
        QFormLayout *sizeLayout = new QFormLayout();

        QLabel *imgSizeLabel = new QLabel(tr("Image size (square) [px]:"));
        imgSizeBox = new QSpinBox();
        imgSizeBox->setMaximum(800);
        imgSizeBox->setValue(imgSize);
        imgSizeBox->setFixedWidth(80);

        sizeLayout->addRow(imgSizeLabel, imgSizeBox);

        QLabel *interMethodLabel = new QLabel(tr("Interpolation method:"));
        interMethodBox = new QComboBox();
        interMethodBox->addItem(QString("Linear"), cv::InterpolationFlags::INTER_LINEAR);
        interMethodBox->addItem(QString("Area"), cv::InterpolationFlags::INTER_AREA);
        interMethodBox->addItem(QString("Cubic"), cv::InterpolationFlags::INTER_CUBIC);
        interMethodBox->addItem(QString("Lanczos"), cv::InterpolationFlags::INTER_LANCZOS4);
        interMethodBox->setCurrentIndex(interMethodBox->findData(interMethod));
        interMethodBox->setFixedWidth(80);

        sizeLayout->addRow(interMethodLabel, interMethodBox);

        sizeGroup->setLayout(sizeLayout);
        mainLayout->addWidget(sizeGroup);

        QGroupBox *ellipseFitGroup = new QGroupBox("Algorithm specific: Ellipse Fit");

        QFormLayout *ellipseFitLayout = new QFormLayout();

        QLabel *maxRadiLabel = new QLabel(tr("Max. Ellipse Radius [px]:"));
        maxRadiBox = new QSpinBox();
        maxRadiBox->setMaximum(5000);
        maxRadiBox->setValue(max_ellipse_radi);
        maxRadiBox->setFixedWidth(80);
        ellipseFitLayout->addRow(maxRadiLabel, maxRadiBox);

        QLabel *ellipseThresholdLabel = new QLabel(tr("Ellipse Goodness Threshold:"));
        ellipseThresholdBox = new QSpinBox();
        ellipseThresholdBox->setMaximum(100);
        ellipseThresholdBox->setValue(good_ellipse_threshold);
        ellipseThresholdBox->setFixedWidth(80);
        ellipseFitLayout->addRow(ellipseThresholdLabel, ellipseThresholdBox);

        ellipseFitGroup->setLayout(ellipseFitLayout);
        mainLayout->addWidget(ellipseFitGroup);


        QHBoxLayout *buttonsLayout = new QHBoxLayout();

        resetButton = new QPushButton("Reset algorithm parameters");
        fileButton = new QPushButton("Load config file");

        buttonsLayout->addWidget(resetButton);
        connect(resetButton, SIGNAL(clicked()), this, SLOT(onResetClick()));
        buttonsLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding));

        buttonsLayout->addWidget(fileButton);
        connect(fileButton, SIGNAL(clicked()), this, SLOT(onLoadFileClick()));

        mainLayout->addLayout(buttonsLayout);

        setLayout(mainLayout);
    }

    void loadSettingsFromFile(QString filename) {

        std::ifstream file(filename.toStdString());
        json j;
        file>> j;

        //std::cout << std::setw(4) << j << std::endl;

        QList<float> customs = PupilMethodSetting::defaultParameters[Settings::DEFAULT];

        // GB: I think it is meaningful to have these here, so I added
        customs[2] = j["Parameter Set"]["imgSize"];
        customs[3] = j["Parameter Set"]["interMethod"];

        customs[0] = j["Parameter Set"]["max_ellipse_radi"];
        customs[1] = j["Parameter Set"]["good_ellipse_threshold"];

        insertCustomEntry(customs);
    }

    QMap<Settings, QList<float>> defaultParameters = {
            { Settings::DEFAULT, {50.0f, 15.0f, 680, cv::InterpolationFlags::INTER_LINEAR} },
            { Settings::ROI_0_3_OPTIMIZED, {146.0f, 7.0f, 680, cv::InterpolationFlags::INTER_LINEAR} },
            { Settings::ROI_0_6_OPTIMIZED, {216.0f, 34.0f, 680, cv::InterpolationFlags::INTER_LINEAR} },
            { Settings::FULL_IMAGE_OPTIMIZED, {39.0f, 0.0f, 680, cv::InterpolationFlags::INTER_LINEAR} },
            { Settings::AUTOMATIC_PARAMETRIZATION, {-1.0f, 15.0f, 680, cv::InterpolationFlags::INTER_AREA} },
            { Settings::CUSTOM, {-1.0f, 15.0f, 680, cv::InterpolationFlags::INTER_AREA} }
    };
    // GB: TODO: possible slight discrepancy: the former hardcoded DEF_SIZE was by default 800 set by Moritz (?)
    // but now we have it the same as imgSize.. e.g. 680
    // but by default by the alg. authors, IMG_SIZE was 400 and DEF_SIZE was 800..
    // so we need to make clear what defSize should be, to keep contingency with previous
    // PupilEXT-version (0.1.1. or 0.1.2) hardcoded performance but also provide flexibility.

private slots:

    void onParameterConfigSelection(QString configKey) {
        setConfigIndex(configKey);
        QList<float>& selectedParameter = getCurrentParameters();

        imgSizeBox->setValue(selectedParameter[2]);
        interMethodBox->setCurrentIndex(interMethodBox->findData(selectedParameter[3]));
        ellipseThresholdBox->setValue(selectedParameter[1]);

        if(isAutoParamEnabled()) {
            maxRadiBox->setEnabled(false);
            // TODO: hide value text too
        } else {
            maxRadiBox->setEnabled(true);
            maxRadiBox->setValue(selectedParameter[0]);
        }

        //applySpecificSettings(); // settings are only updated when apply click in pupildetectionsettingsdialog
    }

};
