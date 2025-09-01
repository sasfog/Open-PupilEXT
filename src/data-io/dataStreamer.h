#pragma once

/**
    @author Gabor Benyei
*/

#include <QtCore/QObject>
//#include <QtCore/QFile>
//#include <QtCore/QTextStream>
//
#include <QByteArray>
#include <QSerialPort>
#include <QTextStream>
#include <QTimer>

#include <QSettings>
#include <QCoreApplication>

#include "../supportFunctions.h"
#include "../pDataTypes.h"

#include <QtXml>
#include "../pupilDetection.h"

#include "../recEventTracker.h"
#include "eyeDataSerializer.h"
#include "connPoolCOM.h"
#include "connPoolUDP.h"

#ifdef USE_LSL
#include "lsl_cpp.h"
#endif

// TODO: make a kind of HTTP "REST API-like" thing with only a few accepted requests, 
// returning e.g. the last read pupil data? Could be fun :)
/**

    Code made to handle an std::vector of Pupils, in order to comply with new signal-slot strategy, which
    I introduced to manage different pupil detection processing modes (procModes).
    Also it uses EyeDataSerializer class to process every pupil detection output.

*/
class DataStreamer : public QObject {
    Q_OBJECT

public:

    // NOTE: LSL does not have one of these, as that is "not" a serialized type of streaming in our context
    //  it just runs or not, in its own format. Might be configurable later for data style, but yet it has one
    //  simple style.
    enum DataContainer {UNDEFINED = 0, CSV = 1, JSON = 2, XML = 3, YAML = 4, LSL_XDF = 5, LSL_V1 = 6};

    explicit DataStreamer(
        ConnPoolCOM *connPoolCOM,
        ConnPoolUDP *connPoolUDP,
        PupilDetection *pupilDetection,
        RecEventTracker *recEventTracker,
        QObject *parent
        ); 
    ~DataStreamer() override;
    void close();

//    void startUDPStreamer(QUdpSocket *socket, QHostAddress ip, quint16 port, DataContainer dataContainer);
    void startUDPStreamer(int poolIndex, int srate, DataContainer dataContainer);
    void startCOMStreamer(int poolIndex, int srate, DataContainer dataContainer);
    
    void stopUDPStreamer();
    void stopCOMStreamer();

#ifdef USE_LSL
    void startLSLStreamer(int srate, DataContainer dataContainer, ProcMode procMode);
    void stopLSLStreamer();
#endif

    int getNumActiveStreamers();

private:

    ConnPoolCOM *connPoolCOM;
    int connPoolCOMIndex = -1;

    ConnPoolUDP *connPoolUDP;
    int connPoolUDPIndex = -1;

#ifdef USE_LSL
    lsl::stream_outlet* LSLOutlet = nullptr;
    DataContainer LSLdataContainer;
    QElapsedTimer timerLSL;
    int sampleRateDelayLSL;
    LSL_XDF_Eye lsl_xdf_Eye = XDF_LEFT;
    LSL_XDF_Camera lsl_xdf_Camera = XDF_MAIN;
    PDataType lsl_xdf_Diameter = PDataType::PUPIL_DIAMETER;
    PDataType lsl_xdf_Confidence = PDataType::PUPIL_CONFIDENCE;
#endif

    PupilDetection *pupilDetection;
    
//    bool UDPStreamingOn = false;
//    QUdpSocket *UDPsocket;
//    QHostAddress UDPip;
//    quint16 UDPport;

    DataContainer UDPdataContainer;
    DataContainer COMdataContainer;

    QElapsedTimer timerUDP;
    QElapsedTimer timerCOM;
    int sampleRateDelayUDP;
    int sampleRateDelayCOM;

    QSettings *applicationSettings;
    QChar delim; 
    RecEventTracker *recEventTracker;


    uint _trialNumber = 1;
    QString _message = "";
    std::vector<double> _d = {-1.0,-1.0};

public slots:

    void newPupilData(quint64 timestamp, int procMode, const std::vector<Pupil> &Pupils);

signals:
    //void underlyingConnectionsClosed(); // TODO: use for safety

};



