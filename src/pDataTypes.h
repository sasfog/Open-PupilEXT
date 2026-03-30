#pragma once

/**
    @author Gabor Benyei
*/

#include <QtCore/QObject>
#include <QtCore/QMap>
#include "pDataTypeEnum.h"

class PDataTypes /*: public QObject*/ {
    //Q_OBJECT

public:
    /*
    enum PDataType {
        TIME_RAW_TIMESTAMP,
        P_TIME,
        FRAME_NUMBER,
        CAMERA_FPS,
        PUPIL_FPS,
        PUPIL_CENTER_X,
        PUPIL_CENTER_Y,
        PUPIL_MAJOR,
        PUPIL_MINOR,
        PUPIL_WIDTH,
        PUPIL_HEIGHT,
        PUPIL_DIAMETER,
        PUPIL_UNDIST_DIAMETER,
        PUPIL_PHYSICAL_DIAMETER,
        PUPIL_CONFIDENCE,
        PUPIL_OUTLINE_CONFIDENCE,
        PUPIL_CIRCUMFERENCE,
        PUPIL_RATIO,
        PUPIL_ANGLE
    };
    */

    // the list of data rows to be displayed in data table, in the correct order
    static const std::vector<PDataType> dataTableRows;

    // the list of data fields ("columns") to be written to data output, e.g. CSV, or streamed in CSV, XML, JSON, YAML format or via LSL
    static const std::vector<PDataType> dataOutputFields;

    /*
     *
     * result = result % "diameter" % "_" % eyeIdentities[i] % "_" % camIdentities[i] % "_px" % delim;
        result = result % "undistortedDiameter" % "_" % eyeIdentities[i] % "_" % camIdentities[i] % "_px" % delim;
        result = result % "physicalDiameter" % "_" % eyeIdentities[i] % "_" % camIdentities[i] % "_mm" % delim;
        result = result % "width" % "_" % eyeIdentities[i] % "_" % camIdentities[i] % "_px" % delim;
        result = result % "height" % "_" % eyeIdentities[i] % "_" % camIdentities[i] % "_px" % delim;
        result = result % "axisRatio" % "_" % eyeIdentities[i] % "_" % camIdentities[i] % delim;
        result = result % "centerX" % "_" % eyeIdentities[i] % "_" % camIdentities[i] % "_px" % delim;
        result = result % "centerY" % "_" % eyeIdentities[i] % "_" % camIdentities[i] % "_px" % delim;
        result = result % "angle" % "_" % eyeIdentities[i] % "_" % camIdentities[i] % "_deg" % delim;
        result = result % "circumference" % "_" % eyeIdentities[i] % "_" % camIdentities[i] % "_px" % delim;
        result = result % "confidence"  % "_" % eyeIdentities[i] % "_" % camIdentities[i] % delim;
        result = result % "outlineConfidence" % "_" % eyeIdentities[i] % "_" % camIdentities[i] % delim;
     *
     */

    static std::map<PDataType, QString> tyf; // friendly names
    static std::map<PDataType, QString> tyn; // names for data columns
    static std::map<PDataType, QString> tytXDF; // names for data types for XDF compatible LSL streaming structure
    static std::map<PDataType, QString> tyd; // dimensions
};
