
#include "eyeDataSerializer.h"

void EyeDataSerializer::populatePupilNodeXML(quint64 &timestamp, QDomElement &dObj, int idx, const std::vector<Pupil> &Pupils, uint &trialNum, const QString& message, double temperature) {

    for(auto v : PDataTypes::dataOutputFields) {
        QString ds = "";
        if(!PDataTypes::tyd.at(v).isEmpty()) {
            ds = "_" + PDataTypes::tyd.at(v);
        }

        // TODO DEV KISZEDNI AMINT A PUPILLAL EGYÜTT KÖZVETíTETTÉ VÁLIK A TIMESTAMP A STRUCTON ÁT
        if(v == PDataType::TIME_RAW_TIMESTAMP)
            dObj.setAttribute("timestamp_ms", QString::number(timestamp));
        else
            dObj.setAttribute(PDataTypes::tyn.at(v) + ds, QString::number(Pupils[idx].getPData(v)));
    }

    //dObj.setAttribute("timestamp_ms", QString::number(timestamp));
    ////dObj.setAttribute("algorithm", QString::fromStdString(Pupils[idx].algorithmName));
    //dObj.setAttribute("diameter_px", QString::number(Pupils[idx].diameter()));
    //dObj.setAttribute("undistortedDiameter_px", QString::number(Pupils[idx].undistortedDiameter) );
    //dObj.setAttribute("physicalDiameter_mm", QString::number(Pupils[idx].physicalDiameter));
    //dObj.setAttribute("width_px", QString::number(Pupils[idx].width()));
    //dObj.setAttribute("height_px", QString::number(Pupils[idx].height()));
    //dObj.setAttribute("axisRatio_px", QString::number((double)Pupils[idx].width() / Pupils[idx].height()));
    //dObj.setAttribute("centerX_px", QString::number(Pupils[idx].center.x));
    //dObj.setAttribute("centerY_px", QString::number(Pupils[idx].center.y));
    //dObj.setAttribute("angle_deg", QString::number(Pupils[idx].angle));
    //dObj.setAttribute("circumference_px", QString::number(Pupils[idx].circumference()));
    //dObj.setAttribute("confidence", QString::number(Pupils[idx].confidence));
    //dObj.setAttribute("outlineConfidence", QString::number(Pupils[idx].outline_confidence));

    dObj.setAttribute("trial", QString::number(trialNum));
    dObj.setAttribute("message", message);
    dObj.setAttribute("cameraTemperature_c", QString::number(temperature));
}

