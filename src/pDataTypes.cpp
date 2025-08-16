
#include "./pDataTypes.h"

std::map<PDataType, QString> PDataTypes::tyf = {
    {PDataType::TIME_RAW_TIMESTAMP,       "Timestamp"},
    {PDataType::P_TIME,                   "Time"},
    {PDataType::FRAME_NUMBER,             "Frame #"},
    {PDataType::CAMERA_FPS,               "Camera/Image read FPS"},
    {PDataType::PUPIL_FPS,                "Processing FPS"},
    {PDataType::PUPIL_CENTER_X,           "Pupil center x"},
    {PDataType::PUPIL_CENTER_Y,           "Pupil center y"},
    {PDataType::PUPIL_MAJOR,              "Pupil major axis"},
    {PDataType::PUPIL_MINOR,              "Pupil minor axis"},
    {PDataType::PUPIL_WIDTH,              "Pupil width"},
    {PDataType::PUPIL_HEIGHT,             "Pupil height"},
    {PDataType::PUPIL_DIAMETER,           "Pupil diameter"},
    {PDataType::PUPIL_UNDIST_DIAMETER,    "Pupil undistorted diameter"},
    {PDataType::PUPIL_PHYSICAL_DIAMETER,  "Pupil physical diameter"},
    {PDataType::PUPIL_CONFIDENCE,         "Pupil confidence"},
    {PDataType::PUPIL_OUTLINE_CONFIDENCE, "Pupil outline confidence"},
    {PDataType::PUPIL_CIRCUMFERENCE,      "Pupil circumference"},
    {PDataType::PUPIL_RATIO,              "Pupil axis ratio"}
};

std::map<PDataType, QString> PDataTypes::tyn = {
        {PDataType::TIME_RAW_TIMESTAMP,       "timestamp"},
        //{DataType::P_TIME,                     "time"},
        //{DataType::FRAME_NUMBER,             "frameNumber"},
        //{DataType::CAMERA_FPS,               "cameraFPS"},
        //{DataType::PUPIL_FPS,                "processingFPS"},
        {PDataType::PUPIL_CENTER_X,           "pupilCenterX"},
        {PDataType::PUPIL_CENTER_Y,           "pupilCenterY"},
        //{DataType::PUPIL_MAJOR,              "pupilMajorAxis"},
        //{DataType::PUPIL_MINOR,              "pupilMinorAxis"},
        {PDataType::PUPIL_WIDTH,              "pupilWidth"},
        {PDataType::PUPIL_HEIGHT,             "pupilHeight"},
        {PDataType::PUPIL_DIAMETER,           "pupilDiameter"},
        {PDataType::PUPIL_UNDIST_DIAMETER,    "pupilUndistortedDiameter"},
        {PDataType::PUPIL_PHYSICAL_DIAMETER,  "pupilPhysicalDiameter"},
        {PDataType::PUPIL_CONFIDENCE,         "pupilConfidence"},
        {PDataType::PUPIL_OUTLINE_CONFIDENCE, "pupilOutlineConfidence"},
        {PDataType::PUPIL_CIRCUMFERENCE,      "pupilCircumference"},
        {PDataType::PUPIL_RATIO,              "pupilAxisRatio"}
};

std::map<PDataType, QString> PDataTypes::tytXDF = {
        {PDataType::TIME_RAW_TIMESTAMP,       "timestamp"},
        //{DataType::P_TIME,                     ""},
        //{DataType::FRAME_NUMBER,             ""},
        //{DataType::CAMERA_FPS,               ""},
        //{DataType::PUPIL_FPS,                ""},
        {PDataType::PUPIL_CENTER_X,           "PupilX"},
        {PDataType::PUPIL_CENTER_Y,           "PupilY"},
        //{DataType::PUPIL_MAJOR,              "px"},
        //{DataType::PUPIL_MINOR,              "px"},
        {PDataType::PUPIL_WIDTH,              "DiameterX"},
        {PDataType::PUPIL_HEIGHT,             "DiameterY"},
        {PDataType::PUPIL_DIAMETER,           "Diameter"},
        {PDataType::PUPIL_UNDIST_DIAMETER,    "Diameter"},
        {PDataType::PUPIL_PHYSICAL_DIAMETER,  "Diameter"},
        {PDataType::PUPIL_CONFIDENCE,         "Confidence"},
        {PDataType::PUPIL_OUTLINE_CONFIDENCE, "Confidence"},
        {PDataType::PUPIL_CIRCUMFERENCE,      ""},
        {PDataType::PUPIL_RATIO,              ""}
};

std::map<PDataType, QString> PDataTypes::tyd = {
        {PDataType::TIME_RAW_TIMESTAMP,       "ms"},
        //{DataType::P_TIME,                     ""},
        //{DataType::FRAME_NUMBER,             ""},
        //{DataType::CAMERA_FPS,               ""},
        //{DataType::PUPIL_FPS,                ""},
        {PDataType::PUPIL_CENTER_X,           "px"},
        {PDataType::PUPIL_CENTER_Y,           "px"},
        //{DataType::PUPIL_MAJOR,              "px"},
        //{DataType::PUPIL_MINOR,              "px"},
        {PDataType::PUPIL_WIDTH,              "px"},
        {PDataType::PUPIL_HEIGHT,             "px"},
        {PDataType::PUPIL_DIAMETER,           "px"},
        {PDataType::PUPIL_UNDIST_DIAMETER,    "px"},
        {PDataType::PUPIL_PHYSICAL_DIAMETER,  "mm"},
        {PDataType::PUPIL_CONFIDENCE,         ""},
        {PDataType::PUPIL_OUTLINE_CONFIDENCE, ""},
        {PDataType::PUPIL_CIRCUMFERENCE,      "px"},
        {PDataType::PUPIL_RATIO,              ""}
};

const std::vector<PDataType> PDataTypes::dataTableRows = {
        PDataType::TIME_RAW_TIMESTAMP,
        PDataType::P_TIME,
        PDataType::FRAME_NUMBER,
        PDataType::CAMERA_FPS,
        PDataType::PUPIL_FPS,
        //
        PDataType::PUPIL_CENTER_X,
        PDataType::PUPIL_CENTER_Y,
        PDataType::PUPIL_MAJOR,
        PDataType::PUPIL_MINOR,
        PDataType::PUPIL_WIDTH,
        PDataType::PUPIL_HEIGHT,
        PDataType::PUPIL_DIAMETER,
        PDataType::PUPIL_UNDIST_DIAMETER,
        PDataType::PUPIL_PHYSICAL_DIAMETER,
        PDataType::PUPIL_CONFIDENCE,
        PDataType::PUPIL_OUTLINE_CONFIDENCE,
        PDataType::PUPIL_CIRCUMFERENCE,
        PDataType::PUPIL_RATIO
};

const std::vector<PDataType> PDataTypes::dataOutputFields = {
        //DataType::FRAME_NUMBER,
        PDataType::TIME_RAW_TIMESTAMP,
        PDataType::PUPIL_DIAMETER,
        PDataType::PUPIL_UNDIST_DIAMETER,
        PDataType::PUPIL_PHYSICAL_DIAMETER,
        PDataType::PUPIL_WIDTH,
        PDataType::PUPIL_HEIGHT,
        PDataType::PUPIL_RATIO,
        PDataType::PUPIL_CENTER_X,
        PDataType::PUPIL_CENTER_Y,
        PDataType::PUPIL_ANGLE,
        PDataType::PUPIL_CIRCUMFERENCE,
        PDataType::PUPIL_CONFIDENCE,
        PDataType::PUPIL_OUTLINE_CONFIDENCE
};


