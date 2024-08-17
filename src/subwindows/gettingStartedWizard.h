#pragma once

#include <QtWidgets/qwizard.h>
#include <QtCore/QSettings>
#include <opencv2/core/version.hpp>

class GettingStartedWizard : public QWizard {
    Q_OBJECT

public:

    enum WizardPurpose {
        ABOUT_AND_TUTORIAL,
        ABOUT_ONLY,
        TUTORIAL_ONLY
    };

    explicit GettingStartedWizard(WizardPurpose purpose, QWidget *parent = nullptr);

    QWizardPage *createIntro01();
    QWizardPage *createIntro02();
    QWizardPage *createTutorial01();
    QWizardPage *createTutorial02();
    QWizardPage *createTutorial03();
    QWizardPage *createTutorial04();
    QWizardPage *createTutorial05();
    QWizardPage *createTutorial06();
    QWizardPage *createTutorial07();
    QWizardPage *createTutorial08();
    QWizardPage *createTutorial09();
    QWizardPage *createTutorial10();
    QWizardPage *createTutorial11();
    QWizardPage *createTutorial12();
    QWizardPage *createConclusion01();

private:

    QSettings *applicationSettings;

    enum {
        Page_Intro_01,
        Page_Intro_02,
        Page_Tutorial_01,
        Page_Tutorial_02,
        Page_Tutorial_03,
        Page_Tutorial_04,
        Page_Tutorial_05,
        Page_Tutorial_06,
        Page_Tutorial_07,
        Page_Tutorial_08,
        Page_Tutorial_09,
        Page_Tutorial_10,
        Page_Tutorial_11,
        Page_Tutorial_12,
        Page_Conclusion_01, };

    WizardPurpose purpose;
    QString intro01Headline;
    QString tutorialHeadline;

};
