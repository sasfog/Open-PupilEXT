#pragma once

/**
    @author Moritz Lode, Gabor Benyei, Attila Boncser
*/

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>
#include <QtCore/QSettings>
#include "../imageReader.h"

/**
    General settings window (dialog) containing all general settings of the pupilext software
*/

class OpenZipChoiceDialog : public QDialog {
    Q_OBJECT

public:

    enum OpenZipChoiceResponse {OPEN_SPECIFIC = 1, CANCEL = 2};

    explicit OpenZipChoiceDialog(const QString windowTitle, QVector<ImageReader::ZipMultiInfo> zipMultiInfo, QWidget *parent = nullptr);
    ~OpenZipChoiceDialog() override;
    OpenZipChoiceResponse getResponse();
    int getSelectedRecNumber();

private slots:
    void onOpenClicked();
    void onCancelClicked();

private:

    //QSettings *applicationSettings;

    OpenZipChoiceResponse openZipChoiceResponse;
    QComboBox *comboBox;
    QPushButton *buttonOpen;
    QPushButton *buttonCancel;

};
