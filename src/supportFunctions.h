#pragma once

/**
    @author Gabor Benyei, Attila Boncser
*/

#include <iostream>
#include <QtCore/QObject>
#include <QtCore/QFile>
#include <QDir>
#include <QtCore/qfileinfo.h>
#include <cmath>
#include <QRectF>
#include <QVector3D>
#include <QColor>
#include "subwindows/twoChoiceCheckboxDialog.h"
#include <opencv2/core/mat.hpp>

/**

    Various little functions that are needed here and there, e.g. checking and simplifying strings of filenames and paths before using them for I/O

*/

// NOTE: Option "BOTH" could also have been here, but then even deeper if/else's
//  and averaging would be needed ...and even more spaghetti
// TODO: USE OTHER, BROADER-SCOPE ENUMS INSTEAD OF THESE
enum LSL_XDF_Eye {XDF_LEFT = 1, XDF_RIGHT = 2};
enum LSL_XDF_Camera {XDF_MAIN = 1, XDF_SECONDARY = 2};

class SupportFunctions : public QObject
{
    Q_OBJECT

public:

    //static double avgec(double a, double b) {
    //    if(a == -1.0)
    //        return b;
    //    if(b == -1.0)
    //        return a;
    //    return (a+b)/2.0;
    //}

    static QString makeUniqueLSLSourceID() {
        QString bs = qgetenv("USER");
        if (bs.isEmpty())
            bs = qgetenv("USERNAME");

        QString sourceID = "PupilEXT";
        if (!bs.isEmpty()) {
            QString temp = QString::number(qHash(bs));
            sourceID.append("-" + temp.mid(temp.length()-7, 6));
        } else
            sourceID.append("-123456");

        return sourceID;
    }

    static QVariant toVariantFromWrapped(QString valueInStr, bool *ok) {
        *ok = false;
        valueInStr = valueInStr.replace(" ","");
        //qDebug() << valueInStr;

        if(valueInStr.startsWith("QVector3D")) {
            valueInStr = valueInStr.remove(0, 10);
            valueInStr = valueInStr.remove(valueInStr.length()-1, 1);

            QStringList strings = valueInStr.split(",");

            if(strings.length() == 3) {
                *ok = true;
                return (QVariant)QVector3D(strings[0].toFloat(ok), strings[1].toFloat(ok), strings[2].toFloat(ok));
            }
        } else if(valueInStr.startsWith("QList<CameraConnectorPin>")) {

            // BUG TODO: this might read as this, but save as QList<int> or QList<QVariant>

            valueInStr = valueInStr.remove(0, 25);
            valueInStr = valueInStr.remove(valueInStr.length()-1, 1);

            QStringList strings = valueInStr.split(",");
            QList<int> result;
            for(int h = 0; h < strings.length(); h++) {
                result.append(strings[0].toInt(ok));
            }

            return QVariant::fromValue(result);
        }
        return QVariant();
    }

    static QString camelCaseToFriendly(const QString& s)
    {
        // //QRegularExpression regexp("[A-Z][a-z]*|\\d+[a-z]+|\\d+");
        QRegularExpression regexp("[A-Z]{2,}(?=[A-Z][a-z])|[A-Z][a-z]+|\\d+[a-z]+|\\d+|[A-Z]");
        // QRegularExpression regexp("[A-Z]{2,}(?=[A-Z][a-z])|[A-Z][a-z]+|\d+[a-z]+|\d+|[A-Z]");
        QRegularExpressionMatchIterator match = regexp.globalMatch(s);
        QStringList strings;

        while(match.hasNext())
            strings.append(match.next().capturedTexts());

        QString result = strings.join(" ");

        // NOTE: For some reason, the output does not correspond to what should be expected
        // as e.g. seen on https://regex101.com/ for this expression. So here is a workaround
        bool hooked = false;
        int i = result.length()-4;
        while(i > 0) {
            if(result[i]==" " && result[i+1].isUpper() && (hooked || (result[i+2]==" " && result[i+3].isUpper())) ) {
                result = result.remove(i,1);
                i--;
                hooked = true;
            } else {
                hooked = false;
            }
            i--;
        }
        if(result.toUpper() == result) {
            result = result.replace(" ","");
        }

        return result;
    }

