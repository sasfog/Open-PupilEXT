#pragma once

/**
    @author Gabor Benyei
*/

#include <QtCore/QObject>
#include <QByteArray>
#include <QSerialPort>
#include <QTextStream>
#include <QTimer>

#include <QSettings>
#include <QCoreApplication>

#include "supportFunctions.h"

#include <QtXml>
#include "pupilDetection.h"
#include "dataWriter.h"

#include "pDataTypes.h" // DEV

#include <QStringBuilder>

#include "lsl_cpp.h"

/**
    
    Transforms every pupildetection output into text, mostly for sending through dataStreamer, but also dataWriter uses the CSV version

*/
class EyeDataSerializer : public QObject {
    Q_OBJECT

public:

    static QString getHeaderCSV(const std::vector<char> &eyeIdentities, const std::vector<char> &camIdentities, QChar delim, DataWriterDataStyle dataStyle);
    static void addLSLChannelsInfo_XDF(lsl::stream_info *info,
                                                   const LSL_XDF_Eye lsl_xdf_Eye,
                                                   const LSL_XDF_Camera lsl_xdf_Camera,
                                                   const PDataType lsl_xdf_Diameter,
                                                   const PDataType lsl_xdf_Confidence);
    static void addLSLChannelsInfo_V1(const std::vector<char> &eyeIdentities, const std::vector<char> &camIdentities, lsl::stream_info *info);

    static std::vector<double> pupilToLSLsample_XDF(quint64 timestamp,
                                                    int procMode,
                                                    const std::vector<Pupil> &Pupils,
                                                    const LSL_XDF_Eye lsl_xdf_Eye,
                                                    const LSL_XDF_Camera lsl_xdf_Camera,
                                                    const PDataType lsl_xdf_Diameter,
                                                    const PDataType lsl_xdf_Confidence);
    static std::vector<double> pupilToLSLsample_V1(quint64 timestamp, const std::vector<Pupil> &Pupils);

    static QString pupilToRowCSV(quint64 timestamp, int procMode, const std::vector<Pupil> &Pupils, uint trialNum, QChar delim, DataWriterDataStyle dataStyle, const QString& message, const std::vector<double> &temperatures);
    static QString pupilToJSON(quint64 timestamp, int procMode, const std::vector<Pupil> &Pupils, uint trialNum, const QString& message, const std::vector<double> &temperatures);
    static QString pupilToXML(quint64 timestamp, int procMode, const std::vector<Pupil> &Pupils, uint trialNum, const QString& message, const std::vector<double> &temperatures);
    static QString pupilToYAML(quint64 timestamp, int procMode, const std::vector<Pupil> &Pupils, uint trialNum, const QString& message, const std::vector<double> &temperatures);

    static void populatePupilNodeJSON(quint64 &timestamp, QJsonObject &dObj, int idx, const std::vector<Pupil> &Pupils, uint &trialNum, const QString& message, double temperature);
    static void populatePupilNodeXML(quint64 &timestamp, QDomElement &dObj, int idx, const std::vector<Pupil> &Pupils, uint &trialNum, const QString& message, double temperature);
    static void populatePupilNodeYAML(quint64 &timestamp, QString &obj, ushort depth, int idx, const std::vector<Pupil> &Pupils, uint &trialNum, const QString& message,  double temperature);
    static void addRowYAML(QString &obj, QString key, QString value, ushort depth, bool isLeaf);

};

