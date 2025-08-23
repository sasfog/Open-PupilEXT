#pragma once

#include <QtWidgets>
#include <QDialog>
#include <QCheckBox>
#include <QDebug>
#include <QLabel>
#include <QVBoxLayout>

class ThreeChoiceDialog : public QDialog {
Q_OBJECT

public:

    enum ThreeChoiceResponse {OPTION_1 = 1, OPTION_2 = 2, OPTION_3 = 3};

    explicit ThreeChoiceDialog(
            const QString windowTitle,
            const QString contentText,
            const QString option1Text,
            const QString option2Text,
            const QString option3Text,
            //const QString checkboxText,
            //const int option1width,
            //const int option2width,
            const QSize minSizeW,
            QWidget *parent = nullptr) {

        this->setWindowTitle(windowTitle);
        //this->setMinimumSize(450,150);
        this->setMinimumSize(minSizeW);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QLabel *dialogLabel = new QLabel(contentText); // "Existing data was found under the target path/name you specified. Please choose whether you would like to append to the existing recording or keep it and save the new recording with an automatically generated different path/name?"
        dialogLabel->setWordWrap(true);
        mainLayout->addWidget(dialogLabel);

        //rememberChoiceBox = new QCheckBox(checkboxText); // "Remember this choice"
        //mainLayout->addWidget(rememberChoiceBox);

        QHBoxLayout *buttonsLayout = new QHBoxLayout(this);
        buttonOption1 = new QPushButton(option1Text);
        buttonOption2 = new QPushButton(option2Text);
        buttonOption3 = new QPushButton(option3Text);
        // buttonOption1->setFixedWidth(option1Width); // 210
        // //buttonOption2->setFixedWidth(option2Width); // 200
        buttonsLayout->addWidget(buttonOption1);
        buttonsLayout->addStretch();
        buttonsLayout->addWidget(buttonOption2);
        buttonsLayout->addStretch();
        buttonsLayout->addWidget(buttonOption3);
        mainLayout->addLayout(buttonsLayout);

        // Set default focus on option 1
        buttonOption1->setDefault(true);
        buttonOption2->setDefault(false);
        buttonOption3->setDefault(false);

        setLayout(mainLayout);

        connect(buttonOption1, &QPushButton::clicked, this, &ThreeChoiceDialog::onOption1Clicked);
        connect(buttonOption2, &QPushButton::clicked, this, &ThreeChoiceDialog::onOption2Clicked);
        connect(buttonOption3, &QPushButton::clicked, this, &ThreeChoiceDialog::onOption3Clicked);
//    connect(this, &QDialog::close, this, &ThreeChoiceDialog::onKeepAndSaveNewClicked);

    };
    ~ThreeChoiceDialog() override = default;
    ThreeChoiceResponse getResponse() {
        return threeChoiceResponse;
    };
    //bool getRememberChoice() {
    //    return rememberChoice;
    //};

private slots:
    void onOption1Clicked() {
        //rememberChoice = rememberChoiceBox->isChecked();
        threeChoiceResponse = ThreeChoiceResponse::OPTION_1;
        accept();
    };
    void onOption2Clicked() {
        //rememberChoice = rememberChoiceBox->isChecked();
        threeChoiceResponse = ThreeChoiceResponse::OPTION_2;
        accept();
    };
    void onOption3Clicked() {
        //rememberChoice = rememberChoiceBox->isChecked();
        threeChoiceResponse = ThreeChoiceResponse::OPTION_3;
        accept();
    };

private:
    ThreeChoiceResponse threeChoiceResponse;
    //bool rememberChoice = false;
    //QCheckBox *rememberChoiceBox;
    QPushButton *buttonOption1;
    QPushButton *buttonOption2;
    QPushButton *buttonOption3;
};
