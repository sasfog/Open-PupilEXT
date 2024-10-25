#pragma once

/**
    @author Gabor Benyei, Attila Boncser
*/

#include <QSettings>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtCore/qdir.h>
#include <QtWidgets/QPushButton>
#include "QtOpenGLViewer/QtOpenGLViewer.h"


/**

    under construction

*/
class SetupGeometryDialog : public QDialog {
Q_OBJECT

public:

    explicit SetupGeometryDialog(QWidget *parent = nullptr);

    ~SetupGeometryDialog() override;

protected:

    void reject() override;

private:

    QSettings *applicationSettings;

    QPushButton *applyButton;
    QPushButton *cancelButton;
    QPushButton *applyCloseButton;

    QtOpenGLViewer *qtOpenGlViewer;


    QCheckBox *a1Box;
    QCheckBox *a2Box;
    QCheckBox *a3Box;

    QGroupBox *aGroup;
    QLabel *aLabel;
    QComboBox *comboBox;


    void createForm();
    void updateForm();
    void loadSettings();

private slots:

    //void onComboSelection(int idx);
    void applyButtonClick();
    void cancelButtonClick();
    void applyCloseButtonClick();

    void updateContents();

public slots:

    void onSettingsChange();

};