    static QString simplifyReceivedMessage(QString str)
    {
        // remove CR, LF and other strange characters
        str.replace("\r", "");
        str.replace("\n", "");
        // str.replace("\t", "");
        str.replace("\"", "");
        // Qt has this method to remove trailing whitespace from the beginning and the end of a string, and replace internal whitespaces with a single space
        str = str.simplified();
        // if there are "\" characters, change them to "/"
        str.replace("\\", "/");

        return str;
    };

    static QString simplifyPathName(QString str)
    {
        // remove CR, LF and other strange characters
        str.replace("\r", "");
        str.replace("\n", "");
        str.replace("\t", "");
        // fullPath.replace(":", ""); // todo: only one ":" should maximally occur
        str.replace("*", "");
        str.replace("\"", "");
        str.replace("<", "");
        str.replace(">", "");
        str.replace("|", "");
        // Qt has this method to remove trailing whitespace from the beginning and the end of a string, and replace internal whitespaces with a single space
        str = str.simplified();
        // if there are "\" characters, change them to "/"
        str.replace("\\", "/"); // TODO: WARNING: can interfere with OS-native separator, though / is safe but \ is not always so
        // todo: remove "..." and replace with ".." as long as they persist, etc. Now it is just a workaround
        str.replace("/./", "/");
        str.replace("...", "..");

        return str;
    };

    // This function is used to simplify directory names that are auto-created (not selected in a GUI dialog by the user)
    // Only a-z, 0-9, and _ are accepted, anything else will translate to _ character, and length will be trimmed to 240 chars
    // (usually 255 is max by filesystems, but we reserve the last chars for auto-naming in worst-case scenarios)
    static QString simplifyNewPathNodeName(QString str, bool &changed)
    {
        QString str2 = str;
        // [^a-zA-Z\d\s:]
        // [^a-zA-Z0-9_:]
        // [-`~!@#$%^&*()—+=|:;<>«»,.?/{}'"\[\]\]
//        str2.replace(QRegExp(QString::fromUtf8("[^a-zA-Z0-9_:]")), "_");
        str2.replace(QRegularExpression(QString::fromUtf8("[^a-zA-Z0-9_ :]")), "_");
        // NOTE: we leave whitespaces as well. this may be changed later,
        // but if we would like to change whitespaces in path, that is probably a lot of work, as
        // e.g. the user name on the computer can contain a whitespace, thus all the user wants to save will be
        // created in a parallelly created folder structure, under a similar but whitespace-eliminated path similar
        // to the one they wanted to use in the first place.
        // TODO: The proper way would be to inform the user in GUI
        // whenever a whitespace-containing path is encountered for writing or planned for writing later.

        str2 = str2.mid(0, qMin(str.size(), 240));
        if(str2!=str) {
            changed = true;
        }
        return str2;
    };

