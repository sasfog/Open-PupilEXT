#include <iostream>
#include "dataStreamer.h"

/**
    @author Gabor Benyei
*/

DataStreamer::DataStreamer(
    ConnPoolCOM *connPoolCOM,
    ConnPoolUDP *connPoolUDP,
    PupilDetection *pupilDetection,
    RecEventTracker *recEventTracker,
    QObject *parent
    ) : 
    QObject(parent),
    connPoolCOM(connPoolCOM),
    connPoolUDP(connPoolUDP),
    pupilDetection(pupilDetection),
    recEventTracker(recEventTracker),
    applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), parent))
{
    delim = applicationSettings->value("dataWriterDelimiter", ",").toString()[0];
    //delim = applicationSettings->value("delimiterToUse", ',').toChar(); // somehow this just doesnt work

    lsl_xdf_Eye = (LSL_XDF_Eye)applicationSettings->value("StreamingSettings.LSL.eye", LSL_XDF_Eye::XDF_LEFT).toInt();
    lsl_xdf_Camera = (LSL_XDF_Camera)applicationSettings->value("StreamingSettings.LSL.camera", LSL_XDF_Camera::XDF_MAIN).toInt();
    lsl_xdf_Diameter = (PDataType)applicationSettings->value("StreamingSettings.LSL.pupilData", PDataType::PUPIL_DIAMETER).toInt();
    lsl_xdf_Confidence = (PDataType)applicationSettings->value("StreamingSettings.LSL.confidence", PDataType::PUPIL_CONFIDENCE).toInt();
}

void DataStreamer::startUDPStreamer(int poolIndex, int srate, DataContainer dataContainer) {

    connPoolUDPIndex = poolIndex;
    UDPdataContainer = dataContainer;
    qDebug() << "Now starting UDP streaming to ip: " << connPoolUDP->getInstance(poolIndex)->objectName() << " and port " << connPoolUDP->getInstance(poolIndex)->localPort() << " using data container " << dataContainer;
    sampleRateDelayUDP = 1000 / srate;
    timerUDP.start();
}

void DataStreamer::startCOMStreamer(int poolIndex, int srate, DataContainer dataContainer) {

    connPoolCOMIndex = poolIndex;
    COMdataContainer = dataContainer;
    qDebug() << "Now starting COM streaming on poolIndex" << poolIndex << " of port: " << connPoolCOM->getInstance(poolIndex)->portName() << "using data container " << dataContainer;
    sampleRateDelayCOM = 1000 / srate;
    timerCOM.start();
}