QString EyeDataSerializer::pupilToXML(quint64 timestamp, int procMode, const std::vector<Pupil> &Pupils, uint trialNum, const QString& message, const std::vector<double> &temperatures) {

    QDomDocument document;
    QDomElement root = document.createElement("EyeData");
    document.appendChild(root);
    
    QDomElement pupilObjA;
    QDomElement pupilObjB;
    QDomElement viewObjAMain;
    QDomElement viewObjASec;
    QDomElement viewObjBMain;
    QDomElement viewObjBSec;

    switch((ProcMode)procMode) {
        case ProcMode::SINGLE_IMAGE_ONE_PUPIL:
            pupilObjA = document.createElement({Pupils[SINGLE_IMAGE_ONE_PUPIL_MAIN].eyeIdentity});
            root.appendChild(pupilObjA);
            viewObjAMain = document.createElement("Main");
            populatePupilNodeXML(timestamp, viewObjAMain, SINGLE_IMAGE_ONE_PUPIL_MAIN, Pupils, trialNum, message, temperatures[0]);
            pupilObjA.appendChild(viewObjAMain);
            break;
        case ProcMode::SINGLE_IMAGE_TWO_PUPIL:
            pupilObjA = document.createElement({Pupils[SINGLE_IMAGE_TWO_PUPIL_R].eyeIdentity});
            pupilObjB = document.createElement({Pupils[SINGLE_IMAGE_TWO_PUPIL_L].eyeIdentity});
            root.appendChild(pupilObjA);
            root.appendChild(pupilObjB);
            viewObjAMain = document.createElement("Main");
            viewObjBMain = document.createElement("Main");
            populatePupilNodeXML(timestamp, viewObjAMain, SINGLE_IMAGE_TWO_PUPIL_R, Pupils, trialNum, message, temperatures[0]);
            populatePupilNodeXML(timestamp, viewObjBMain, SINGLE_IMAGE_TWO_PUPIL_L, Pupils, trialNum, message, temperatures[0]);
            pupilObjA.appendChild(viewObjAMain);
            pupilObjB.appendChild(viewObjBMain);
            break;
        case ProcMode::STEREO_IMAGE_ONE_PUPIL:
            pupilObjA = document.createElement({Pupils[STEREO_IMAGE_ONE_PUPIL_MAIN].eyeIdentity});
            root.appendChild(pupilObjA);
            viewObjAMain = document.createElement("Main");
            viewObjASec = document.createElement("Sec");
            populatePupilNodeXML(timestamp, viewObjAMain, STEREO_IMAGE_ONE_PUPIL_MAIN, Pupils, trialNum, message, temperatures[0]);
            populatePupilNodeXML(timestamp, viewObjASec, STEREO_IMAGE_ONE_PUPIL_SEC, Pupils, trialNum, message, temperatures[1]);
            pupilObjA.appendChild(viewObjAMain);
            pupilObjA.appendChild(viewObjASec);
            break;
        case ProcMode::STEREO_IMAGE_TWO_PUPIL:
            pupilObjA = document.createElement({Pupils[STEREO_IMAGE_TWO_PUPIL_R_MAIN].eyeIdentity});
            pupilObjB = document.createElement({Pupils[STEREO_IMAGE_TWO_PUPIL_L_MAIN].eyeIdentity});
            root.appendChild(pupilObjA);
            root.appendChild(pupilObjB);
            viewObjAMain = document.createElement("Main");
            viewObjASec = document.createElement("Sec");
            viewObjBMain = document.createElement("Main");
            viewObjBSec = document.createElement("Sec");
            populatePupilNodeXML(timestamp, viewObjAMain, STEREO_IMAGE_TWO_PUPIL_R_MAIN, Pupils, trialNum, message, temperatures[0]);
            populatePupilNodeXML(timestamp, viewObjASec, STEREO_IMAGE_TWO_PUPIL_R_SEC, Pupils, trialNum, message, temperatures[1]);
            populatePupilNodeXML(timestamp, viewObjBMain, STEREO_IMAGE_TWO_PUPIL_L_MAIN, Pupils, trialNum, message, temperatures[0]);
            populatePupilNodeXML(timestamp, viewObjBSec, STEREO_IMAGE_TWO_PUPIL_L_SEC, Pupils, trialNum, message, temperatures[1]);
            pupilObjA.appendChild(viewObjAMain);
            pupilObjA.appendChild(viewObjASec);
            pupilObjB.appendChild(viewObjBMain);
            pupilObjB.appendChild(viewObjBSec);
            break;
        // case ProcMode::MIRR_IMAGE_ONE_PUPIL:
        //     pupilObjA = document.createElement("A");
        //     root.appendChild(pupilObjA);
        //     viewObjAMain = document.createElement("Main");
        //     viewObjASec = document.createElement("Sec");
        //     populatePupilNodeXML(timestamp, viewObjAMain, MIRR_IMAGE_ONE_PUPIL_MAIN, Pupils, trialNum, temperatures[0], message);
        //     populatePupilNodeXML(timestamp, viewObjASec, MIRR_IMAGE_ONE_PUPIL_SEC, Pupils, trialNum, temperatures[0], message);
        //     pupilObjA.appendChild(viewObjAMain);
        //     pupilObjA.appendChild(viewObjASec);
        //     break;

        //default:
            //break;
    }

    return document.toString();
}


void EyeDataSerializer::populatePupilNodeJSON(quint64 &timestamp, QJsonObject &dObj, int idx, const std::vector<Pupil> &Pupils, uint &trialNum, const QString& message, double temperature) {

    for(auto v : PDataTypes::dataOutputFields) {
        QString ds = "";
        if(!PDataTypes::tyd.at(v).isEmpty()) {
            ds = "_" + PDataTypes::tyd.at(v);
        }

        // TODO DEV KISZEDNI AMINT A PUPILLAL EGYÜTT KÖZVETíTETTÉ VÁLIK A TIMESTAMP A STRUCTON ÁT
        if(v == PDataType::TIME_RAW_TIMESTAMP)
            dObj["timestamp_ms"] = QString::number(timestamp);
        else
            dObj[PDataTypes::tyn.at(v) + ds] = QString::number(Pupils[idx].getPData(v));
    }

    //dObj["timestamp_ms"] = QString::number(timestamp);
    ////dObj["algorithm"] = QString::fromStdString(Pupils[idx].algorithmName);
    //dObj["diameter_px"] = QString::number(Pupils[idx].diameter());
    //dObj["undistortedDiameter_px"] = QString::number(Pupils[idx].undistortedDiameter) ;
    //dObj["physicalDiameter_mm"] = QString::number(Pupils[idx].physicalDiameter);
    //dObj["width_px"] = QString::number(Pupils[idx].width());
    //dObj["height_px"] = QString::number(Pupils[idx].height());
    //dObj["axisRatio_px"] = QString::number((double)Pupils[idx].width() / Pupils[idx].height());
    //dObj["centerX_px"] = QString::number(Pupils[idx].center.x);
    //dObj["centerY_px"] = QString::number(Pupils[idx].center.y);
    //dObj["angle_deg"] = QString::number(Pupils[idx].angle);
    //dObj["circumference_px"] = QString::number(Pupils[idx].circumference());
    //dObj["confidence"] = QString::number(Pupils[idx].confidence);
    //dObj["outlineConfidence"] = QString::number(Pupils[idx].outline_confidence);

    dObj["trial"] = QString::number(trialNum);
    dObj["message"] = message;
    dObj["cameraTemperature_c"] = QString::number(temperature);
}