    // IMPORTANT: This function is supposed to be only ever called for a PATH and never an exact file name
    static bool preparePath(const QString &target, bool &changedAnything, QString &changedPath, bool &newNodeCreated)
    {
        // TODO: is this always safe to solely rely on?
        if (QFileInfo(target).absoluteDir().exists() && QFileInfo(target).isWritable())
            return true;

        // std::cout << "Specified path does not point to an existing folder. Now we try to create the path iteratively: folder, sub-folder sub-sub-folder, ..." << std::endl;
        // std::cout << "target = " << target.toStdString() << std::endl;

        // GB NOTE: .absolutePath() causes undefined behaviour
        // (without exception) when called upon a string that does NOT point to a file,
        // but only a folder, on an external drive (not .exe path).
        // I could not figure out the exact problem, but made a workaround.
        // Problem can be diagnosed if desiredPath variable contains the .exe path and the target path combined.
        // I think it is some kind of memory problem in the background
        // Qt 5.15.2, MSVC 2019 v86_amd64
//        QString desiredPath;
////        if (target[target.length() - 1] == '/')
//        if (QFileInfo(target).completeSuffix().isEmpty()) { // if "target" is a path itself
//            desiredPath = target;
//        }
//        else { // if "target" is pointing to a file, not a folder/dir
//            desiredPath = QFileInfo(target).absolutePath();
////            int idx = target.lastIndexOf("/");
////            desiredPath = target.mid(0, idx);
//        }
        QString desiredPath = target;

        changedPath = desiredPath;
        QString tempPath = simplifyPathName(desiredPath);
        if(tempPath != target) {
            changedAnything = true;
            desiredPath = tempPath;
        }

        // std::cout << "desiredPath = " << desiredPath.toStdString() << std::endl;
        QStringList subStrings = desiredPath.split('/');
        bool candidateExists = false;
        bool changedExists = false;
//        bool newNodeCreated = false;
        bool changedNodeName = false;

        // std::cout << "subStrings 0-th elem = " << subStrings[0].toStdString() << std::endl;
        QString cumulatedPath = subStrings[0] + "/";
        QString cumulatedPathCandidate;
        QString newNodeName;
        for (int h = 1; h < subStrings.length() && !subStrings[h].isEmpty(); h++)
        {
            // std::cout << "subStrings n-th elem = " << subStrings[h].toStdString() << std::endl;
            cumulatedPathCandidate = cumulatedPath + subStrings[h] + "/";
            // std::cout << "Now creating: " << QFileInfo(cumulatedPath).absolutePath().toStdString() << std::endl;
            candidateExists = QFileInfo(cumulatedPathCandidate).absoluteDir().exists();

            if (!candidateExists)
            {
                // TODO: make some feedback, notify higher level code, and the user about the auto-renaming of illegal path/directory tree node names
                newNodeName = simplifyNewPathNodeName(subStrings[h], changedNodeName);
                cumulatedPath = cumulatedPath + newNodeName + "/";

                newNodeCreated = QDir().mkpath(QFileInfo(cumulatedPath).absolutePath());
                changedExists = QFileInfo(cumulatedPath).absoluteDir().exists();
                if (!newNodeCreated && !changedExists)
                    break;

                if(changedNodeName) {
                    changedAnything = true;
                    qDebug() << "Had to change directory name from: " << subStrings[h] << " to: " << newNodeName;
                    changedNodeName = false;
                }
            } else {
                cumulatedPath = cumulatedPathCandidate;
            }
        }

        if(changedAnything) {
            changedPath = cumulatedPath;
        }

        if (QFileInfo(changedPath).absoluteDir().exists() && QFileInfo(changedPath).isWritable())
            return true;
        else
            return false;
    };

    static QString stripIfInventedName(QString fileBaseName) {
        QString workCopy = fileBaseName;
//        while(workCopy.length()>0 && workCopy[workCopy.length()-1].isDigit()) {
//            workCopy.chop(1);
//        }
        if(workCopy.length()==0) {
            return fileBaseName;
        }
        if(workCopy.endsWith("_Run", Qt::CaseInsensitive)) {
            workCopy.chop(4);
        }
        return workCopy;
    };