void DataStreamer::startLSLStreamer(int srate, DataContainer dataContainer, ProcMode procMode) {

    LSLdataContainer = dataContainer;

    std::string name = "NAME";
    std::string type = "TYPE";
    int n_channels = 0;
    int max_buffered = 360;

    if(dataContainer == DataStreamer::DataContainer::LSL_XDF) {
        n_channels = 7;
        name = "PupilEXT";
        //type = "Pupil";
        type = "Gaze";
    } else if(dataContainer == DataStreamer::DataContainer::LSL_V1) {
        name = "PupilEXT";
        //type = "Pupil";
        type = "EyeData";
        //type = "EyeDataFFT";

        switch((ProcMode)procMode) {
            case ProcMode::SINGLE_IMAGE_ONE_PUPIL:
                n_channels = 13;
                break;
            case ProcMode::SINGLE_IMAGE_TWO_PUPIL:
                n_channels = 25;
                break;
            case ProcMode::STEREO_IMAGE_ONE_PUPIL:
                n_channels = 25;
                break;
            case ProcMode::STEREO_IMAGE_TWO_PUPIL:
                n_channels = 48;
                break;
            // case ProcMode::MIRR_IMAGE_ONE_PUPIL:
            //     //break;
            default:
                throw std::runtime_error("Proc Mode undetermined at LSL streaming start");
            //    n_channels = 0;
        }
    } else {
        throw std::runtime_error("Data Container undetermined at LSL streaming start");
    }


    lsl::stream_info *info = new lsl::stream_info(
            name, type, n_channels, srate, lsl::cf_double64, std::string(name) += type);

    // TODO:
    //  add some description fields
    //info.desc().append_child_value("manufacturer", "LSL");

    lsl::xml_element acqs = info->desc().append_child("acquisition")
            .append_child_value("manufacturer", "hahaha")
            .append_child_value("model", "hahaha")
            .append_child_value("serialnumber", type);

    // <label>
    // # label of the channel
    // <eye>
    // # which eye the channel is referring to (can be left, right, or both)
    // <type>
    // # type of data in this channel, can be an of the following values:
    // # ** ScreenX, ScreenY: screen coordinates of the gaze cursor (can also refer to a scene image), usually in pixels,
    // # ** DirectionX,DirectionY,DirectionZ: 3d gaze vector in some coordinate system
    // # ** PositionX,PositionY,PositionZ: 3d position of the eye center in some coordinate system
    // # ** IntersectionX, IntersectionY, IntersectionZ: 2d or 3d position of the intersection point with a plane (in some coordinate system)
    // # ** HeadX, HeadY, HeadZ: 3d location of the head center in some coordinate system,
    // # ** PupilX, PupilY, PupilZ: 2d or 3d location of the pupil center in some coordinate system,
    // # ** ReflexX, ReflexY, ReflexZ: 2d or 3d location of the illuminator's reflection point in some coordinate system,
    // # ** Radius or Diameter: the overall pupil radius or diameter (usually in mm or pixels),
    // # ** RadiusX,RadiusY: horizontal and vertical pupil radius
    // # ** DiameterX,DiameterY: horizontal and vertical pupil diameter
    // # ** Confidence for confidence information (preferred unit: normalized)
    // # ** FrameNumber: frame number that the parameters were calculated from
    // # * PlaneNumber or ObjectId: number or identifier of the object that was intersected by the gaze vector
    // <unit>
    // # measurement unit (e.g., pixels, mm, normalized)

    if(dataContainer == DataStreamer::DataContainer::LSL_XDF) {
        EyeDataSerializer::addLSLChannelsInfo_XDF(
                info,
                lsl_xdf_Eye,
                lsl_xdf_Camera,
                lsl_xdf_Diameter,
                lsl_xdf_Confidence );
    } else if(dataContainer == DataStreamer::DataContainer::LSL_V1) {
        EyeDataSerializer::addLSLChannelsInfo_V1(
                pupilDetection->getEyeIdentities(),
                pupilDetection->getCamIdentities(),
                info );
    }

    LSLOutlet = new lsl::stream_outlet(*info, 0, max_buffered);

//    std::vector<float> sample(n_channels, 0.0);


    qDebug() << "Now starting LSL streaming";
    sampleRateDelayLSL = 1000 / srate;
    timerLSL.start();
}
    
void DataStreamer::stopUDPStreamer() {
    connPoolUDPIndex = -1;
    UDPdataContainer = DataContainer::CSV;
    qDebug() << "Stopping UDP streaming";
}

void DataStreamer::stopCOMStreamer() {
    connPoolCOMIndex = -1;
    COMdataContainer = DataStreamer::CSV;
    qDebug() << "Stopping COM streaming";
}

void DataStreamer::stopLSLStreamer() {

    // TODO
    delete LSLOutlet;
    LSLOutlet = nullptr;

    qDebug() << "Stopping LSL streaming";
}

