
#include <QtCore/qstandardpaths.h>
#include <QtConcurrent>
#include <opencv2/imgcodecs.hpp>
#include <QtCore/qthreadpool.h>
#include "imageWriter.h"
#include "supportFunctions.h"

// Creates a new image writer that outputs images in the given directory
// If stereo is true, a stereo directory structure is created in the given directory
ImageWriter::ImageWriter(QObject *parent) :
    QObject(parent),
    //stereoMode(stereo),
    applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), parent)) {

    // TODO: is this advised like this, if we are inside a thread ?
    imageWriterFormatString = applicationSettings->value("imageWriterFormat.chosenFormat", "tiff").toString();

    if(imageWriterFormatString == "png") {
        int pngCompression = applicationSettings->value("imageWriterFormat.png.compression", "0").toInt();
        writeParams = {cv::IMWRITE_PNG_COMPRESSION , pngCompression};
        qDebug() << "PNG compression: " << pngCompression;
    } else if(imageWriterFormatString == "jpeg") {
        int jpegQuality = applicationSettings->value("imageWriterFormat.jpeg.quality", "100").toInt();
        writeParams = {cv::IMWRITE_JPEG_QUALITY, jpegQuality};
        qDebug() << "JPEG quality: " << jpegQuality;
    } else if(imageWriterFormatString == "webp") {
        int webpQuality = applicationSettings->value("imageWriterFormat.webp.quality", "100").toInt();
        writeParams = {cv::IMWRITE_WEBP_QUALITY, webpQuality};
        qDebug() << "WEBP quality:" << webpQuality;
    } else {
        writeParams = std::vector<int>();
    }

    /*
    outputDirectory = QDir(directory);

    if(stereoMode) {
        outputDirectorySecondary = outputDirectory;
        if(!outputDirectory.exists("0")) {
            outputDirectory.mkdir("0");
        }
        outputDirectory.cd("0");

        if(!outputDirectorySecondary.exists("1")) {
            outputDirectorySecondary.mkdir("1");
        }
        outputDirectorySecondary.cd("1");
    }
    */
}

ImageWriter::~ImageWriter() {
    stopWriting();
};