    static QString prepareOutputZipDirForImageWriter(QString filePathAndname, QSettings* applicationSettings, bool &changedGiven, QWidget* parent) {
        QString imageWriterDataRule = applicationSettings->value("imageWriterDataRule", "ask").toString();

        if(filePathAndname.isEmpty()) {
            return filePathAndname;
        }

        QString fileName = filePathAndname.mid(filePathAndname.lastIndexOf("/")+1, filePathAndname.length()-(filePathAndname.lastIndexOf("/")));
        QString containingDirectory = filePathAndname.mid(0, filePathAndname.lastIndexOf("/"));

        // bool changedGiven = false;
        QString changedPath;
        bool newNodeCreated = false;
        bool pathWriteable = SupportFunctions::preparePath(containingDirectory, changedGiven, changedPath, newNodeCreated);
        if(!pathWriteable) {
            // TODO: Throw exception?
            changedGiven = true;
            return QString();
        }
        if(changedGiven) {
            QMessageBox *msgBox = new QMessageBox(parent);
            msgBox->setWindowTitle("Path name changed");
            msgBox->setText("The given path/name contained nonstandard characters,\nwhich were changed automatically for the following: a-z, A-Z, 0-9, _");
            msgBox->setIcon(QMessageBox::Warning);
            msgBox->setModal(false);
            msgBox->show();

            filePathAndname = changedPath + "/" + fileName;
        }

        // QDir outputDirectory = QDir(directory);
        bool exists = QFile(filePathAndname).exists();
        bool hasContent = QFile(filePathAndname).size() > 0;

        // TODO: what if there is e.g. a single recording already, the user says "append" but the current setup is for stereo camera...? Incongruent recording can result
        if(exists && hasContent && imageWriterDataRule == "ask") {
            TwoChoiceCheckboxDialog *dialog = new TwoChoiceCheckboxDialog(
                    "Image output archive already exists",
                    "Existing data was found under the target path/name you specified. Please choose whether you would like to append to the existing recording or keep it and save the new recording with an automatically generated different path/name?",
                    "Append to existing",
                    "Keep existing and save new as well",
                    "Remember this choice",
                    QSize(450,150),
                    true,
                    parent);
            dialog->setModal(true);
            // dialog->raise();
            if(dialog->exec() == QDialog::Accepted)
            {
                auto resp = dialog->getResponse();
                bool rememberChoice = dialog->getRememberChoice();

                if(resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_1) {
                    imageWriterDataRule = "append";
                } else /*if(resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_2)*/ {
                    imageWriterDataRule = "new";
                }

                if((resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_1 || resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_2) && rememberChoice) {
                    applicationSettings->setValue("imageWriterDataRule", imageWriterDataRule);
                }
            }
        }

        if(exists && hasContent && imageWriterDataRule == "new") {
            bool nameInvented = false;
            int nameIter = 1;
            QString tryBase = filePathAndname.chopped(4);
            tryBase = SupportFunctions::stripIfInventedName(tryBase);
            // TODO: proper exception handling
            while(!nameInvented) {
                nameIter++;
//                outputDirectory = QDir(tryBase + "_RunI" + QString::number(nameIter));
                filePathAndname = tryBase + "_RunI" + QString::number(nameIter) + ".zip";
//                nameInvented = !outputDirectory.exists();
                nameInvented = !QFile(filePathAndname).exists();
                if(nameIter >=65000)
                    filePathAndname = tryBase + "_TooManyRunsI";
            }
        }
        //std::cout << outputDirectory.absolutePath().toStdString() << std::endl;
//        return outputDirectory.absolutePath();
        return filePathAndname;
    };

    static QString prepareOutputDirForImageWriter(QString directory, QSettings* applicationSettings, bool &changedGiven, QWidget* parent) {
        QString imageWriterDataRule = applicationSettings->value("imageWriterDataRule", "ask").toString();

        if(directory.isEmpty()) {
            return directory;
        }

        if(directory[directory.length()-1] == '/') {
            directory.chop(1);
        }

        // bool changedGiven = false;
        QString changedPath;
        bool newNodeCreated = false;
        bool pathWriteable = SupportFunctions::preparePath(directory, changedGiven, changedPath, newNodeCreated);
        if(!pathWriteable) {
            // TODO: Throw exception?
            changedGiven = true;
            return QString();
        }
        if(changedGiven) {
            QMessageBox *msgBox = new QMessageBox(parent);
            msgBox->setWindowTitle("Path name changed");
            msgBox->setText("The given path/name contained nonstandard characters,\nwhich were changed automatically for the following: a-z, A-Z, 0-9, _");
            msgBox->setIcon(QMessageBox::Warning);
            msgBox->setModal(false);
            msgBox->show();

            directory = changedPath;
        }

        // QDir outputDirectory = QDir(directory);
        bool exists = QDir(directory).exists();
        bool hasContent = !QDir(directory).isEmpty();

        // TODO: what if there is e.g. a single recording already, the user says "append" but the current setup is for stereo camera...? Incongruent recording can result
        if(!exists) {
// mkdir(".") DOES NOT WORK ON MACOS, ONLY WINDOWS. (Reported on MacOS 12.7.6 and Windows 10)
//            outputDirectory.mkdir(".");
            QDir().mkpath(directory);
        } else if(hasContent && imageWriterDataRule == "ask") {
            TwoChoiceCheckboxDialog *dialog = new TwoChoiceCheckboxDialog(
                    "Image output folder already exists",
                    "Existing data was found under the target path/name you specified. Please choose whether you would like to append to the existing recording or keep it and save the new recording with an automatically generated different path/name?",
                    "Append to existing",
                    "Keep existing and save new as well",
                    "Remember this choice",
                    QSize(450,150),
                    true,
                    parent);
            dialog->setModal(true);
            // dialog->raise();
            if(dialog->exec() == QDialog::Accepted)
            {
                auto resp = dialog->getResponse();
                bool rememberChoice = dialog->getRememberChoice();

                if(resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_1) {
                    imageWriterDataRule = "append";
                } else /*if(resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_2)*/ {
                    imageWriterDataRule = "new";
                }

                if((resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_1 || resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_2) && rememberChoice) {
                    applicationSettings->setValue("imageWriterDataRule", imageWriterDataRule);
                }
            }
        }

        if(exists && hasContent && imageWriterDataRule == "new") {
            bool nameInvented = false;
            int nameIter = 1;
            QString tryBase = directory;
            tryBase = SupportFunctions::stripIfInventedName(tryBase);
            // TODO: proper exception handling
            while(!nameInvented) {
                nameIter++;
//                outputDirectory = QDir(tryBase + "_RunI" + QString::number(nameIter));
                directory = tryBase + "_RunI" + QString::number(nameIter);
//                nameInvented = !outputDirectory.exists();
                nameInvented = !QDir(directory).exists();
                if(nameIter >=65000)
                    directory = tryBase + "_TooManyRunsI";
            }
            // mkdir(".") DOES NOT WORK ON MACOS, ONLY WINDOWS. (Reported on MacOS 12.7.6 and Windows 10)
//            outputDirectory.mkdir(".");
            QDir().mkpath(directory);
        }
        //std::cout << outputDirectory.absolutePath().toStdString() << std::endl;
//        return outputDirectory.absolutePath();
        return directory;
    };