QString EyeDataSerializer::pupilToJSON(quint64 timestamp, int procMode, const std::vector<Pupil> &Pupils, uint trialNum, const QString& message, const std::vector<double> &temperatures) {

    QJsonObject root;
    
    QJsonObject pupilObjA;
    QJsonObject pupilObjB;
    QJsonObject viewObjAMain;
    QJsonObject viewObjASec;
    QJsonObject viewObjBMain;
    QJsonObject viewObjBSec;

    switch((ProcMode)procMode) {
        case ProcMode::SINGLE_IMAGE_ONE_PUPIL:
            populatePupilNodeJSON(timestamp, viewObjAMain, SINGLE_IMAGE_ONE_PUPIL_MAIN, Pupils, trialNum, message, temperatures[0]);
            pupilObjA["Main"] = viewObjAMain;
            root[{Pupils[SINGLE_IMAGE_ONE_PUPIL_MAIN].eyeIdentity}] = pupilObjA;
            break;
        case ProcMode::SINGLE_IMAGE_TWO_PUPIL:
            populatePupilNodeJSON(timestamp, viewObjAMain, SINGLE_IMAGE_TWO_PUPIL_R, Pupils, trialNum, message, temperatures[0]);
            populatePupilNodeJSON(timestamp, viewObjBMain, SINGLE_IMAGE_TWO_PUPIL_L, Pupils, trialNum, message, temperatures[0]);
            pupilObjA["Main"] = viewObjAMain;
            root[{Pupils[SINGLE_IMAGE_TWO_PUPIL_R].eyeIdentity}] = pupilObjA;
            pupilObjB["Main"] = viewObjBMain;
            root[{Pupils[SINGLE_IMAGE_TWO_PUPIL_L].eyeIdentity}] = pupilObjB;
            break;
        case ProcMode::STEREO_IMAGE_ONE_PUPIL:
            populatePupilNodeJSON(timestamp, viewObjAMain, STEREO_IMAGE_ONE_PUPIL_MAIN, Pupils, trialNum, message, temperatures[0]);
            populatePupilNodeJSON(timestamp, viewObjASec, STEREO_IMAGE_ONE_PUPIL_SEC, Pupils, trialNum, message, temperatures[1]);
            pupilObjA["Main"] = viewObjAMain;
            pupilObjA["Sec"] = viewObjASec;
            root[{Pupils[STEREO_IMAGE_ONE_PUPIL_MAIN].eyeIdentity}] = pupilObjA;
            break;
        case ProcMode::STEREO_IMAGE_TWO_PUPIL:
            populatePupilNodeJSON(timestamp, viewObjAMain, STEREO_IMAGE_TWO_PUPIL_R_MAIN, Pupils, trialNum, message, temperatures[0]);
            populatePupilNodeJSON(timestamp, viewObjASec, STEREO_IMAGE_TWO_PUPIL_R_SEC, Pupils, trialNum, message, temperatures[1]);
            populatePupilNodeJSON(timestamp, viewObjBMain, STEREO_IMAGE_TWO_PUPIL_L_MAIN, Pupils, trialNum, message, temperatures[0]);
            populatePupilNodeJSON(timestamp, viewObjBSec, STEREO_IMAGE_TWO_PUPIL_L_SEC, Pupils, trialNum, message, temperatures[1]);
            pupilObjA["Main"] = viewObjAMain;
            pupilObjA["Sec"] = viewObjASec;
            root[{Pupils[STEREO_IMAGE_TWO_PUPIL_R_MAIN].eyeIdentity}] = pupilObjA;
            pupilObjB["Main"] = viewObjBMain;
            pupilObjB["Sec"] = viewObjBSec;
            root[{Pupils[STEREO_IMAGE_TWO_PUPIL_L_MAIN].eyeIdentity}] = pupilObjB;
            break;
        // case ProcMode::MIRR_IMAGE_ONE_PUPIL:
        //     populatePupilNodeJSON(timestamp, viewObjAMain, MIRR_IMAGE_ONE_PUPIL_MAIN, Pupils, trialNum, temperatures[0], message);
        //     populatePupilNodeJSON(timestamp, viewObjASec, MIRR_IMAGE_ONE_PUPIL_SEC, Pupils, trialNum, temperatures[0], message);
        //     pupilObjA["Main"] = viewObjAMain;
        //     pupilObjA["Sec"] = viewObjASec;
        //     root["A"] = pupilObjA;
        //     break;
        
        //default:
            //break;
    }

    QJsonObject dObj;
    dObj["EyeData"] = root;
    QJsonDocument dDoc(dObj);
    QByteArray content = dDoc.toJson();
    return QString(content);
}

