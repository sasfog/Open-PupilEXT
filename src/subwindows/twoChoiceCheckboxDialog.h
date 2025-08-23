#pragma once

#include <QtWidgets>
#include <QDialog>
#include <QCheckBox>
#include <QDebug>
#include <QLabel>
#include <QVBoxLayout>

class TwoChoiceCheckboxDialog : public QDialog {
Q_OBJECT

public:

    enum TwoChoiceCheckboxResponse {OPTION_1 = 1, OPTION_2 = 2};

    explicit TwoChoiceCheckboxDialog(
            const QString windowTitle,
            const QString contentText,
            const QString option1Text,
            const QString option2Text,
            const QString checkboxText,
            //const int option1width,
            //const int option2width,
            const QSize minSizeW,
            bool isOption2Default,
            QWidget *parent = nullptr) {

        this->setWindowTitle(windowTitle);
        //this->setMinimumSize(450,150);
        this->setMinimumSize(minSizeW);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QLabel *dialogLabel = new QLabel(contentText); // "Existing data was found under the target path/name you specified. Please choose whether you would like to append to the existing recording or keep it and save the new recording with an automatically generated different path/name?"
        dialogLabel->setWordWrap(true);
        mainLayout->addWidget(dialogLabel);

        rememberChoiceBox = new QCheckBox(checkboxText); // "Remember this choice"
        mainLayout->addWidget(rememberChoiceBox);

        QFormLayout *buttonsLayout = new QFormLayout(this);
        buttonOption1 = new QPushButton(option1Text); // "Append to existing"
        buttonOption2 = new QPushButton(option2Text); // "Keep existing and save new as well"
        // buttonOption1->setFixedWidth(option1Width); // 210
        // //buttonOption2->setFixedWidth(option2Width); // 200
        buttonsLayout->addRow(buttonOption1, buttonOption2);
        mainLayout->addLayout(buttonsLayout);

        // Set default focus on the safer solution button
        if(isOption2Default) {
            buttonOption1->setDefault(false);
            buttonOption2->setDefault(true);
        } else {
            buttonOption1->setDefault(true);
            buttonOption2->setDefault(false);
        }

        setLayout(mainLayout);

        connect(buttonOption1, &QPushButton::clicked, this, &TwoChoiceCheckboxDialog::onOption1Clicked);
        connect(buttonOption2, &QPushButton::clicked, this, &TwoChoiceCheckboxDialog::onOption2Clicked);
//    connect(this, &QDialog::close, this, &TwoChoiceCheckboxDialog::onKeepAndSaveNewClicked);

    };
    ~TwoChoiceCheckboxDialog() override = default;
    TwoChoiceCheckboxResponse getResponse() {
        return twoChoiceCheckboxResponse;
    };
    bool getRememberChoice() {
        return rememberChoice;
    };

private slots:
    void onOption1Clicked() {
        rememberChoice = rememberChoiceBox->isChecked();
        twoChoiceCheckboxResponse = TwoChoiceCheckboxResponse::OPTION_1;
        accept();
    };
    void onOption2Clicked() {
        rememberChoice = rememberChoiceBox->isChecked();
        twoChoiceCheckboxResponse = TwoChoiceCheckboxResponse::OPTION_2;
        accept();
    };

private:
    TwoChoiceCheckboxResponse twoChoiceCheckboxResponse;
    bool rememberChoice = false;
    QCheckBox *rememberChoiceBox;
    QPushButton *buttonOption1;
    QPushButton *buttonOption2;
};