    static QString prepareOutputFileForDataWriter(QString fileName, QSettings* applicationSettings, bool &changedGiven, QWidget* parent) {
        QString dataWriterDataRule = applicationSettings->value("dataWriterDataRule", "ask").toString();

        //bool changedGiven = false;
        QString changedPath;
        bool newNodeCreated = false;
        // TODO: false case and exception handling
        bool pathWriteable = SupportFunctions::preparePath(QFileInfo(fileName).absolutePath(), changedGiven, changedPath, newNodeCreated);
        if(!pathWriteable) {
            // TODO: Throw exception?
            changedGiven = true;
            return QString();
        }
        if(changedGiven) {
        //    QMessageBox::warning(parent, "Path name changed",
        //                         "The given path/name contained nonstandard characters,\nwhich were changed automatically for the following: a-z, A-Z, 0-9, _");
            fileName = changedPath + QFileInfo(fileName).completeBaseName() + '.' + QFileInfo(fileName).completeSuffix();
        }

        QFileInfo dataFile(fileName);
        bool exists = dataFile.exists();
        bool hasContent = dataFile.size() != 0;

        if(exists && hasContent && dataWriterDataRule == "ask") {
            TwoChoiceCheckboxDialog *dialog = new TwoChoiceCheckboxDialog(
                    "Data recording output file already exists",
                    "Existing data was found under the target path/name you specified. Please choose whether you would like to append to the existing recording or keep it and save the new recording with an automatically generated different path/name?",
                    "Append to existing",
                    "Keep existing and save new as well",
                    "Remember this choice",
                    QSize(450,150),
                    true,
                    parent);
            dialog->setModal(true);
            // dialog->raise();
            if(dialog->exec() == QDialog::Accepted)
            {
                auto resp = dialog->getResponse();
                bool rememberChoice = dialog->getRememberChoice();

                if(resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_1) {
                    dataWriterDataRule = "append";
                } else /*if(resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_2)*/ {
                    dataWriterDataRule = "new";
                }

                if((resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_1 || resp == TwoChoiceCheckboxDialog::TwoChoiceCheckboxResponse::OPTION_2) && rememberChoice) {
                    applicationSettings->setValue("dataWriterDataRule", dataWriterDataRule);
                }
            }
        }

        if(exists && hasContent && dataWriterDataRule == "new") {
            QFileInfo fileCandidate;
            QString fileNameCandidate;
            bool nameInvented = false;
            int nameIter = 1;
            QString tryBase = dataFile.absolutePath() + '/' + dataFile.completeBaseName();
            tryBase = SupportFunctions::stripIfInventedName(tryBase);
            // TODO: proper exception handling
            while(!nameInvented) {
                nameIter++;
                fileNameCandidate = (tryBase + "_RunD" + QString::number(nameIter) + '.' + dataFile.completeSuffix());
                if(nameIter >=65000)
                    fileNameCandidate = (tryBase + "_TooManyRunsD" + '.' + dataFile.completeSuffix());
                //std::cout << fileNameCandidate.toStdString() << std::endl;
                fileCandidate = QFileInfo(fileNameCandidate);
                nameInvented = !fileCandidate.exists();
            }
            // mkdir(".") DOES NOT WORK ON MACOS, ONLY WINDOWS. (Reported on MacOS 12.7.6 and Windows 10)
            //outputDirectory.mkdir(".");
            dataFile = fileCandidate;
        }
        //std::cout << dataFile.absolutePath().toStdString() << std::endl;
        //std::cout << dataFile.fileName().toStdString() << std::endl;
        //std::cout << dataFile.completeSuffix().toStdString() << std::endl;
        return dataFile.absolutePath() + '/' + dataFile.fileName();
    };