QString EyeDataSerializer::getHeaderCSV(const std::vector<QChar> &eyeIdentities, const std::vector<QChar> &camIdentities, QChar delim, DataWriterDataStyle dataStyle) {

    QString result; // TODO: .reserve() ?

    //result = result % "timestamp_ms" % delim;
    //result = result % "algorithm" % delim;

    // NOTE: physicalDiameter will be duplicated redundantly. But it does not matter, this way data is much more self explanatory
    bool isTimestampAlreadyAdded = false;
    for(int i = 0; i < eyeIdentities.size(); i++) {
        for(auto v : PDataTypes::dataOutputFields) {
            QString ds = "";
            if(!PDataTypes::tyd.at(v).isEmpty()) {
                ds = "_" + PDataTypes::tyd.at(v);
            }

            // TODO DEV KISZEDNI AMINT A PUPILLAL EGYÜTT KÖZVETíTETTÉ VÁLIK A TIMESTAMP A STRUCTON ÁT
            if(v == PDataType::TIME_RAW_TIMESTAMP) {
                if(!isTimestampAlreadyAdded) {
                    result = result % "timestamp_ms" % delim;
                    isTimestampAlreadyAdded = true;
                }
            } else
                result = result % PDataTypes::tyn.at(v) % ds % "_" % eyeIdentities[i] % "_" % camIdentities[i] % delim;
        }
    }

    result = result % "trial" % delim;
    result = result % "message" % delim;
    result = result % "cameraTemperature_c_M" % delim;
    result = result % "cameraTemperature_c_S";

    return result;
}

#ifdef USE_LSL
void EyeDataSerializer::addLSLChannelsInfo_XDF(lsl::stream_info *info,
                                   const LSL_XDF_Eye lsl_xdf_Eye,
                                   const LSL_XDF_Camera lsl_xdf_Camera,
                                   const PDataType lsl_xdf_Diameter,
                                   const PDataType lsl_xdf_Confidence) {

    lsl::xml_element chns = info->desc().append_child("channels");

    std::string eyeStr = (lsl_xdf_Eye == LSL_XDF_Eye::XDF_RIGHT) ? "right" : "left";

    // TODO NOTE: if the specified eye does not match with the eye identity in case of a one-eye recording,
    //  data will still be from that one available eye. Inform user?

    //result = result % QString::fromStdString(Pupils[SINGLE_IMAGE_ONE_PUPIL_MAIN].algorithmName);
    //result = result % message;

    // common, but not eye dependent... say "both" to eyes ?
    chns.append_child("channel").append_child_value("label", PDataTypes::tyn.at(PDataType::TIME_RAW_TIMESTAMP).toStdString()).append_child_value("eye", "both")
            .append_child_value("type", PDataTypes::tytXDF.at(PDataType::TIME_RAW_TIMESTAMP).toStdString()).append_child_value("unit", PDataTypes::tyd.at(PDataType::TIME_RAW_TIMESTAMP).toStdString());

    // specified in GUI
    chns.append_child("channel").append_child_value("label", PDataTypes::tyn.at(lsl_xdf_Diameter).toStdString()).append_child_value("eye", eyeStr)
            .append_child_value("type", PDataTypes::tytXDF.at(lsl_xdf_Diameter).toStdString()).append_child_value("unit", PDataTypes::tyd.at(lsl_xdf_Diameter).toStdString());
    // common
    chns.append_child("channel").append_child_value("label", PDataTypes::tyn.at(PDataType::PUPIL_WIDTH).toStdString()).append_child_value("eye", eyeStr)
            .append_child_value("type", PDataTypes::tytXDF.at(PDataType::PUPIL_WIDTH).toStdString()).append_child_value("unit", PDataTypes::tyd.at(PDataType::PUPIL_WIDTH).toStdString());
    chns.append_child("channel").append_child_value("label", PDataTypes::tyn.at(PDataType::PUPIL_HEIGHT).toStdString()).append_child_value("eye", eyeStr)
            .append_child_value("type", PDataTypes::tytXDF.at(PDataType::PUPIL_HEIGHT).toStdString()).append_child_value("unit", PDataTypes::tyd.at(PDataType::PUPIL_HEIGHT).toStdString());
    chns.append_child("channel").append_child_value("label", PDataTypes::tyn.at(PDataType::PUPIL_CENTER_X).toStdString()).append_child_value("eye", eyeStr)
            .append_child_value("type", PDataTypes::tytXDF.at(PDataType::PUPIL_CENTER_X).toStdString()).append_child_value("unit", PDataTypes::tyd.at(PDataType::PUPIL_CENTER_X).toStdString());
    chns.append_child("channel").append_child_value("label", PDataTypes::tyn.at(PDataType::PUPIL_CENTER_X).toStdString()).append_child_value("eye", eyeStr)
            .append_child_value("type", PDataTypes::tytXDF.at(PDataType::PUPIL_CENTER_Y).toStdString()).append_child_value("unit", PDataTypes::tyd.at(PDataType::PUPIL_CENTER_Y).toStdString());
    // specified in GUI
    chns.append_child("channel").append_child_value("label", PDataTypes::tyn.at(lsl_xdf_Confidence).toStdString()).append_child_value("eye", eyeStr)
            .append_child_value("type", PDataTypes::tytXDF.at(lsl_xdf_Confidence).toStdString()).append_child_value("unit", PDataTypes::tyd.at(lsl_xdf_Confidence).toStdString());

    // //(double)trialNum,
    // //temperatures[0],
    // //temperatures[1]
}

