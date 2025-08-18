#pragma once

/**
    @author Gabor Benyei
*/

#include <QDialog>
#include <QtWidgets/QComboBox>
#include <QtCore>
#include <QtWidgets/QPushButton>

#include <QtCore/QSettings>

#include <QSerialPort>
#include <QSerialPortInfo>

#include "custom-widgets/IPCtrl.h"
#include "../connPoolCOM.h"
#include "../connPoolUDP.h"

#include "../pupilDetection.h"
#include "../dataStreamer.h"

/**
    In this dialog the user can specify the means of streaming pupil detection output to another machine.
    Some of the code regarding serial connection was copied from MCUSettingsDialogInst.

    NOTE: used several parts of the code from former serialSettingsDialog made by ML
*/
class StreamingSettingsDialog : public QDialog {
    Q_OBJECT

public:

    //explicit StreamingSettingsDialog(ConnPoolUDP *connPoolUdp, ConnPoolCOM *connPoolCOM, QWidget *parent = nullptr);
    explicit StreamingSettingsDialog(
        ConnPoolCOM *connPoolCOM,
        ConnPoolUDP *connPoolUDP,
        PupilDetection *pupilDetection,
        //DataStreamer *dataStreamer,
        QWidget *parent = nullptr);

    ~StreamingSettingsDialog() override;

    //void closeEvent(QCloseEvent *);

    int getConnPoolUDPIndex();
    int getConnPoolCOMIndex();
    DataStreamer::DataContainer getDataContainerUDP();
    DataStreamer::DataContainer getDataContainerCOM();
#ifdef USE_LSL
    DataStreamer::DataContainer getDataContainerLSL();
#endif

private:

    ConnPoolUDP *connPoolUDP;
    int connPoolUDPIndex = -1;

    ConnPoolCOM *connPoolCOM;
    int connPoolCOMIndex = -1;

    PupilDetection *pupilDetection;
    //DataStreamer *dataStreamer;


    void createForm();
    void connectSignals();

    QSettings *applicationSettings;

    QLabel *udpSampleRateLabel;
    QLabel *udpIpLabel;
    QLabel *udpPortLabel;

    QSpinBox *udpSampleRateBox;
    IPCtrl *udpIpBox;
    QSpinBox *udpPortBox;

    QLabel *dataContainerUDPLabel;
    QComboBox *dataContainerUDPBox;

    ConnPoolUDPInstanceSettings m_currentSettingsUDP;
    ConnPoolCOMInstanceSettings m_currentSettingsCOM;

    QGroupBox *udpGroup;
    QGroupBox *comGroup;

    QPushButton *refreshButton;

    QLabel *comSampleRateLabel;
    QLabel *comPortLabel;
    QLabel *baudRateLabel;
    QLabel *dataBitsLabel;
    QLabel *parityLabel;
    QLabel *stopBitsLabel;
    QLabel *flowControlLabel;

    QComboBox *dataContainerCOMBox;
    QLabel *dataContainerCOMLabel;

    QSpinBox *comSampleRateBox;
    QComboBox *serialPortInfoListBox;
    QComboBox *baudRateBox;
    QComboBox *dataBitsBox;
    QComboBox *flowControlBox;
    QComboBox *parityBox;
    QComboBox *stopBitsBox;

    // NOTE leave it here, it holds a note for the user
    QGroupBox *lslGroup;
#ifdef USE_LSL
    bool LSLconnected = false;

    QLabel *lslSampleRateLabel;
    QSpinBox *lslSampleRateBox;
    QComboBox *dataContainerLSLBox;
    QLabel *dataContainerLSLLabel;

    QWidget *lslRestrictiveOptionsSectionW;
    QLabel *specXDFeyeLabel;
    QComboBox *specXDFeyeBox;
    QLabel *specXDFcameraLabel;
    QComboBox *specXDFcameraBox;
    QLabel *specXDFpupDataLabel;
    QComboBox *specXDFpupDataBox;
    QLabel *specXDFconfLabel;
    QComboBox *specXDFconfBox;

    QPushButton *connectLSLButton;
    QPushButton *disconnectLSLButton;
#endif

    QPushButton *connectUDPButton;
    QPushButton *disconnectUDPButton;
    QPushButton *connectCOMButton;
    QPushButton *disconnectCOMButton;

    void fillCOMParameters();

    void updateSettings();

    void saveSettings();
    void loadSettings();

private slots:
    void updateCOMDevices();

    // These are reserved for possible later features, e.g. stream-on-demand
    //void readData(const QString &msg);
    //void interpretCommand(const QString &msg);

public slots:
    bool isAnyConnected();
    bool isUDPConnected();
    bool isCOMConnected();

#ifdef USE_LSL
    bool isLSLConnected(); // Sounds strange, but this is by far more clean in the coding point of view
    void onConnectLSLClick();
    void disconnectLSL();
    void connectLSL();
    void saveLSLSettings();
    void setLimitationsWhileConnectedLSL(bool state);
    void setLimitationsWhileStreamingLSL(bool state);
#endif

    void onConnectUDPClick();
    void disconnectUDP();
    void onConnectCOMClick();
    void disconnectCOM();

    void connectUDP(const ConnPoolUDPInstanceSettings &p);
    void connectCOM(const ConnPoolCOMInstanceSettings &p);

    void saveUDPSettings();
    void saveCOMSettings();

    void setLimitationsWhileConnectedUDP(bool state);  
    void setLimitationsWhileStreamingUDP(bool state);
    void setLimitationsWhileConnectedCOM(bool state);
    void setLimitationsWhileStreamingCOM(bool state);
    void setLimitationsWhileStreamingAny(bool state);

    //void setLimitationsWhileStreaming(bool state);

signals:
    void onUDPConnect();
    void onUDPDisconnect();
    void onCOMConnect();
    void onCOMDisconnect();
#ifdef USE_LSL
    void onLSLConnect();
    void onLSLDisconnect();
#endif
    //void onConnStateChanged();

};