    // Shortens a long string for display on the GUI, depending on allowed max character length
    // e.g. from C:/something/another/file.csv it makes C:/somethin...er/file.csv
    static QString shortenStringForDisplay(const QString &str, int numMaxCharacters) {

        if(numMaxCharacters < 5) {
            std::cerr << "Cannot shorten string to fit on less than 5 characters, using 5 now" << std::endl;
            numMaxCharacters = 5;
        }

        if(str.length() < numMaxCharacters)
            return str;

        int numCharsOneSide = floor(numMaxCharacters/2)-2;
        return QString(str.mid(0, numCharsOneSide) + "..." + str.mid(str.length()-numCharsOneSide, numCharsOneSide));

    };

    /**
     * If previously loaded discrete ROI is valid, returns same discrete ROI, otherwise returns one at 60%.
    */
    static QRectF calculateRoiD(const QRectF imageRect, const QRectF discreteROI, const QRectF rationalROI, const float defaultRatio = 0.6f)
    {
        QRectF newRoi;

        if (!rationalROI.isEmpty())
        {
            if (isValidRoi(imageRect, discreteROI, rationalROI))
            {
                return getRectDiscreteFromRational(imageRect, rationalROI);
            }
        }
        return getDefaultRectD(imageRect, imageRect.center(), defaultRatio);
    }

    /**
     * If previously loaded rational ROI is valid, returns same rational ROI, otherwise returns one at 60%. 
    */
    static QRectF calculateRoiR(const QRectF imageRect, const QRectF discreteROI, const QRectF rationalROI, const float defaultRatio = 0.6f)
    {
        QRectF newRoi;

        if (!rationalROI.isEmpty())
        {
            if (isValidRoi(imageRect, discreteROI, rationalROI))
            {
                return rationalROI;
            }
        }
        return getDefaultRectR(imageRect, imageRect.center(), defaultRatio);
    }

    /**
     * Returns a 4:3 ratio discrete ROI with size of defaultRatio.
    */
    static QRectF getDefaultRectD(const QRectF imageRect, const QPointF center, const float defaultRatio)
    {
        int height = 0;
        int width = 0;
        if (imageRect.height() < imageRect.width())
        {
            height = std::round(imageRect.height() * defaultRatio);
            width = std::round((height / 3) * 4);
        }
        else
        {
            width = std::round(imageRect.width() * defaultRatio);
            height = std::round((width / 4) * 3);
        }
        QRectF newRoi = QRectF(center.x() - width / 2, center.y() - height / 2, width, height);
        return newRoi;
    }

    /**
     * Returns a 4:3 ratio rational ROI with size of defaultRatio.
    */
    static QRectF getDefaultRectR(const QRectF imageRect, const QPointF center, const float defaultRatio)
    {
        QRectF rectR = getDefaultRectD(imageRect, center, defaultRatio);
        return QRectF(rectR.topLeft().x() / imageRect.width(), rectR.topLeft().y() / imageRect.height(),
                      rectR.width() / imageRect.width(), rectR.height() / imageRect.height());
    }

