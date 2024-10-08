#pragma once

/**
    @authors Moritz Lode, Gabor Benyei, Attila Boncser
*/

#include "PupilMethodSetting.h"
#include "../../pupil-detection-methods/Starburst.h"
#include <QtWidgets/QWidget>
#include <QtWidgets/QtWidgets>
#include <QtWidgets/QLabel>
#include "../../SVGIconColorAdjuster.h"

#include "json.h"
#include <fstream>
// for convenience
using json = nlohmann::json;

/**
    Pupil Detection Algorithm setting for the Starburst algorithm, displayed in the pupil detection setting dialog
*/
class StarburstSettings : public PupilMethodSetting {
    Q_OBJECT

public:

    // GB: added pupilDetection instance to get the actual ROIs for Autometric Parametrization calculations
    explicit StarburstSettings(PupilDetection * pupilDetection, Starburst *m_starburst, QWidget *parent=0) : 
        PupilMethodSetting("StarburstSettings.configParameters","StarburstSettings.configIndex", parent), 
        p_starburst(m_starburst), 
        pupilDetection(pupilDetection){

        PupilMethodSetting::setDefaultParameters(defaultParameters);
        createForm();
        configsBox->setCurrentText(settingsMap.key(configIndex));

        if(isAutoParamEnabled()) {
            edgeThresholdBox->setEnabled(false);
            //
            crRatioBox->setEnabled(false);
            crWindowSizeBox->setEnabled(false);
        } else {
            edgeThresholdBox->setEnabled(true);
            //
            crRatioBox->setEnabled(true);
            crWindowSizeBox->setEnabled(true);
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
        pLabel->setText("Li, Dongheng & Winfield, D. & Parkhurst, D.J., \"Starburst: A hybrid algorithm for video-based eye tracking combining feature-based and model-based approaches.\", 2005<br/>Part of the <a href=\"http://thirtysixthspan.com/openEyes/software.html\">cvEyeTracker</a> software License: <a href=\"https://www.gnu.org/licenses/gpl-3.0.txt\">GPL</a>");
        infoLayoutRow1->addWidget(pLabel);

        infoLayout->addLayout(infoLayoutRow1);

        QLabel *confLabel;
        if(p_starburst->hasConfidence())
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
#if _DEBUG
        QLabel *warnLabel = new QLabel("CAUTION: Debug build may perform very slow. Use release build or adjust processing speed to not risk memory overflow.");
        SupportFunctions::setSmallerLabelFontSize(warnLabel);
        warnLabel->setWordWrap(true);
        warnLabel->setStyleSheet(QStringLiteral("QLabel{color: red;}"));
        infoLayout->addWidget(warnLabel);
#endif

        infoBox->setLayout(infoLayout);
    }

    ~StarburstSettings() override = default;

    void add2(Starburst *s_starburst) {
        starburst2 = s_starburst;
    }
    void add3(Starburst *s_starburst) {
        starburst3 = s_starburst;
    }
    void add4(Starburst *s_starburst) {
        starburst4 = s_starburst;
    }

public slots:

    void loadSettings() override {
        PupilMethodSetting::loadSettings();

        if(isAutoParamEnabled()) {
            float autoParamPupSizePercent = applicationSettings->value("autoParamPupSizePercent", pupilDetection->getAutoParamPupSizePercent()).toFloat();
            pupilDetection->setAutoParamEnabled(true);
            pupilDetection->setAutoParamPupSizePercent(autoParamPupSizePercent);
            pupilDetection->setAutoParamScheduled(true);

            edgeThresholdBox->setEnabled(false);
            //
            crRatioBox->setEnabled(false);
            crWindowSizeBox->setEnabled(false);
        } else {
            pupilDetection->setAutoParamEnabled(false);
            edgeThresholdBox->setEnabled(true);
            //
            crRatioBox->setEnabled(true);
            crWindowSizeBox->setEnabled(true);
        }

        applySpecificSettings();
    }

    void applySpecificSettings() override {
        
        // First come the parameters roughly independent from ROI size and relative pupil size 
        int rays = p_starburst->rays;				            //number of rays to use to detect feature points
        int min_feature_candidates = p_starburst->min_feature_candidates;	//minimum number of pupil feature candidates

        p_starburst->rays = numRaysBox->value();
        p_starburst->min_feature_candidates = minFeatureCandidatesBox->value();

        QList<float>& currentParameters = getCurrentParameters();
        currentParameters[1] = numRaysBox->value();
        currentParameters[2] = minFeatureCandidatesBox->value();

        if(starburst2) {
            starburst2->rays = numRaysBox->value();
            starburst2->min_feature_candidates = minFeatureCandidatesBox->value();
        }
        if(starburst3) {
            starburst3->rays = numRaysBox->value();
            starburst3->min_feature_candidates = minFeatureCandidatesBox->value();
        }
        if(starburst4) {
            starburst4->rays = numRaysBox->value();
            starburst4->min_feature_candidates = minFeatureCandidatesBox->value();
        }

        // Then the specific ones that are set by autoParam
        int procMode = pupilDetection->getCurrentProcMode();
        if(isAutoParamEnabled()) {
            // TODO: GET VALUE FROM pupildetection failsafe
            float autoParamPupSizePercent = applicationSettings->value("autoParamPupSizePercent", pupilDetection->getAutoParamPupSizePercent()).toFloat();
            pupilDetection->setAutoParamPupSizePercent(autoParamPupSizePercent);
            pupilDetection->setAutoParamScheduled(true);

        } else {

            int edge_threshold = p_starburst->edge_threshold;		//threshold of pupil edge points detection
            //
            int corneal_reflection_ratio_to_image_size = p_starburst->corneal_reflection_ratio_to_image_size; // approx max size of the reflection relative to image height -> height/this
            int crWindowSize = p_starburst->crWindowSize;		    //corneal reflection search window size

            p_starburst->edge_threshold = edgeThresholdBox->value();
            //
            p_starburst->corneal_reflection_ratio_to_image_size = crRatioBox->value();
            p_starburst->crWindowSize = crWindowSizeBox->value();

            currentParameters[0] = edgeThresholdBox->value();
            //
            currentParameters[3] = crRatioBox->value(); 
            currentParameters[4] = crWindowSizeBox->value(); 

            if(starburst2) {
                starburst2->edge_threshold = edgeThresholdBox->value();
                //
                starburst2->corneal_reflection_ratio_to_image_size = crRatioBox->value();
                starburst2->crWindowSize = crWindowSizeBox->value();
            }
            if(starburst3) {
                starburst3->edge_threshold = edgeThresholdBox->value();
                //
                starburst3->corneal_reflection_ratio_to_image_size = crRatioBox->value();
                starburst3->crWindowSize = crWindowSizeBox->value();
            }
            if(starburst4) {
                starburst4->edge_threshold = edgeThresholdBox->value();
                //
                starburst4->corneal_reflection_ratio_to_image_size = crRatioBox->value();
                starburst4->crWindowSize = crWindowSizeBox->value();
            }
            
        }

        emit onConfigChange(configsBox->currentText());
    }

    void applyAndSaveSpecificSettings() override {
        applySpecificSettings();
        PupilMethodSetting::saveSpecificSettings();
    }

private:

    Starburst *p_starburst;
    Starburst *starburst2 = nullptr;
    Starburst *starburst3 = nullptr;
    Starburst *starburst4 = nullptr;

    PupilDetection *pupilDetection;

    QSpinBox *edgeThresholdBox;
    QSpinBox *numRaysBox;
    QSpinBox *minFeatureCandidatesBox;
    QSpinBox *crRatioBox;
    QSpinBox *crWindowSizeBox;

    void createForm() {
        PupilMethodSetting::loadSettings();
        QList<float> selectedParameter = configParameters.value(configIndex);

        int edge_threshold = selectedParameter[0];
        int rays = selectedParameter[1];
        int min_feature_candidates = selectedParameter[2];
        int corneal_reflection_ratio_to_image_size = selectedParameter[3];
        int crWindowSize = selectedParameter[4];


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

        QGroupBox *edgeGroup = new QGroupBox("Algorithm specific: Edge Detection");
        QGroupBox *crGroup = new QGroupBox("Algorithm specific: Corneal Reflection (CR)");

        QFormLayout *edgeLayout = new QFormLayout();
        QFormLayout *crLayout = new QFormLayout();

        QLabel *edgeThresholdLabel = new QLabel(tr("Edge Threshold:"));
        edgeThresholdBox = new QSpinBox();
        edgeThresholdBox->setMaximum(1000);
        edgeThresholdBox->setValue(edge_threshold);
        edgeThresholdBox->setFixedWidth(80);
        edgeLayout->addRow(edgeThresholdLabel, edgeThresholdBox);

        QLabel *numRaysLabel = new QLabel(tr("Number of Rays:"));
        numRaysBox = new QSpinBox();
        numRaysBox->setValue(rays);
        numRaysBox->setFixedWidth(80);
        edgeLayout->addRow(numRaysLabel, numRaysBox);

        QLabel *minFeatureCandidatesLabel = new QLabel(tr("Min. Feature Candidates:"));
        minFeatureCandidatesBox = new QSpinBox();
        minFeatureCandidatesBox->setValue(min_feature_candidates);
        minFeatureCandidatesBox->setFixedWidth(80);
        edgeLayout->addRow(minFeatureCandidatesLabel, minFeatureCandidatesBox);

        edgeGroup->setLayout(edgeLayout);
        mainLayout->addWidget(edgeGroup);

        QLabel *crRatioLabel = new QLabel(tr("CR Ratio (to Image Height):"));
        crRatioBox = new QSpinBox();
        crRatioBox->setValue(corneal_reflection_ratio_to_image_size);
        crRatioBox->setFixedWidth(80);
        crLayout->addRow(crRatioLabel, crRatioBox);

        QLabel *crWindowSizeLabel = new QLabel(tr("CR Window Size [px]:"));
        crWindowSizeBox = new QSpinBox();
        crWindowSizeBox->setMaximum(5000);
        crWindowSizeBox->setValue(crWindowSize);
        crWindowSizeBox->setFixedWidth(80);
        crLayout->addRow(crWindowSizeLabel, crWindowSizeBox);

        crGroup->setLayout(crLayout);
        mainLayout->addWidget(crGroup);

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

        QList<float> customs = defaultParameters[Settings::DEFAULT];

        customs[0] = j["Parameter Set"]["edge_threshold"];
        customs[1] =j["Parameter Set"]["rays"];
        customs[2] =j["Parameter Set"]["min_feature_candidates"];
        customs[3] =j["Parameter Set"]["corneal_reflection_ratio_to_image_size"];
        customs[4] =j["Parameter Set"]["crWindowSize"];


      insertCustomEntry(customs);

    }

    QMap<Settings, QList<float>> defaultParameters = {
            { Settings::DEFAULT, {20.0f, 18.0f, 10.0f, 10.0f, 301.0f} },
            { Settings::ROI_0_3_OPTIMIZED, {77.0f, 8.0f, 2.0f, 4.0f, 417.0f} },
            { Settings::ROI_0_6_OPTIMIZED, {27.0f, 8.0f, 1.0f, 10.0f, 197.0f} },
            { Settings::FULL_IMAGE_OPTIMIZED, {21.0f, 32.0f, 7.0f, 10.0f, 433.0f} },
            { Settings::AUTOMATIC_PARAMETRIZATION, {-1.0f, 12.0f, 8.0f, -1.0f, -1.0f} },
            { Settings::CUSTOM, {-1.0f, 8.0f, 7.0f, -1.0f, -1.0f} }
    };


private slots:

    void onParameterConfigSelection(QString configKey) {
        setConfigIndex(configKey);
        QList<float>& selectedParameter = getCurrentParameters();

        // First come the parameters roughly independent from ROI size and relative pupil size 
        numRaysBox->setValue(selectedParameter[1]);
        minFeatureCandidatesBox->setValue(selectedParameter[2]);

        // Then the specific ones that are set by autoParam
        if(isAutoParamEnabled()) {
            edgeThresholdBox->setEnabled(false);
            //
            crRatioBox->setEnabled(false);
            crWindowSizeBox->setEnabled(false);
            // TODO: hide value text too
        } else {
            edgeThresholdBox->setEnabled(true);
            //
            crRatioBox->setEnabled(true);
            crWindowSizeBox->setEnabled(true);

            edgeThresholdBox->setValue(selectedParameter[0]);
            //
            crRatioBox->setValue(selectedParameter[3]);
            crWindowSizeBox->setValue(selectedParameter[4]);
        }

        //applySpecificSettings(); // settings are only updated when apply click in pupildetectionsettingsdialog
    }

};