void EyeDataSerializer::addLSLChannelsInfo_V1(const std::vector<QChar> &eyeIdentities, const std::vector<QChar> &camIdentities, lsl::stream_info *info) {

    lsl::xml_element chns = info->desc().append_child("channels");

    std::string eyeStr = {};
    std::string camStr = {};

    //chns.append_child("channel").append_child_value("label", "timestamp")
    //        .append_child_value("type", "timestamp").append_child_value("unit", "ms");

    // NOTE: physicalDiameter will be duplicated redundantly. But it does not matter, this way data is much more self explanatory
    bool isTimestampAlreadyAdded = false;
    for(int i = 0; i < eyeIdentities.size(); i++) {
        eyeStr = "both";
        if(eyeIdentities[i] == 'R')
            eyeStr = "right";
        else if(eyeIdentities[i] == 'L')
            eyeStr = "left";
        camStr = QString(camIdentities[i]).toStdString();

        for(auto v : PDataTypes::dataOutputFields) {

            // TODO DEV KISZEDNI AMINT A PUPILLAL EGYÜTT KÖZVETíTETTÉ VÁLIK A TIMESTAMP A STRUCTON ÁT
            if(v == PDataType::TIME_RAW_TIMESTAMP) {
                if(!isTimestampAlreadyAdded) {
                    chns.append_child("channel").append_child_value("label", "timestamp")
                            .append_child_value("type", "timestamp").append_child_value("unit", "ms");
                    isTimestampAlreadyAdded = true;
                }
            } else
                chns.append_child("channel").append_child_value("label", PDataTypes::tyn.at(v).toStdString()).append_child_value("eye", eyeStr)
                    .append_child_value("type", PDataTypes::tytXDF.at(v).toStdString()).append_child_value("unit", PDataTypes::tyd.at(v).toStdString()).append_child_value("camera", camStr);
        }

        //chns.append_child("channel").append_child_value("label", "diameter").append_child_value("eye", eyeStr)
        //        .append_child_value("type", "Diameter").append_child_value("unit", "px").append_child_value("camera", camStr);
        //chns.append_child("channel").append_child_value("label", "undistortedDiameter").append_child_value("eye", eyeStr)
        //        .append_child_value("type", "Diameter").append_child_value("unit", "px").append_child_value("camera", camStr);
        //chns.append_child("channel").append_child_value("label", "physicalDiameter").append_child_value("eye", eyeStr)
        //        .append_child_value("type", "Diameter").append_child_value("unit", "mm").append_child_value("camera", camStr);
        //chns.append_child("channel").append_child_value("label", "width").append_child_value("eye", eyeStr)
        //        .append_child_value("type", "DiameterX").append_child_value("unit", "px").append_child_value("camera", camStr);
        //chns.append_child("channel").append_child_value("label", "height").append_child_value("eye", eyeStr)
        //        .append_child_value("type", "DiameterY").append_child_value("unit", "px").append_child_value("camera", camStr);
        //chns.append_child("channel").append_child_value("label", "axisRatio").append_child_value("eye", eyeStr)
        //        .append_child_value("unit", "").append_child_value("camera", camStr);
        //chns.append_child("channel").append_child_value("label", "centerX").append_child_value("eye", eyeStr)
        //        .append_child_value("type", "PupilX").append_child_value("unit", "px").append_child_value("camera", camStr);
        //chns.append_child("channel").append_child_value("label", "centerY").append_child_value("eye", eyeStr)
        //        .append_child_value("type", "PupilY").append_child_value("unit", "px").append_child_value("camera", camStr);
        //chns.append_child("channel").append_child_value("label", "angle").append_child_value("eye", eyeStr)
        //        .append_child_value("unit", "rad").append_child_value("camera", camStr);
        //chns.append_child("channel").append_child_value("label", "circumference").append_child_value("eye", eyeStr)
        //        .append_child_value("unit", "px").append_child_value("camera", camStr);
        //chns.append_child("channel").append_child_value("label", "confidence").append_child_value("eye", eyeStr)
        //        .append_child_value("type", "confidence").append_child_value("unit", "").append_child_value("camera", camStr);
        //chns.append_child("channel").append_child_value("label", "outlineConfidence").append_child_value("eye", eyeStr)
        //        .append_child_value("type", "confidence").append_child_value("unit", "").append_child_value("camera", camStr);
    }
    //auto hahaha = info->as_xml();
    //"trial"
    //"message"
    //"cameraTemperature_M_c"
    //"cameraTemperature_S_c"
}

