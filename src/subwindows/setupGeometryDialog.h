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
#include <QTreeView>

#include "setupModelTreeView/setupModelTreeModel.h"
#include "QJsonModel/QJsonModel.hpp"


/**

    under construction

    Note:
    There is a tree view here, which depicts the internals of the geometry setup model. It could be argued that the tree
    model (necessary for the tree view itself to work) could be used as the geometry setup model at the same time.
    However, in the long run, it would be better to separate GUI and underlying data. So this tree model used here is
    "redundantly" kept and updated with the setup model back and forth.

*/
class SetupGeometryDialog : public QDialog {
Q_OBJECT

public:

    enum SetupHelp {
        SETUP_3D,
        COMPONENT_HELP_LENS,
        COMPONENT_HELP_CAMERA,
        COMPONENT_HELP_ILLUMINATOR,
        COMPONENT_HELP_HEAD
    };

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

    QWidget *helpBox;
    QScrollArea *helpBoxSca;
    QVBoxLayout *helpBoxLayout;
    QLabel *helpContentText;

    QPushButton *helpContentImage = new QPushButton();

    QIcon helpLensIcon1;
    QIcon helpHeadIcon1;

    int testHelp = 0;


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

    void showSetupHelp(SetupHelp setupHelp = SETUP_3D);

    void testHelpRotateButtonClicked();

    void updateContents();

public slots:

    void onSettingsChange();

};