bool ImageWriter::prepareForWriting(const QString& imageOutputTarget, bool stereo) {

    stereoMode = stereo;

    // TODO: there may be folders whose name ends with zip or .zip .. ?
    qDebug() << "imageOutputTarget = " << imageOutputTarget;

    if (imageOutputTarget.endsWith(".zip")) {
        imageWriterTarget = IWTARGET_ZIP;
        outputZip = imageOutputTarget;
        outputZipInnerRootDirectory = imageOutputTarget;
        outputZipInnerRootDirectory.chop(4);
        outputZipInnerRootDirectory = outputZipInnerRootDirectory.mid(outputZipInnerRootDirectory.lastIndexOf("/")+1, outputZipInnerRootDirectory.length()-(outputZipInnerRootDirectory.lastIndexOf("/")+1));
        //qDebug() << outputZipInnerRootDirectory;
        // These need to be set here already, becuase we would like to extract the content already of these files, if we can
        metaSnapshotFileName = outputZipInnerRootDirectory + "/imagerec_meta.xml";
        offlineEventLogFileName = outputZipInnerRootDirectory + "/offline_event_log.xml";

        foundZipAlreadyExist = QFile(imageOutputTarget).exists();
        imageOutputTargetZip = new QuaZip(imageOutputTarget);

        auto openMode = QuaZip::mdCreate;
        if(foundZipAlreadyExist) {
            openMode = QuaZip::mdAdd;

            // IMPORTANT: We need to carefully make a copy of all the filenames found in the file upon opening,
            //  because later we cannot access it without closing the file beforehand. I do not know if this is
            //  meant to be a bug or a feature, but certainly this is the case for the version I tested with, QuaZip 1-4.
            QuaZip fcz(imageOutputTarget);
            if(!fcz.open(QuaZip::mdUnzip)) {
                qWarning("QuaZip error during: open(): %d", imageOutputTargetZip->getZipError());
                //E.g. unexpected enf of file error (-1000)
                imageWriterStatus = IWSTATUS_ZIP_UNOPENABLE;
                return false;
            }
            foundFileInfoList = fcz.getFileInfoList();
            foundFileNameList = fcz.getFileNameList();

            QuaZipFile fcf(&fcz);
            bool ok = true;
            QByteArray a;
            ok &= fcz.setCurrentFile(metaSnapshotFileName);
            ok &= fcf.open(QIODevice::ReadOnly);
            a = fcf.readAll();
            ok &= (fcf.getZipError() == UNZ_OK);
            ok &= fcf.atEnd();
            fcf.close();
            ok &= (fcf.getZipError() == UNZ_OK);
            if(ok) {
                foundMetaSnapshotContent = a;
            }
            ok = true;
            a.clear();
            ok &= fcz.setCurrentFile(offlineEventLogFileName);
            ok &= fcf.open(QIODevice::ReadOnly);
            a = fcf.readAll();
            ok &= (fcf.getZipError() == UNZ_OK);
            ok &= fcf.atEnd();
            fcf.close();
            ok &= (fcf.getZipError() == UNZ_OK);
            if(ok) {
                foundOfflineEventLogContent = a;
            }

            fcz.close();
        }

        // TODO: if existing zip is found, append to it
        imageOutputTargetZip = new QuaZip(imageOutputTarget);
        if (!imageOutputTargetZip->open(openMode)) {
            qWarning("QuaZip error during: open(): %d", imageOutputTargetZip->getZipError());
            //E.g. unexpected enf of file error (-1000)
            imageWriterStatus = IWSTATUS_ZIP_UNOPENABLE;
            return false;
        }
        // TODO: There are many supported QTextCodecs in Qt6. Should we use them?
        //  E.g. in case the offline_event_log.xml contains messages in some unusual format.. ?
        //  But that file is already produced with some default Qt encoding settings.. we should check that too.
        // imageSourceZip->setFileNameCodec("IBM866");

        imageOutputTargetZipInnerFile = new QuaZipFile(imageOutputTargetZip);

        //int imencodeBufferSizeMb = 20;
        //imencodeBuffer.resize(imencodeBufferSizeMb* 1024*1024);

    } else {

        imageWriterTarget = IWTARGET_DIRECTORY;
        outputDirectory = imageOutputTarget;
        metaSnapshotFileName = imageOutputTarget + "/imagerec_meta.xml";
        offlineEventLogFileName = imageOutputTarget + "/offline_event_log.xml";

        bool ok = true;

        // Prepare the writeable directories
        QDir directory = QDir(imageOutputTarget);
        if (stereoMode) {
            if (!directory.exists("0")) {
                ok &= directory.mkdir("0");
            }
            if (!directory.exists("1")) {
                ok &= directory.mkdir("1");
            }
        }
        if(!ok) {
            imageWriterStatus = IWSTATUS_ERROR;
            return ok;
        }

        QFile msf(metaSnapshotFileName);
        if(msf.exists() && msf.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream in(&msf);
            //qDebug() << msf.size() << msf.readAll();
            foundMetaSnapshotContent = msf.readAll();
        }
        QFile oel(offlineEventLogFileName);
        if(oel.exists() && oel.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream in(&oel);
            //qDebug() << oel.size() << oel.readAll();
            foundOfflineEventLogContent = oel.readAll();
        }
    }
    imageWriterStatus = IWSTATUS_OK;
    return true;
};

void ImageWriter::stopWriting() {
    if(imageWriterTarget == IWTARGET_ZIP) {
        if(!imageOutputTargetZipInnerFile || !imageOutputTargetZip) {
            return;
        }
        if(imageOutputTargetZipInnerFile->isOpen()) {
            imageOutputTargetZipInnerFile->close();
            if (imageOutputTargetZipInnerFile->getZipError() != UNZ_OK) {
                qWarning("QuaZip error during: file.close(): %d", imageOutputTargetZipInnerFile->getZipError());
            }
        }
        imageOutputTargetZip->close();
        if (imageOutputTargetZip->getZipError() != UNZ_OK) {
            qWarning("QuaZip error during: close(): %d", imageOutputTargetZip->getZipError());
        }
        delete imageOutputTargetZipInnerFile;
        delete imageOutputTargetZip;
        imageOutputTargetZipInnerFile = nullptr;
        imageOutputTargetZip = nullptr;
    }
    imageWriterStatus = ImageWriter::IWSTATUS_UNDETERMINED;
    foundZipAlreadyExist = false;
    foundFileInfoList.clear();
    foundFileNameList.clear();
    metaSnapshotFileName = "";
    offlineEventLogFileName = "";
    foundMetaSnapshotContent = "";
    foundOfflineEventLogContent = "";
};