std::vector<double> EyeDataSerializer::pupilToLSLsample_XDF(
        quint64 timestamp,
        int procMode,
        const std::vector<Pupil> &Pupils,
        LSL_XDF_Eye lsl_xdf_Eye,
        LSL_XDF_Camera lsl_xdf_Camera,
        PDataType lsl_xdf_Diameter,
        PDataType lsl_xdf_Confidence ) {

    std::vector<double> result = {};
    int pdx = 0;

    switch((ProcMode)procMode) {
        case ProcMode::SINGLE_IMAGE_ONE_PUPIL:
            pdx = SINGLE_IMAGE_ONE_PUPIL_MAIN;
            break;
        case ProcMode::SINGLE_IMAGE_TWO_PUPIL:
            // TODO: yet it hardcodedly assumes that the left eye is eye B
            if(lsl_xdf_Eye == LSL_XDF_Eye::XDF_RIGHT) {
                pdx = SINGLE_IMAGE_TWO_PUPIL_R;
            } else /*if(lsl_xdf_Eye == DataStreamer::LEFT)*/ {
                pdx = SINGLE_IMAGE_TWO_PUPIL_L;
            }
            break;
        case ProcMode::STEREO_IMAGE_ONE_PUPIL:
            if (lsl_xdf_Camera == LSL_XDF_Camera::XDF_MAIN) {
                pdx = STEREO_IMAGE_ONE_PUPIL_MAIN;
            } else {
                pdx = STEREO_IMAGE_ONE_PUPIL_SEC;
            }
            break;
        case ProcMode::STEREO_IMAGE_TWO_PUPIL:
            // TODO: yet it hardcodedly assumes that the left eye is eye B
            if(lsl_xdf_Eye == LSL_XDF_Eye::XDF_RIGHT) {
                if (lsl_xdf_Camera == LSL_XDF_Camera::XDF_MAIN) {
                    pdx = STEREO_IMAGE_TWO_PUPIL_R_MAIN;
                } else {
                    pdx = STEREO_IMAGE_TWO_PUPIL_R_SEC;
                }
            } else /*if(lsl_xdf_Eye == DataStreamer::LEFT)*/ {
                if (lsl_xdf_Camera == LSL_XDF_Camera::XDF_MAIN) {
                    pdx = STEREO_IMAGE_TWO_PUPIL_L_MAIN;
                } else {
                    pdx = STEREO_IMAGE_TWO_PUPIL_L_SEC;
                }
            }
            break;
        // case ProcMode::MIRR_IMAGE_ONE_PUPIL:
        //     //break;

        default:
            result = {};
    }

    //result = result % QString::fromStdString(Pupils[SINGLE_IMAGE_ONE_PUPIL_MAIN].algorithmName);
    //result = result % message;

    // common, but not eye dependent... say "both" to eyes ?
    result.push_back((double) (timestamp));

    // specified in GUI
    result.push_back(Pupils[pdx].getPData(lsl_xdf_Diameter));
    // common
    result.push_back(Pupils[pdx].getPData(PDataType::PUPIL_WIDTH));
    result.push_back(Pupils[pdx].getPData(PDataType::PUPIL_HEIGHT));
    result.push_back(Pupils[pdx].getPData(PDataType::PUPIL_CENTER_X));
    result.push_back(Pupils[pdx].getPData(PDataType::PUPIL_CENTER_Y));
    // specified in GUI
    result.push_back(Pupils[pdx].getPData(lsl_xdf_Confidence));

    //(double) trialNum
    //temperatures[0],
    //temperatures[1]

    return result;
}

std::vector<double> EyeDataSerializer::pupilToLSLsample_V1(quint64 timestamp, const std::vector<Pupil> &Pupils) {

    std::vector<double> result;

    //result.push_back((double)(timestamp));

    // NOTE: physicalDiameter will be duplicated redundantly. But it does not matter, this way data is much more self explanatory
    bool isTimestampAlreadyAdded = false;
    for(int i = 0; i < Pupils.size(); i++) {
        for(auto v : PDataTypes::dataOutputFields) {

            // TODO DEV KISZEDNI AMINT A PUPILLAL EGYÜTT KÖZVETíTETTÉ VÁLIK A TIMESTAMP A STRUCTON ÁT
            if(v == PDataType::TIME_RAW_TIMESTAMP) {
                if(!isTimestampAlreadyAdded) {
                    result.push_back((double) (timestamp));
                    isTimestampAlreadyAdded = true;
                }
            } else
                result.push_back(Pupils[i].getPData(v));
        }
    }

    return result;
}
#endif

