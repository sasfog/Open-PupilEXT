#pragma once

/**
    @author Gábor Bényei
*/

#include <iostream>
#include <QtCore/QObject>
#include <QtCore/QFile>
#include <QDir>
#include <QtCore/qfileinfo.h>
#include <cmath>
#include <QRectF>

/**
    
    Various little functions that are needed here and there, e.g. checking and simplifying strings of filenames and paths before using them for I/O

*/
class SupportFunctions : public QObject {
    Q_OBJECT

public: 

    static QString simplifyReceivedMessage(QString str) {
        // remove CR, LF and other strange characters
        str.replace("\r", "");
        str.replace("\n", "");
        //str.replace("\t", "");
        str.replace("\"", "");
        // Qt has this method to remove trailing whitespace from the beginning and the end of a string, and replace internal whitespaces with a single space
        str = str.simplified();
        // if there are "\" characters, change them to "/"
        str.replace("\\", "/");

        return str;
    };

    static QString simplifyPathName(QString str) {
        // remove CR, LF and other strange characters
        str.replace("\r", "");
        str.replace("\n", "");
        str.replace("\t", "");
        //fullPath.replace(":", ""); // todo: only one ":" should maximally occur
        str.replace("*", "");
        str.replace("\"", "");
        str.replace("<", "");
        str.replace(">", "");
        str.replace("|", "");
        // Qt has this method to remove trailing whitespace from the beginning and the end of a string, and replace internal whitespaces with a single space
        str = str.simplified();
        // if there are "\" characters, change them to "/"
        str.replace("\\", "/");
        //todo: remove "..." and replace with ".." as long as they persist, etc. Now it is just a workaround
        str.replace("/./", "/");
        str.replace("...", "..");

        return str;
    };

    static bool preparePath(const QString& target) {

        if(QFileInfo(target).absoluteDir().exists() == true) 
            return true;
        
        //std::cout << "Specified path does not point to an existing folder. Now we try to create the path iteratively: folder, sub-folder sub-sub-folder, ..." << std::endl;
        //std::cout << "target = " << target.toStdString() << std::endl;

        // GB NOTE: .absolutePath() causes undefined behaviour 
        // (without exception) when called upon a string that does NOT point to a file, 
        // but only a folder, on an external drive (not .exe path). 
        // I could not figure out the exact problem, but made a workaround. 
        // Problem can be diagnosed if desiredPath variable contains the .exe path and the target path combined.
        // I think it is some kind of memory problem in the background
        // Qt 5.15.2, MSVC 2019 v86_amd64
        QString desiredPath;
        if(target[target.length()-1] == '/') { // if "target" is a path itself
            //std::cout << "DESIRED PATH VAR = target "<< std::endl;
            desiredPath = target;
        } else {// if "target" is pointing to a file, not a folder/dir
            //std::cout << "DESIRED PATH VAR = QFileInfo(target).absolutePath()"<< std::endl;
            //desiredPath = QFileInfo(target).absolutePath(); 
            int idx = target.lastIndexOf("/");
            desiredPath = target.mid(0, idx);
        }
        //std::cout << "desiredPath = " << desiredPath.toStdString() << std::endl;
        QStringList subStrings = desiredPath.split('/');
        bool success = false;

        //std::cout << "subStrings 0-th elem = " << subStrings[0].toStdString() << std::endl;
        QString cumulatedPath = subStrings[0] + "/";
        for(int h=1; h < subStrings.length(); h++)
        {
            //std::cout << "subStrings n-th elem = " << subStrings[h].toStdString() << std::endl;
            cumulatedPath = cumulatedPath + subStrings[h] + "/";
            //std::cout << "Now creating: " << QFileInfo(cumulatedPath).absolutePath().toStdString() << std::endl;
            if(QFileInfo(cumulatedPath).absoluteDir().exists() == false) {
                success = QDir().mkdir(QFileInfo(cumulatedPath).absolutePath());
                if(!success)
                    break;
            }
        }

        if(success)
            return true;
        else 
            return false;
    
    };

    static QRectF calculateRoiD(QRectF imageRect, QRectF discreteROI, QRectF rationalROI, float defaultRatio = 0.6f){
        QRectF newRoi;

        if(!rationalROI.isEmpty()){
            if (isValidRoi(imageRect, discreteROI, rationalROI)){
                return getRectDiscreteFromRational(imageRect, rationalROI);
            }
        }
        return getDefaultRectD(imageRect, imageRect.center(), defaultRatio);
    }

    static QRectF calculateRoiR(QRectF imageRect, QRectF discreteROI, QRectF rationalROI, float defaultRatio = 0.6f){
        QRectF newRoi;

        if(!rationalROI.isEmpty()){
            if (isValidRoi(imageRect, discreteROI, rationalROI)){
                return rationalROI;
            }
        }
        return getDefaultRectR(imageRect, imageRect.center(), defaultRatio);
    }

    static QRectF getRectDiscreteFromRational(QRectF imageRect, QRectF rationalROI){
        return QRectF(rationalROI.x() * imageRect.width(), rationalROI.y() * imageRect.height(),
                    rationalROI.width() * imageRect.width(), rationalROI.height() * imageRect.height());

    }
    static QRectF getDefaultRectD(QRectF imageRect, QPointF center, float defaultRatio){
        int height = 0;
        int width = 0;
        if (imageRect.height() < imageRect.width()){
            height = std::round(imageRect.height() * defaultRatio);
            width = std::round((height / 3) * 4);
        }
        else {
            width = std::round(imageRect.width() * defaultRatio);
            height = std::round((width / 4) * 3);
        }
        QRectF newRoi = QRectF(center.x() - width / 2, center.y() - height / 2, width, height);
        return newRoi;
    }

    static QRectF getDefaultRectR(QRectF imageRect, QPointF center, float defaultRatio){
        QRectF rectR = getDefaultRectD(imageRect, center, defaultRatio);
        return QRectF(rectR.topLeft().x() / imageRect.width(), rectR.topLeft().y() / imageRect.height(),
        rectR.width() / imageRect.width(), rectR.height() / imageRect.height());
    }

    static bool isValidRoi(QRectF imageRect, QRectF discreteROI, QRectF rationalROI){
        float epsilon = 0.0001f;
        float widthRatio = discreteROI.width() / imageRect.width();
        bool widthMatches = widthRatio - epsilon <= rationalROI.width() && rationalROI.width() <= widthRatio + epsilon;
        float heightRatio = discreteROI.height() / imageRect.height();
        bool heightMatches = heightRatio - epsilon <= rationalROI.height() && rationalROI.height() <= heightRatio + epsilon;
        float xRatio = discreteROI.x() / imageRect.width();
        bool xMatches = xRatio - epsilon <= rationalROI.x() && rationalROI.x() <= xRatio + epsilon;
        float yRatio = discreteROI.y() / imageRect.height();
        bool yMatches = yRatio - epsilon <= rationalROI.y() && rationalROI.y() <= yRatio + epsilon;
        return xMatches && yMatches && widthMatches && heightMatches;
    }
};