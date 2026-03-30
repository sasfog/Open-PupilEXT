
#include "openZipChoiceDialog.h"

OpenZipChoiceDialog::OpenZipChoiceDialog(const QString windowTitle, QVector<ImageReader::ZipMultiInfo> zipMultiInfo, QWidget *parent) :
        QDialog(parent)
{
    this->setWindowTitle(windowTitle);
    this->setMinimumSize(450,150);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *dialogLabel = new QLabel(tr("The zip file you chose to open contains multiple recordings in subdirectories. Which one would you like to open?"));
    dialogLabel->setWordWrap(true);
    mainLayout->addWidget(dialogLabel);

    comboBox = new QComboBox(this);
    for( const auto &zmi : zipMultiInfo ) {
        //qDebug() << zmi.recordingLength;
        //qDebug() << zmi.recordingLength/1000;
        //qDebug() << zmi.recordingLength/1000/60;
        //qDebug() << QString::number(zmi.recordingLength/1000/60, 'f', 3);

        QTime recordingLengthTime = QTime::fromMSecsSinceStartOfDay(zmi.recordingLength);
        QString recordingLengthStr = recordingLengthTime.toString("hh:mm:ss");
        comboBox->addItem(zmi.recordingName + " (" + recordingLengthStr + ")" );
    }
    comboBox->setCurrentIndex(0);
    //listWidget->setVerticalScrollMode();
    mainLayout->addWidget(comboBox);

    QFormLayout *buttonsLayout = new QFormLayout(this);
    buttonOpen = new QPushButton(tr("Open"));
    buttonCancel = new QPushButton(tr("Cancel"));
    buttonOpen->setFixedWidth(210);
    //buttonKeepAndSaveNew->setFixedWidth(200);
    buttonsLayout->addRow(buttonOpen, buttonCancel);
    mainLayout->addLayout(buttonsLayout);

    // Set default focus on the safer solution button
    buttonOpen->setDefault(false);
    buttonCancel->setDefault(true);

    setLayout(mainLayout);

    connect(buttonOpen, &QPushButton::clicked, this, &OpenZipChoiceDialog::onOpenClicked);
    connect(buttonCancel, &QPushButton::clicked, this, &OpenZipChoiceDialog::onCancelClicked);
//    connect(this, &QDialog::close, this, &OpenZipChoiceDialog::onKeepAndSaveNewClicked);

}

OpenZipChoiceDialog::~OpenZipChoiceDialog()  = default;

OpenZipChoiceDialog::OpenZipChoiceResponse OpenZipChoiceDialog::getResponse() {
    return openZipChoiceResponse;
}

int OpenZipChoiceDialog::getSelectedRecNumber() {
    return (comboBox->currentIndex() + 1);
}

void OpenZipChoiceDialog::onOpenClicked() {
    openZipChoiceResponse = OpenZipChoiceResponse::OPEN_SPECIFIC;
    accept();
}

void OpenZipChoiceDialog::onCancelClicked() {
    openZipChoiceResponse = OpenZipChoiceResponse::CANCEL;
    accept();
}