// Converts a pupil detection to a string row that is written to file
// CAUTION: This must exactly reproduce the format defined by the header fields
QString EyeDataSerializer::pupilToRowCSV(quint64 timestamp, int procMode, const std::vector<Pupil> &Pupils, uint trialNum, QChar delim, DataWriterDataStyle dataStyle, const QString& message, const std::vector<double> &temperatures) {

    QString result; // TODO: .reserve() ?

    //result = result % QString::number(timestamp);
    //result = result % delim % QString::fromStdString(Pupils[SINGLE_IMAGE_ONE_PUPIL_MAIN].algorithmName);

    // NOTE: physicalDiameter will be duplicated redundantly. But it does not matter, this way data is much more self explanatory
    bool isTimestampAlreadyAdded = false;
    for(int i = 0; i < Pupils.size(); i++) {
        for(auto v : PDataTypes::dataOutputFields) {


            // TODO DEV KISZEDNI AMINT A PUPILLAL EGYÜTT KÖZVETíTETTÉ VÁLIK A TIMESTAMP A STRUCTON ÁT
            if(v == PDataType::TIME_RAW_TIMESTAMP) {
                if(!isTimestampAlreadyAdded) {
                    result = result % QString::number(timestamp);
                    isTimestampAlreadyAdded = true;
                }
            } else
                result = result % delim % QString::number(Pupils[i].getPData(v));
        }
    }

    //for(int i = 0; i < Pupils.size(); i++) {
    //    result = result % delim % QString::number(Pupils[i].diameter());
    //    result = result % delim % QString::number(Pupils[i].undistortedDiameter);
    //    result = result % delim % QString::number(Pupils[i].physicalDiameter);
    //    result = result % delim % QString::number(Pupils[i].width());
    //    result = result % delim % QString::number(Pupils[i].height());
    //    result = result % delim % QString::number(((double) Pupils[i].width() / Pupils[i].height()));
    //    result = result % delim % QString::number(Pupils[i].center.x);
    //    result = result % delim % QString::number(Pupils[i].center.y);
    //    result = result % delim % QString::number(Pupils[i].angle);
    //    result = result % delim % QString::number(Pupils[i].circumference());
    //    result = result % delim % QString::number(Pupils[i].confidence);
    //    result = result % delim % QString::number(Pupils[i].outline_confidence);
    //}

    result = result % delim % QString::number(trialNum);
    result = result % delim % message;
    result = result % delim % QString::number(temperatures[0]);
    result = result % delim % QString::number(temperatures[1]);

    return result;
}


QString EyeDataSerializer::pupilToYAML(quint64 timestamp, int procMode, const std::vector<Pupil> &Pupils, uint trialNum, const QString& message, const std::vector<double> &temperatures) {

    QString obj;

    addRowYAML(obj, "EyeData", "", 0, false);

    switch((ProcMode)procMode) {
        case ProcMode::SINGLE_IMAGE_ONE_PUPIL:

            addRowYAML(obj, {Pupils[SINGLE_IMAGE_ONE_PUPIL_MAIN].eyeIdentity}, "", 1, false);
            addRowYAML(obj, "Main", "", 2, false);
            populatePupilNodeYAML(timestamp, obj, 3, SINGLE_IMAGE_ONE_PUPIL_MAIN, Pupils, trialNum, message, temperatures[0]);
            break;
        case ProcMode::SINGLE_IMAGE_TWO_PUPIL:

            addRowYAML(obj, {Pupils[SINGLE_IMAGE_TWO_PUPIL_R].eyeIdentity}, "", 1, false);
            addRowYAML(obj, "Main", "", 2, false);
            populatePupilNodeYAML(timestamp, obj, 3, SINGLE_IMAGE_TWO_PUPIL_R, Pupils, trialNum, message, temperatures[0]);
            addRowYAML(obj, {Pupils[SINGLE_IMAGE_TWO_PUPIL_L].eyeIdentity}, "", 1, false);
            addRowYAML(obj, "Main", "", 2, false);
            populatePupilNodeYAML(timestamp, obj, 3, SINGLE_IMAGE_TWO_PUPIL_L, Pupils, trialNum, message, temperatures[0]);
            break;
        case ProcMode::STEREO_IMAGE_ONE_PUPIL:

            addRowYAML(obj, {Pupils[STEREO_IMAGE_ONE_PUPIL_MAIN].eyeIdentity}, "", 1, false);
            addRowYAML(obj, "Main", "", 2, false);
            populatePupilNodeYAML(timestamp, obj, 3, STEREO_IMAGE_ONE_PUPIL_MAIN, Pupils, trialNum, message, temperatures[0]);
            addRowYAML(obj, {Pupils[STEREO_IMAGE_ONE_PUPIL_SEC].eyeIdentity}, "", 1, false);
            addRowYAML(obj, "Sec", "", 2, false);
            populatePupilNodeYAML(timestamp, obj, 3, STEREO_IMAGE_ONE_PUPIL_SEC, Pupils, trialNum, message, temperatures[1]);
            break;
        case ProcMode::STEREO_IMAGE_TWO_PUPIL:

            addRowYAML(obj, {Pupils[STEREO_IMAGE_TWO_PUPIL_R_MAIN].eyeIdentity}, "", 1, false);
            addRowYAML(obj, "Main", "", 2, false);
            populatePupilNodeYAML(timestamp, obj, 3, STEREO_IMAGE_TWO_PUPIL_R_MAIN, Pupils, trialNum, message, temperatures[0]);
            addRowYAML(obj, {Pupils[STEREO_IMAGE_TWO_PUPIL_R_SEC].eyeIdentity}, "", 1, false);
            addRowYAML(obj, "Sec", "", 2, false);
            populatePupilNodeYAML(timestamp, obj, 3, STEREO_IMAGE_TWO_PUPIL_R_SEC, Pupils, trialNum, message, temperatures[1]);

            addRowYAML(obj, {Pupils[STEREO_IMAGE_TWO_PUPIL_L_MAIN].eyeIdentity}, "", 1, false);
            addRowYAML(obj, "Main", "", 2, false);
            populatePupilNodeYAML(timestamp, obj, 3, STEREO_IMAGE_TWO_PUPIL_L_MAIN, Pupils, trialNum, message, temperatures[0]);
            addRowYAML(obj, {Pupils[STEREO_IMAGE_TWO_PUPIL_L_SEC].eyeIdentity}, "", 1, false);
            addRowYAML(obj, "Sec", "", 2, false);
            populatePupilNodeYAML(timestamp, obj, 3, STEREO_IMAGE_TWO_PUPIL_L_SEC, Pupils, trialNum, message, temperatures[1]);
            break;
            
        // case ProcMode::MIRR_IMAGE_ONE_PUPIL:

        //     addRowYAML(obj, "A", "", 1, false);
        //     addRowYAML(obj, "Main", "", 2, false);
        //     populatePupilNodeYAML(timestamp, obj, 3, MIRR_IMAGE_ONE_PUPIL_MAIN, Pupils, trialNum, temperatures[0], message);
        //     addRowYAML(obj, "A", "", 1, false);
        //     addRowYAML(obj, "Sec", "", 2, false);
        //     populatePupilNodeYAML(timestamp, obj, 3, MIRR_IMAGE_ONE_PUPIL_SEC, Pupils, trialNum, temperatures[0], message);
        //     break;
        
        //default:
            //break;
    }

    return obj;
}