// Slot callback which receives new camera images
// Write the received image to disk using the specified image format
// File writing is executed using Qts concurrent to not block the GUI thread for writing
void ImageWriter::onNewImage(const CameraImage &img) {

    // TODO: beautify

    bool ok = true;
    qDebug() << "Saving image at recording timestamp: " << QString::number(img.timestamp);

    QString fileName;
    if (imageWriterTarget == IWTARGET_ZIP) {

        if (stereoMode) {
            fileName = outputZipInnerRootDirectory + "/0/" + QString::number(img.timestamp) + "." + imageWriterFormatString;
            cv::imencode(('.'+imageWriterFormatString).toStdString(), img.img, imencodeBuffer, writeParams);
            ok &= imageOutputTargetZipInnerFile->open(QIODevice::WriteOnly, QuaZipNewInfo(fileName), nullptr, 0, 0);
            imageOutputTargetZipInnerFile->write(
                    reinterpret_cast<const char *>(imencodeBuffer.data()),
                    static_cast<int>(imencodeBuffer.size()));
            ok &= (imageOutputTargetZipInnerFile->getZipError() == UNZ_OK);
            imageOutputTargetZipInnerFile->close();
            ok &= (imageOutputTargetZipInnerFile->getZipError() == UNZ_OK);

            fileName = outputZipInnerRootDirectory + "/1/" + QString::number(img.timestamp) + "." + imageWriterFormatString;
            cv::imencode(('.'+imageWriterFormatString).toStdString(), img.imgSecondary, imencodeBuffer, writeParams);
            ok &= imageOutputTargetZipInnerFile->open(QIODevice::WriteOnly, QuaZipNewInfo(fileName), nullptr, 0, 0);
            imageOutputTargetZipInnerFile->write(
                    reinterpret_cast<const char *>(imencodeBuffer.data()),
                    static_cast<int>(imencodeBuffer.size()));
            ok &= (imageOutputTargetZipInnerFile->getZipError() == UNZ_OK);
            imageOutputTargetZipInnerFile->close();
            ok &= (imageOutputTargetZipInnerFile->getZipError() == UNZ_OK);
        } else {
            fileName = outputZipInnerRootDirectory + "/" + QString::number(img.timestamp) + "." + imageWriterFormatString;
            cv::imencode(('.'+imageWriterFormatString).toStdString(), img.img, imencodeBuffer, writeParams);
            ok &= imageOutputTargetZipInnerFile->open(QIODevice::WriteOnly, QuaZipNewInfo(fileName), nullptr, 0, 0);
            imageOutputTargetZipInnerFile->write(
                    reinterpret_cast<const char *>(imencodeBuffer.data()),
                    static_cast<int>(imencodeBuffer.size()));
            ok &= (imageOutputTargetZipInnerFile->getZipError() == UNZ_OK);
            imageOutputTargetZipInnerFile->close();
            ok &= (imageOutputTargetZipInnerFile->getZipError() == UNZ_OK);
        }

    } else if (imageWriterTarget == IWTARGET_DIRECTORY) {

        if (stereoMode) {
            fileName = outputDirectory + "/0/" + QString::number(img.timestamp) + "." + imageWriterFormatString;
            //QtConcurrent::run(cv::imwrite, filepath.toStdString(), img.img, writeParams);
            ok &= cv::imwrite(fileName.toStdString(), img.img, writeParams);
            fileName = outputDirectory + "/1/" + QString::number(img.timestamp) + "." + imageWriterFormatString;
            //QtConcurrent::run(cv::imwrite, filepathSecondary.toStdString(), img.imgSecondary, writeParams);
            ok &= cv::imwrite(fileName.toStdString(), img.imgSecondary, writeParams);
        } else {
            // Write every image over a thread pool managed by QT, this way nothing blocks and we can write images very fast (cpu heavy)
            fileName = outputDirectory + "/" + QString::number(img.timestamp) + "." + imageWriterFormatString;
            //QtConcurrent::run(cv::imwrite, filepath.toStdString(), img.img, writeParams);
            ok &= cv::imwrite(fileName.toStdString(), img.img, writeParams);
        }
    }

    if (!ok) {
        emit writingFailed();
    }
}