// On new pupil data, stream it
void DataStreamer::newPupilData(quint64 timestamp, int procMode, const std::vector<Pupil> &Pupils) {

    _trialNumber = recEventTracker->getTrialIncrement(timestamp).trialNumber;
    _message = recEventTracker->getMessage(timestamp).messageString;
    _d = recEventTracker->getTemperatureCheck(timestamp).temperatures;

    bool anyUsed = false;
    if(connPoolUDPIndex >= 0 && connPoolUDP->getInstance(connPoolUDPIndex) != nullptr &&
        timerUDP.elapsed() >= sampleRateDelayUDP ) {
    //if( UDPStreamingOn && UDPsocket != nullptr) {
        QString str = "";
        if(UDPdataContainer == DataContainer::CSV)
            str = EyeDataSerializer::pupilToRowCSV(timestamp, procMode, Pupils, _trialNumber, delim, DataWriterDataStyle::DATASTYLE_V3, _message, _d);
        else if(UDPdataContainer == DataContainer::JSON)
            str = EyeDataSerializer::pupilToJSON(timestamp, procMode, Pupils, _trialNumber, _message, _d);
        else if(UDPdataContainer == DataContainer::XML)
            str = EyeDataSerializer::pupilToXML(timestamp, procMode, Pupils, _trialNumber, _message, _d);
        else if(UDPdataContainer == DataContainer::YAML)
            str = EyeDataSerializer::pupilToYAML(timestamp, procMode, Pupils, _trialNumber, _message, _d);

        connPoolUDP->writeToInstance( connPoolUDPIndex, (str + '\n').toUtf8() );
        timerUDP.start();
        anyUsed = true;
    }
    if(connPoolCOMIndex >= 0 && connPoolCOM->getInstance(connPoolCOMIndex) != nullptr &&
        timerCOM.elapsed() >= sampleRateDelayCOM) {

        QString str = "";
        if(COMdataContainer == DataContainer::CSV)
            str = EyeDataSerializer::pupilToRowCSV(timestamp, procMode, Pupils, _trialNumber, delim, DataWriterDataStyle::DATASTYLE_V3, _message, _d);
        else if(COMdataContainer == DataContainer::JSON)
            str = EyeDataSerializer::pupilToJSON(timestamp, procMode, Pupils, _trialNumber, _message, _d);
        else if(COMdataContainer == DataContainer::XML)
            str = EyeDataSerializer::pupilToXML(timestamp, procMode, Pupils, _trialNumber, _message, _d);
        else if(COMdataContainer == DataContainer::YAML)
            str = EyeDataSerializer::pupilToYAML(timestamp, procMode, Pupils, _trialNumber, _message, _d);

        connPoolCOM->writeToInstance( connPoolCOMIndex, (str + '\n').toUtf8() );
        timerCOM.start();
        anyUsed = true;
    }
    if(LSLOutlet != nullptr &&
       timerLSL.elapsed() >= sampleRateDelayLSL) {

        std::vector<double> sample;
        if(LSLdataContainer == DataContainer::LSL_XDF)
            sample = EyeDataSerializer::pupilToLSLsample_XDF(timestamp,
                procMode,
                Pupils,
                lsl_xdf_Eye,
                lsl_xdf_Camera,
                lsl_xdf_Diameter,
                lsl_xdf_Confidence);
        else if(LSLdataContainer == DataContainer::LSL_V1)
            //str = EyeDataSerializer::pupilToJSON(timestamp, procMode, Pupils, filename, _trialNumber, _message, _d);
            sample = EyeDataSerializer::pupilToLSLsample_V1(timestamp, Pupils);

        LSLOutlet->push_sample(sample);
        // TODO: EVENTS also

        timerLSL.start();
        anyUsed = true;
    }

    if(!anyUsed) { // This is just for extra safety
        qDebug() << "Streamers are not in use, stopping all.";
        //emit underlyingConnectionsClosed();
        stopUDPStreamer();
        stopCOMStreamer();
        stopLSLStreamer();
    }
}

int DataStreamer::getNumActiveStreamers() {
    int num = 0;
    if(connPoolUDPIndex >= 0 && connPoolUDP->getInstance(connPoolUDPIndex) != nullptr) {
        num++;
    }
    if(connPoolCOMIndex >= 0 && connPoolCOM->getInstance(connPoolCOMIndex) != nullptr) {
        num++;
    }
    return num;
}

DataStreamer::~DataStreamer() {
    close();
}

// Close the files, filestreams, etc
void DataStreamer::close() {
    
    /*
    if(streamingMethod == StreamingMethod::COM && serialPort != nullptr && serialPort->isOpen()) 
        serialPort->close();
    if(streamingMethod == StreamingMethod::UDP && socket != nullptr && socket->isOpen()) 
        socket->close();
    */
}