    /**
     * Test if the loaded rational ROI ratios matches
    */
    static bool isValidRoi(const QRectF imageRect, const QRectF discreteROI, const QRectF rationalROI)
    {
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

    static QRectF getRectDiscreteFromRational(const QRectF imageRect, const QRectF rationalROI)
    {
        return getRectDiscreteFromRational(QSizeF(imageRect.width(), imageRect.height()), rationalROI);
    }

    static QRectF getRectDiscreteFromRational(const QSizeF size, const QRectF rationalROI)
    {
        return QRectF(rationalROI.x() * size.width(), rationalROI.y() * size.height(),
                      rationalROI.width() * size.width(), rationalROI.height() * size.height());
    }

    static QColor changeColors(QColor color, bool doLighten, bool isEnabled) {

        //if(color == Qt::darkRed) {
        //    qDebug() << "valami";
        //    qDebug() << color.valueF();
        //}

        // invert only the HSV "value"/intensity value (mirror it to 0.5 on a 0.0-1.0 range)
        if(doLighten && color.valueF() <= 0.52f) {
            color = QColor::fromHsvF(color.hsvHueF(), color.hsvSaturationF(), 1.0f-(color.valueF()/2.0));
        }

        if(doLighten) {
            if(!isEnabled) {
                color = QColor::fromHsvF(color.hsvHueF(), 0.8, 0.5);
            }
        } else {
            if(!isEnabled) {
                color = QColor::fromHsvF(color.hsvHueF(), 0.1, 0.7);
            }
        }

        return color;
    };

    // This function was in videoView but moved here as other classes may use it in future
    // Source Andy Maloney: https://github.com/asmaloney/asmOpenCV/blob/master/asmOpenCV.h
    static inline QImage cvMatToQImage(const cv::Mat &inMat)
    {
        switch (inMat.type())
        {
            // 8-bit, 4 channel
            case CV_8UC4:
            {
                QImage image(inMat.data,
                             inMat.cols, inMat.rows,
                             static_cast<int>(inMat.step),
                             QImage::Format_ARGB32);

                return image;
            }

                // 8-bit, 3 channel
            case CV_8UC3:
            {
                QImage image(inMat.data,
                             inMat.cols, inMat.rows,
                             static_cast<int>(inMat.step),
                             QImage::Format_RGB888);

                return image.rgbSwapped();
            }

                // 8-bit, 1 channel
            case CV_8U:
            {
#if QT_VERSION >= 0x050500

                // From Qt 5.5
                QImage image(inMat.data, inMat.cols, inMat.rows,
                             static_cast<int>(inMat.step),
                             QImage::Format_Grayscale8);
#else
                static QVector<QRgb>  sColorTable;

                // only create our color table the first time
                if (sColorTable.isEmpty())
                {
                    sColorTable.resize(256);
                    for (int i = 0; i < 256; ++i)
                    {
                        sColorTable[i] = qRgb(i, i, i);
                    }
                }

                QImage image(inMat.data,
                    inMat.cols, inMat.rows,
                    static_cast<int>(inMat.step),
                    QImage::Format_Indexed8);

                image.setColorTable(sColorTable);
#endif
                return image;
            }

            default:
                std::cerr << "cvMatToQImage() - cv::Mat image type not handled in switch:" << inMat.type() << std::endl;
                break;
        }

        return QImage();
    }

    // Reads out a bool value from QSettings in a safe way, to surely work around a that sometimes the bool values
    // are saved as textual true/false values, but anything other than number 0 will be read as true,
    // ultimately causing any state to be read as true
    static bool readBoolFromQSettings(QString keyString, bool defaultState, QSettings *applicationSettings) {
        const QByteArray readData = applicationSettings->value(keyString, QString::number((int)defaultState)).toByteArray();
        //std::cout << m_metaSnapshotsEnabled.toStdString() << std::endl; //
        if (readData.isEmpty()) {
            return defaultState;
        }
        if (readData == "1" || readData == "true")
            return true;
        else
            return false;
    }

    static void setSmallerLabelFontSize(QLabel *label) {
        QFont actualFont = label->font();
        int newFontSize = (int)(actualFont.pointSizeF()*0.85F);
        actualFont.setPointSize(newFontSize);
        label->setFont(actualFont);
    }
};