bool ImageWriter::writeMetaSnapshot(const QString &content) {

    // NOTE: if we append to an existing recording, currently the meta snapshot that
    //  was already inside there, will be duplicated (as zips allow storing multiple
    //  files under the same name)
    bool ok = true;
    if(imageWriterTarget == IWTARGET_ZIP) {
        // NOTE: currently this is not handled differently. Not at this level (.e.g. append to the file content blindly),
        // and not at higher level (append the contained DOM structure nodes) either. This could be improved later,
        // to let the meta snapshot hold snapshots of different image recording start/stop events, ultimately storing a
        // complete snapshot of important application and camera settings, known at one point in time.
        // TODO: low priority

        //if(foundZipAlreadyExist && foundFileNameList.contains(metaSnapshotFileName)) {
        //    ok &= imageOutputTargetZipInnerFile->open(QIODevice::Append, QuaZipNewInfo(metaSnapshotFileName));
        //} else {
            ok &= imageOutputTargetZipInnerFile->open(QIODevice::WriteOnly, QuaZipNewInfo(metaSnapshotFileName), nullptr, 0, 0);
        //}
        auto bytesWritten = imageOutputTargetZipInnerFile->write(content.toUtf8());
        ok &= (imageOutputTargetZipInnerFile->getZipError() == UNZ_OK);
        imageOutputTargetZipInnerFile->close();
        ok &= (imageOutputTargetZipInnerFile->getZipError() == UNZ_OK);

        return ok;
    } else if(imageWriterTarget == IWTARGET_DIRECTORY) {

        QFile *dataFile = new QFile(metaSnapshotFileName);
        //if(dataFile->exists() == false) {
        //    return;
        //}

        // NOTE: if the data file exists, just append to it. Events from the existing recording are not read, but kept.
        if (!dataFile->open(QIODevice::WriteOnly /*| QIODevice::ReadWrite*/ | QIODevice::Text)) {
            std::cout << "Recording failure. Could not open: " << metaSnapshotFileName.toStdString() << std::endl;
            delete dataFile;
            dataFile = nullptr;

            return false;
        }

        QTextStream *textStream = new QTextStream(dataFile);
        *textStream << content;

        std::cout << metaSnapshotFileName.toStdString() << std::endl;

        if(dataFile) {
            dataFile->close();
            dataFile->deleteLater();
        }
        delete textStream;
        delete dataFile;
        dataFile = nullptr;
        textStream = nullptr;
    } else {
        qDebug() << "Error during writing meta snapshot.";
        return false;
    }
    return ok;
}

bool ImageWriter::writeOfflineEventLog(const QString &content) {

    bool ok = true;
    if(imageWriterTarget == IWTARGET_ZIP) {
        // IMPORTANT: the file is appended on a higher level, i.e. not the actual file content byte arrays are put together,
        //  but the xml DOM structure is opened, appended to, and then serialized again, elsewhere. Yet no need to treat
        //  an existing file differently, but the this code snippet is left here.
        //if(foundZipAlreadyExist && foundFileNameList.contains(offlineEventLogFileName)) {
        //    ok &= imageOutputTargetZipInnerFile->open(QIODevice::Append, QuaZipNewInfo(offlineEventLogFileName));
        //} else {
            ok &= imageOutputTargetZipInnerFile->open(QIODevice::WriteOnly, QuaZipNewInfo(offlineEventLogFileName), nullptr, 0, 0);
        //}
        imageOutputTargetZipInnerFile->write(content.toUtf8());
        ok &= (imageOutputTargetZipInnerFile->getZipError() == UNZ_OK);
        imageOutputTargetZipInnerFile->close();
        ok &= (imageOutputTargetZipInnerFile->getZipError() == UNZ_OK);

        return ok;
    } else if(imageWriterTarget == IWTARGET_DIRECTORY) {

        QFile *dataFile = new QFile(offlineEventLogFileName);
        //if(dataFile->exists() == false) {
        //    return;
        //}

        if (!dataFile->open(QIODevice::WriteOnly /*| QIODevice::ReadWrite*/ | QIODevice::Text)) {
            std::cout << "Recording failure. Could not open: " << offlineEventLogFileName.toStdString() << std::endl;
            delete dataFile;
            dataFile = nullptr;

            return false;
        }

        QTextStream *textStream = new QTextStream(dataFile);
        *textStream << content;

        std::cout << offlineEventLogFileName.toStdString() << std::endl;

        if(dataFile) {
            dataFile->close();
            dataFile->deleteLater();
        }
        delete textStream;
        delete dataFile;
        dataFile = nullptr;
        textStream = nullptr;
    } else {
        qDebug() << "Error during writing offline event log.";
        return false;
    }
    return ok;
}