void EyeDataSerializer::populatePupilNodeYAML(quint64 &timestamp, QString &obj, ushort depth, int idx, const std::vector<Pupil> &Pupils, uint &trialNum, const QString& message, double temperature) {

    depth+=1;
    for(auto v : PDataTypes::dataOutputFields) {
        QString ds = "";
        if(!PDataTypes::tyd.at(v).isEmpty()) {
            ds = "_" + PDataTypes::tyd.at(v);
        }

        // TODO DEV KISZEDNI AMINT A PUPILLAL EGYÜTT KÖZVETíTETTÉ VÁLIK A TIMESTAMP A STRUCTON ÁT
        if(v == PDataType::TIME_RAW_TIMESTAMP)
            addRowYAML(obj, PDataTypes::tyn.at(v) + ds, QString::number(timestamp), depth, true);
        else
            addRowYAML(obj, PDataTypes::tyn.at(v) + ds, QString::number(Pupils[idx].getPData(v)), depth, true);
    }
    //addRowYAML(obj, "timestamp_ms", QString::number(timestamp), depth, true);
    ////addRowYAML(obj, "algorithm", QString::fromStdString(Pupils[idx].algorithmName), depth, true);
    //addRowYAML(obj, "diameter_px", QString::number(Pupils[idx].diameter()), depth, true);
    //addRowYAML(obj, "undistortedDiameter_px", QString::number(Pupils[idx].undistortedDiameter), depth, true);
    //addRowYAML(obj, "physicalDiameter_mm", QString::number(Pupils[idx].physicalDiameter), depth, true);
    //addRowYAML(obj, "width_px", QString::number(Pupils[idx].width()), depth, true);
    //addRowYAML(obj, "height_px", QString::number(Pupils[idx].height()), depth, true);
    //addRowYAML(obj, "axisRatio_px", QString::number((double)Pupils[idx].width() / Pupils[idx].height()), depth, true);
    //addRowYAML(obj, "centerX_px", QString::number(Pupils[idx].center.x), depth, true);
    //addRowYAML(obj, "centerY_px", QString::number(Pupils[idx].center.y), depth, true);
    //addRowYAML(obj, "angle_deg", QString::number(Pupils[idx].angle), depth, true);
    //addRowYAML(obj, "circumference_px", QString::number(Pupils[idx].circumference()), depth, true);
    //addRowYAML(obj, "confidence", QString::number(Pupils[idx].confidence), depth, true);
    //addRowYAML(obj, "outlineConfidence", QString::number(Pupils[idx].outline_confidence), depth, true);

    addRowYAML(obj, "trial", QString::number(trialNum), depth, true);
    addRowYAML(obj, "message", message, depth, true);
    addRowYAML(obj, "cameraTemperature_c", QString::number(temperature), depth, true);
}

void EyeDataSerializer::addRowYAML(QString &obj, QString key, QString value, ushort depth, bool isLeaf) {
    
    // object:
    //     key: value
    //     array:
    //         - null_value:
    //         - boolean: true
    //         - integer: 1
    //         - alias: value
    
    for(ushort c=0; c<depth; c++) {
        obj = obj % "  ";
    }

    //if(isLeaf)
    //    obj = obj % "- " % key % ": " % value % '\n';
    //else
        obj = obj % key % ": " % value % '\n';
    
}


