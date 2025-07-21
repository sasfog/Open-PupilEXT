
#include <opencv2/core/utility.hpp>
#include <opencv2/opencv.hpp>
#include <QtConcurrent/QtConcurrent>
#include "imageReader.h"

// Creates a new image reader which opens the given imageSource and plays back the contained image files
// Image files are read in alphabetically order, using the OpenCV function cv::glob()
// Actual playback process is performed using Qts concurrent thread execution, to no block the GUI thread
// First it is checked wherever a stereo imageSource structure exists or not, which
ImageReader::ImageReader(QString imageSource, int subrecordingNumber, QMutex *imageMutex, QWaitCondition *imagePublished, QWaitCondition *imageProcessed, int playbackSpeed, bool playbackLoop, QObject *parent) :
    QObject(parent),
    startTimestamp(0),
    playbackSpeed(playbackSpeed),
    noDelay(false),
    stereoMode(false),
    synchronised(false),
    playbackLoop(playbackLoop),
    state(PlaybackState::STOPPED),
    imageMutex(imageMutex),
    imagePublished(imagePublished),
    imageProcessed(imageProcessed),
    currentImageIndex(0) {

    qDebug() << "imageSource = " << imageSource;
    // NOTE: There might be directory names that end with "zip" or ".zip" too

    if(imageSource.endsWith(zipSuffix) && QFile(imageSource).exists()) {
        qDebug() << "Image source seems to be a Zip archive.";
        imageReaderSource = IMSOURCE_ZIP;
        exploreZip(imageSource, subrecordingNumber);

    } else if( QDir(imageSource).exists() ) {
        QDir imageSourceDir = QDir(imageSource);
        QList<QFileInfo> fil;

        // Check if in imageSource, a stereo structure with directories 0 and 1 for main and secondary camera are present
        if (imageSourceDir.exists("0") && imageSourceDir.exists("1")) {
            stereoMode = true;
            //fileNames[0] = QDir(imageSourceDir.filePath("0")).entryList(QStringList() << "*.*", QDir::Files);
            //fileNames[1] = QDir(imageSourceDir.filePath("1")).entryList(QStringList() << "*.*", QDir::Files);
            fil = QDir(imageSourceDir.filePath("0")).entryInfoList(QStringList() << "*.*", QDir::Files);
            for (const QFileInfo &fileInfo : fil) {
                fileNames[0] << fileInfo.absoluteFilePath();
            }
            fil = QDir(imageSourceDir.filePath("1")).entryInfoList(QStringList() << "*.*", QDir::Files);
            for (const QFileInfo &fileInfo : fil) {
                fileNames[1] << fileInfo.absoluteFilePath();
            }

            assert(fileNames[0].size() == fileNames[1].size());
        } else {
            //fileNames[0] = QDir(imageSourceDir.path()).entryList(QStringList() << "*.*", QDir::Files);
            fil = QDir(imageSourceDir.path()).entryInfoList(QStringList() << "*.*", QDir::Files);
            for (const QFileInfo &fileInfo : fil) {
                fileNames[0] << fileInfo.absoluteFilePath();
            }
        }

        imageReaderSource = IMSOURCE_DIRECTORY;
        qDebug() << "Image source seems to be a directory.";

        QString suggestedXmlsLocation = imageSourceDir.path();
        QString fPathAndName;

        fPathAndName = suggestedXmlsLocation + '/' + "offline_event_log.xml";
        qDebug() << "Expected offline event log fPathAndName = " << fPathAndName;
        if( QFileInfo(fPathAndName).exists() ) {
            QFile f(fPathAndName);
            if (!f.open(QIODevice::ReadOnly)) {
                std::cout << "Could not open XML file. Check file availability or file access permission.";
            } else {
                offlineEventLogContent = f.readAll();
            }
        }
        fPathAndName = suggestedXmlsLocation + '/' + "imagerec_meta.xml";
        qDebug() << "Expected image rec meta snapshot fPathAndName = " << fPathAndName;
        if( QFileInfo(fPathAndName).exists() ) {
            QFile f(fPathAndName);
            if (!f.open(QIODevice::ReadOnly)) {
                std::cout << "Could not open XML file. Check file availability or file access permission.";
            } else {
                metaSnapshotContent = f.readAll();
            }
        }

        /*
        bool ok;
        for(int u = 0; u < fileNames[0].size(); u++) {
        acqTimestamps.push_back( QFileInfo(fileNames[0][u]).baseName().toULongLong(&ok, 10) );
        }
        */

    } else {
        // TODO: better handling of problem cases, and have fallback
        qDebug() << "Image source as a directory does not exist either.";
        return;
    }

    // TODO: add check to ensure the same timestamp has images in both directories
    fileNames[0].sort();
    fileNames[0] = purgeFileNamesVector(fileNames[0]);
    if(stereoMode) {
        fileNames[1].sort();
        fileNames[1] = purgeFileNamesVector(fileNames[1]);
    }

    acqTimestamps = extractAcqTimestamps(fileNames[0]);
//    qDebug()<<"ImageReader: found " << fileNames[0].size() << " images. Ready." ;

    cv::Mat checkImg = getStillImageSingle(0);
    foundImageWidth = checkImg.cols;
    foundImageHeight = checkImg.rows;
    /*
    // GB: measure found images px size, for documenting in meta-snapshot
    int b=0;
    cv::Mat checkImg;
    while(b < fileNames[0].size() && (foundImageWidth<=0 || foundImageHeight<=0)) {
        checkImg = cv::imread(fileNames[0][b].toStdString(), cv::IMREAD_GRAYSCALE);
        foundImageWidth = checkImg.cols;
        foundImageHeight = checkImg.rows;
    }
    */

    setPlaybackSpeed(playbackSpeed);
}

std::vector<quint64> ImageReader::extractAcqTimestamps(const QStringList &fileNameCandidates) {

    bool ok;
    std::vector<quint64> timestamps;
    for(auto fnc : fileNameCandidates) {

        if( auto lip = fnc.lastIndexOf('/') ) {
            fnc = fnc.mid(lip + 1, fnc.length() - ((lip + 1)));
        }
        if( auto lid = fnc.lastIndexOf('.') ) {
            fnc = fnc.mid(0, lid);
        }
        timestamps.push_back( fnc.toULongLong(&ok, 10) );
    }
    return timestamps;
}

void ImageReader::exploreZip(const QString &imageSource, const int &subrecordingNumber) {

    imageSourceZip = new QuaZip(imageSource);
    if (!imageSourceZip->open(QuaZip::mdUnzip)) {
        qWarning("QuaZip error during: open(): %d", imageSourceZip->getZipError());
        imageReaderStatus = IMSTATUS_ZIP_UNOPENABLE;
        return;
    }
    // TODO: There are many supported QTextCodecs in Qt6. Should we detect and use them?
    //  E.g. in case the offline_event_log.xml contains messages in some unusual format.. ?
    //  But that file is already produced with some default Qt encoding settings.. we should check that too.
    // imageSourceZip->setFileNameCodec("IBM866");

    qDebug("Zip file contains %d entries", imageSourceZip->getEntriesCount());
    qDebug("Zip comment: %s", imageSourceZip->getComment().toLocal8Bit().constData());

    imageSourceZipInnerFile = new QuaZipFile(imageSourceZip);

    auto fileNameCandidates = QStringList::fromList(imageSourceZip->getFileNameList());
    //qDebug() << "fileNameCandidates = " << fileNameCandidates;
    //imageSourceZip->setCurrentFile();

    // Assumptions, in case there are more than one recordings in one zip file:
    // - all the recordings must have the same folder structure (all single or all mono)
    // - all the recordings must have the same image extensions

    QVector<QString> zS_recordingNames;
    //QVector<int> zS_recordingLengths;
    QString zS_recordingName = "";
    QVector<QStringList> zS_fileNames = {QStringList(), QStringList()};
    //QVector <QString> zS_filePaths = {"", ""};
    QString zS_offlineEventLogPathAndName = "";
    QString zS_metaSnapshotPathAndName = "";

    // what is the most frequent file (image) extension inside the zip file
    QString mostFrequentExtension = findMostFrequentExtension(fileNameCandidates);

    // exploring the levels of folders stepping up one-by-one from the level of images
    // all image recordings must be of the same level (single or stereo) inside a recording zip,
    // i.e. the same number of "/" sharacters must appear for all images of the most frequent image extension.
    QVector<QStringList> uniqueDirNames = {QStringList(), QStringList(), QStringList()};
    for(const auto &str : fileNameCandidates) {
        if(str.endsWith(mostFrequentExtension)) {
            QStringList lst = str.split('/');
            if(lst.count() >= 2) { uniqueDirNames[0].append(lst[lst.count()-2]); }
            if(lst.count() >= 3) { uniqueDirNames[1].append(lst[lst.count()-3]); }
            if(lst.count() >= 4) { uniqueDirNames[2].append(lst[lst.count()-4]); }
        }
    }
    uniqueDirNames[0].removeDuplicates();
    uniqueDirNames[1].removeDuplicates();
    uniqueDirNames[2].removeDuplicates();
    //qDebug() << "----- directories at level -1:" << uniqueDirNames[0];
    //qDebug() << "----- directories at level -2:" << uniqueDirNames[1];
    //qDebug() << "----- directories at level -3:" << uniqueDirNames[2];

    // Step 1: determine the possible found recording names
    if(uniqueDirNames[0].size() < 1) {
        // There is no directory inside, just the bunch of images in the zip.
        //          -> The name of the zip file will be deemed as the name of the recording.
        qDebug() << "Case #1: Assuming that there is one single recording in one Zip file, with no containing directory.";
        stereoMode = false;

        auto rn = imageSource;
        rn = rn.mid( rn.length()-(zipSuffix.length()+1), (zipSuffix.length()+1) );
        rn = rn.mid( rn.lastIndexOf('/')+1, rn.length()-((rn.lastIndexOf('/')+1)) );
        zS_recordingNames.append(rn);

    } else if(uniqueDirNames[0].size() == 1) {
        // There is (at least a "-1" directory level) with the last exactly 1 unique name, inside for all the images.
        //          -> The name of that directory file will be deemed as the name of the recording.
        qDebug() << "Case #2: Assuming that there is one single recording in one Zip file, with a containing directory.";
        stereoMode = false;

        zS_recordingNames.append(uniqueDirNames[0][0]);

    } else if(uniqueDirNames[0].size() == 2) {
        // There is (at least a "-1" directory level) with the last exactly 2 unique names, inside for all the images.
        //  There are 2 possible cases now:
        //    - They are the 0 and 1 named directories for a stereo recording
        //      - if there is NO "-2" level directory,
        //          -> The name of the zip file will be deemed as the name of the recording.
        //      - if there is ONE "-2" level directory in which they are,
        //          -> That will be deemed as the name of the recording.
        //      - if there are >ONE "-2" level directories, then there are multiple STEREO recordings in this zip file.
        //          -> A GUI dialog must be asked
        //    - They are called something else than exactly 0 and 1, there must be multiple SINGLE recordings in this zip file.
        //          -> A GUI dialog must be asked

        if( uniqueDirNames[0][0] == "0" && uniqueDirNames[0][1] == "1" ) {
            qDebug() << "Cases #3-5: Assuming that there is one or more stereo recordings in one Zip file.";
            stereoMode = true;

            if(uniqueDirNames[1].size() < 1) {
                //  -> The name of the zip file will be deemed as the name of the recording.
                qDebug() << "Case #3: Assuming that there is one stereo recording in one Zip file, with no containing directory.";

                auto rn = imageSource;
                rn = rn.mid( rn.length()-(zipSuffix.length()+1), (zipSuffix.length()+1) );
                rn = rn.mid( rn.lastIndexOf('/')+1, rn.length()-((rn.lastIndexOf('/')+1)) );
                zS_recordingNames.append(rn);

            } else if(uniqueDirNames[1].size() == 1) {
                //  -> That will be deemed as the name of the recording.
                qDebug() << "Case #4: Assuming that there is one stereo recording in one Zip file, with a containing directory.";

                zS_recordingNames.append(uniqueDirNames[0][0]);

            } else if(uniqueDirNames[1].size() > 1) {
                //  -> A GUI dialog must be asked
                qDebug() << "Case #5: Assuming that there are multiple stereo recordings in one Zip file. Asking the user through GUI.";

                for(const auto &rn : uniqueDirNames[1]) {
                    zS_recordingNames.append(rn);
                    auto tfns = fileNameCandidates
                            .filter(rn + "/")
                            .filter(QRegularExpression(mostFrequentExtension + "$"));
                    foundZipMultiInfo.append({rn, getRecLenFromFileNamesList(tfns, mostFrequentExtension)});
                }
            }
        } else {
            //      -> A GUI dialog must be asked
            qDebug() << "Case #6: Assuming that there are multiple (2) single recordings in one Zip file. Asking the user through GUI.";
            stereoMode = false;

            for(const auto &rn : uniqueDirNames[0]) {
                zS_recordingNames.append(rn);
                auto tfns = fileNameCandidates
                        .filter(rn + "/")
                        .filter(QRegularExpression(mostFrequentExtension + "$"));
                foundZipMultiInfo.append({rn, getRecLenFromFileNamesList(tfns, mostFrequentExtension)});
            }
        }

    } else if(uniqueDirNames[0].size() >= 3) {
        // There is (at least a "-1" directory level) with the last more than 2 unique names, inside for all the images.
        //  There must be multiple SINGLE recordings in this zip file.
        //          -> A GUI dialog must be asked
        qDebug() << "Case #7: Assuming that there are multiple (>2) single recordings in one Zip file. Asking the user through GUI.";
        stereoMode = false;

        for(const auto &rn : uniqueDirNames[0]) {
            zS_recordingNames.append(rn);
            auto tfns = fileNameCandidates
                    .filter(rn + "/")
                    .filter(QRegularExpression(mostFrequentExtension + "$"));
            foundZipMultiInfo.append({rn, getRecLenFromFileNamesList(tfns, mostFrequentExtension)});
        }
    }

    // Step 2: select one from them, if we can
    if( zS_recordingNames.size() > 1 ) {
        if( subrecordingNumber != 0 ) {
            if( zS_recordingNames.length() < abs(subrecordingNumber) ) {
                imageReaderStatus = IMSTATUS_ERROR;
                return;
            } else {
                if(subrecordingNumber < 0) {
                    zS_recordingName = zS_recordingNames[zS_recordingNames.length() + subrecordingNumber];
                } else {//if(subrecordingNumber > 0)
                    zS_recordingName = zS_recordingNames[subrecordingNumber-1];
                }
            }
        } else {
            if( zS_recordingNames.length() > 1 ) {
                imageReaderStatus = IMSTATUS_ZIP_INDECISIVE;
                return;
            } else {
                if(subrecordingNumber < 0) {
                    zS_recordingName = zS_recordingNames[zS_recordingNames.length() + subrecordingNumber];
                } else {//if(subrecordingNumber > 0)
                    zS_recordingName = zS_recordingNames[subrecordingNumber-1];
                }
            }
        }
    } else if( zS_recordingNames.size() == 1 ){
        zS_recordingName = zS_recordingNames[0];
    } else {
        imageReaderStatus = IMSTATUS_ERROR;
        return;
    }

    // TODO: could be a switch-case depending on what we found above, for clarity
    // Step 3: retrieve fileNames vector(s) and suggested Xml locations
    if(uniqueDirNames[0].size() < 1) {
        // Case #1
        zS_fileNames[0] = fileNameCandidates
                .filter(QRegularExpression(mostFrequentExtension + "$"));
    } else if(uniqueDirNames[0].size() == 1) {
        // Case #2
        zS_fileNames[0] = fileNameCandidates
                .filter(uniqueDirNames[0][0] + "/")
                .filter(QRegularExpression(mostFrequentExtension + "$"));
    } else if(uniqueDirNames[0].size() == 2) {
        if( uniqueDirNames[0][0] == "0" && uniqueDirNames[0][1] == "1") {
            qDebug() << "Cases #3-5: Assuming that there is one or more stereo recordings in one Zip file.";
            if(uniqueDirNames[1].size() <= 1) {
                // Cases #3-4
                zS_fileNames[0] = fileNameCandidates
                        .filter("0/")
                        .filter(QRegularExpression(mostFrequentExtension + "$"));
                zS_fileNames[1] = fileNameCandidates
                        .filter("1/")
                        .filter(QRegularExpression(mostFrequentExtension + "$"));
            } else if(uniqueDirNames[1].size() > 1) {
                // Case #5
                zS_fileNames[0] = fileNameCandidates
                        .filter(zS_recordingName + "/0/")
                        .filter(QRegularExpression(mostFrequentExtension + "$"));
                zS_fileNames[1] = fileNameCandidates
                        .filter(zS_recordingName + "/1/")
                        .filter(QRegularExpression(mostFrequentExtension + "$"));
            }
        } else {
            // Case #6
            zS_fileNames[0] = fileNameCandidates
                    .filter(zS_recordingName + "/")
                    .filter(QRegularExpression(mostFrequentExtension + "$"));
        }
    } else if(uniqueDirNames[0].size() >= 3) {
        // Case #7
        zS_fileNames[0] = fileNameCandidates
                .filter(zS_recordingName + "/")
                .filter(QRegularExpression(mostFrequentExtension + "$"));
    }

    // Step 4: retrieve rec event log and meta snapshot
    auto offlineEventLogsFound = fileNameCandidates.filter(QRegularExpression("offline_event_log.xml$"));
    if(!offlineEventLogsFound.empty()) {
        zS_offlineEventLogPathAndName = offlineEventLogsFound[0];
        qDebug() << "zS_offlineEventLogPathAndName = " << zS_offlineEventLogPathAndName;
    } else {
        qDebug() << "zS_offlineEventLogPathAndName not found";
    }
    auto metaSnapshotsFound = fileNameCandidates.filter(QRegularExpression("imagerec_meta.xml$"));
    if(!metaSnapshotsFound.empty()) {
        zS_metaSnapshotPathAndName = metaSnapshotsFound[0];
        qDebug() << "zS_metaSnapshotPathAndName = " << zS_metaSnapshotPathAndName;
    } else {
        qDebug() << "zS_metaSnapshotPathAndName not found";
    }

    //qDebug() << "zS_fileNames[0] = " << zS_fileNames[0];
    //qDebug() << "zS_fileNames[1] = " << zS_fileNames[1];

    fileNames.clear();
    fileNames = {zS_fileNames[0], zS_fileNames[1]};

    QString sl = zS_fileNames[0][0];
    sl = sl.mid(0, sl.lastIndexOf("/"));
    if(stereoMode) {
        // Just one more level up
        sl = sl.mid(0, sl.lastIndexOf("/"));
    }

    QString suggestedXmlsLocation = sl;

    QString fPathAndName;
    //
    fPathAndName = suggestedXmlsLocation + '/' + "offline_event_log.xml";
    qDebug() << "Expected offline event log fPathAndName = " << fPathAndName;
    imageSourceZip->setCurrentFile(fPathAndName);
    imageSourceZipInnerFile->open(QIODevice::ReadOnly);
    offlineEventLogContent = QString(imageSourceZipInnerFile->readAll());
    imageSourceZipInnerFile->close();
    //
    fPathAndName = suggestedXmlsLocation + '/' + "imagerec_meta.xml";
    qDebug() << "Expected image rec meta snapshot fPathAndName = " << fPathAndName;
    imageSourceZip->setCurrentFile(fPathAndName);
    imageSourceZipInnerFile->open(QIODevice::ReadOnly);
    metaSnapshotContent = QString(imageSourceZipInnerFile->readAll());
    imageSourceZipInnerFile->close();
    //
    imageSourceZip->goToFirstFile();

}

ImageReader::~ImageReader() {
    if(state == PlaybackState::PLAYING) {
        state = PlaybackState::STOPPED;

        playbackProcess.waitForFinished();
    }

    if(imageReaderSource == IMSOURCE_ZIP) {
        if(imageSourceZipInnerFile->isOpen()) {
            imageSourceZipInnerFile->close();
            if (imageSourceZipInnerFile->getZipError() != UNZ_OK) {
                qWarning("QuaZip error during: file.close(): %d", imageSourceZipInnerFile->getZipError());
            }
        }
        imageSourceZip->close();
        if (imageSourceZip->getZipError() != UNZ_OK) {
            qWarning("QuaZip error during: close(): %d", imageSourceZip->getZipError());
        }
        delete imageSourceZipInnerFile;
        delete imageSourceZip;
        imageSourceZipInnerFile = nullptr;
        imageSourceZip = nullptr;
    }
}

bool ImageReader::quickReadImageSingle(cv::Mat &img, const int &imageIndex) {
    bool ok = true;
    try {
        if (imageReaderSource == IMSOURCE_DIRECTORY) {
            img = cv::imread(fileNames[0][imageIndex].toStdString(), cv::IMREAD_GRAYSCALE);
        } else { // if(imageReaderSource == IMSOURCE_ZIP) {
            QByteArray a;
            ok &= imageSourceZip->setCurrentFile(fileNames[0][imageIndex]);
            ok &= imageSourceZipInnerFile->open(QIODevice::ReadOnly);
            a = imageSourceZipInnerFile->readAll();
            ok &= (imageSourceZipInnerFile->getZipError() == UNZ_OK);
            ok &= imageSourceZipInnerFile->atEnd();
            imageSourceZipInnerFile->close();
            ok &= (imageSourceZipInnerFile->getZipError() == UNZ_OK);
            //qDebug() << "fileNames[0][currentImageIndex] = " << fileNames[0][imageIndex];

            img = cv::imdecode(cv::InputArray(std::vector<uchar>(a.begin(), a.end())), cv::IMREAD_GRAYSCALE);
            //ok &= !img.empty();
        }
    } catch (const std::exception &e) {
        qWarning() << "ImageReader encountered an error upon initialization: " << e.what();
        img = cv::Mat();
        ok = false;
        return ok;
    }
    return ok;
}

bool ImageReader::quickReadImageStereo(cv::Mat &img, cv::Mat &imgSecondary, const int &imageIndex) {
    bool ok = true;
    try {
        if (imageReaderSource == IMSOURCE_DIRECTORY) {
            // Read images from disk asynchronous to save time
            QFutureSynchronizer<cv::Mat> synchronizer;
            //auto a = QtConcurrent::run(QThreadPool::globalInstance(), cv::imread, fileNames[0][imageIndex].toStdString(), cv::IMREAD_GRAYSCALE);
            // IMPORTANT NOTE: this lambda encapsulation is necessary, because the QtConcurrent::run
            //  template function cannot otherwise infer the arguments correctly, since cv::imread was
            //  changed in the new (4.11.0) version of opencv
            synchronizer.addFuture(QtConcurrent::run([=]() {
                return cv::imread(fileNames[0][imageIndex].toStdString(), cv::IMREAD_GRAYSCALE);
            }));
            synchronizer.addFuture(QtConcurrent::run([=]() {
                return cv::imread(fileNames[1][imageIndex].toStdString(), cv::IMREAD_GRAYSCALE);
            }));
            synchronizer.waitForFinished();
            img = synchronizer.futures().at(0).result();
            imgSecondary = synchronizer.futures().at(1).result();
        } else { // if(imageReaderSource == IMSOURCE_ZIP) {
            QByteArray a;

            ok &= imageSourceZip->setCurrentFile(fileNames[0][imageIndex]);
            ok &= imageSourceZipInnerFile->open(QIODevice::ReadOnly);
            a = imageSourceZipInnerFile->readAll();
            ok &= (imageSourceZipInnerFile->getZipError() == UNZ_OK);
            ok &= imageSourceZipInnerFile->atEnd();
            imageSourceZipInnerFile->close();
            ok &= (imageSourceZipInnerFile->getZipError() == UNZ_OK);
            img = cv::imdecode(cv::InputArray(std::vector<uchar>(a.begin(), a.end())), cv::IMREAD_GRAYSCALE);
            //ok &= !img.empty();

            ok &= imageSourceZip->setCurrentFile(fileNames[1][imageIndex]);
            ok &= imageSourceZipInnerFile->open(QIODevice::ReadOnly);
            a = imageSourceZipInnerFile->readAll();
            ok &= (imageSourceZipInnerFile->getZipError() == UNZ_OK);
            ok &= imageSourceZipInnerFile->atEnd();
            imageSourceZipInnerFile->close();
            ok &= (imageSourceZipInnerFile->getZipError() == UNZ_OK);
            imgSecondary = cv::imdecode(cv::InputArray(std::vector<uchar>(a.begin(), a.end())), cv::IMREAD_GRAYSCALE);
            //ok &= !imgSecondary.empty();
        }
    } catch (const std::exception &e) {
        qWarning() << "ImageReader encountered an error upon initialization: " << e.what();
        img = cv::Mat();
        imgSecondary = cv::Mat();
        ok = false;
        return ok;
    }
    return ok;
}

// Sets the speed with which the images are played back
// This may be limited by the disk read speed and may not reach speeds above 30fps depending on the file sizes etc.
void ImageReader::setPlaybackSpeed(int fps) {
    if(fps <= 0) {
        noDelay = true;
        playbackSpeed = 0;
        playbackDelay = 33; // We set playback delay to some value so that the timestamps which are incremented by that delay are valid
    } else {
        noDelay = false;
        playbackSpeed = fps;
        playbackDelay = (int) (1000.0f / fps); // delay in ms
    }
}

// Executes the play back of the images through reading them in order, creating a CameraImage object and sending the onNewImage signal
// The play back can be stopped by changing the PlaybackState variable state
// When the playback is finished, a finished signal is send (also send when stopping the play back early)
void ImageReader::run() {

    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    while(currentImageIndex < fileNames[0].size()) {
        if (state != PlaybackState::PLAYING) {
//            qDebug() << "Image Reader: Run Loop found Stop/Pause signal" ;
            break;
        }

        std::chrono::steady_clock::time_point beginProcess = std::chrono::steady_clock::now();
        std::chrono::duration<int, std::milli> elapsedDuration = std::chrono::duration_cast<std::chrono::milliseconds>(beginProcess - startTime);
        int elapsedTime = elapsedDuration.count();
        cv::Mat img;
        if( quickReadImageSingle(img, currentImageIndex) &&
            img.data && elapsedTime >= playbackDelay) {

            if (synchronised){
                const QMutexLocker locker(imageMutex);
//                qDebug() << "imageReader locking";
//                qDebug() << "Current image index: " << currentImageIndex;
                runImpl(startTime, elapsedDuration, img);
                //qDebug() << "New Image generated, unlocking";
                imageProcessed->wakeAll();
                if (state == PlaybackState::PLAYING) {
//                    qDebug() << "Waiting for processing";
                    imagePublished->wait(imageMutex);
                }
            }
            else {
//                qDebug() << "Current image index: " << currentImageIndex;
                runImpl(startTime, elapsedDuration, img);
            }


            currentImageIndex++;

        } else if (!img.data){
//            std::cerr << "Image Reader: Image could not be read, skipping: " << fileNames[0][currentImageIndex] ;
            currentImageIndex++;

        } if (playbackLoop && currentImageIndex == fileNames[0].size()) {
//            qDebug() << "ImageReader: end reached, resetting playback, endless looping " ;
            currentImageIndex = 0;
        }
        //qDebug() << "Looping";
    }
//    qDebug() << "Loop ended";

    // Playback loop finished, either due to end of files, or pause/stop action
    if(state != PlaybackState::PAUSED) {
        state = PlaybackState::STOPPED;
        // to signal when we automatically reached the end
        if(currentImageIndex == fileNames[0].size()){
            emit endReached();
            lastCommissionedFrameNumber = -1; // corner case
        }
        currentImageIndex = 0;
//        qDebug() << "finished()";
        emit finished();
    }
    else {
//        qDebug() << "paused()";
        emit paused();
    }
    imageProcessed->wakeAll();
    imagePublished->wakeAll();
}

void ImageReader::runImpl(std::chrono::steady_clock::time_point& startTime, std::chrono::duration<int, std::milli> elapsedDuration,
                          cv::Mat& img){
    startTime += elapsedDuration;
    //std::chrono::steady_clock::time_point beginRead = std::chrono::steady_clock::now();

    //std::cout<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - beginRead).count();

    startTimestamp += playbackDelay;

    //std::cout<<"Reading image: "<< filename ;

    CameraImage cimg;
    cimg.type = CameraImageType::SINGLE_IMAGE_FILE;
    cimg.img = img.clone();
    //cimg.timestamp = startTimestamp;
    cimg.timestamp = acqTimestamps[currentImageIndex]; // using the file name, not the time of image reading operation
    cimg.frameNumber = currentImageIndex; // playbackControlDialog needs it
    cimg.filename = fileNames[0][currentImageIndex].toStdString();
    img.release();

    /*
    if (!noDelay) {
        int durProcess = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - beginProcess).count();
        if (playbackDelay - durProcess > 0) {
            QThread::msleep(playbackDelay - durProcess);
        }
    }
    */
//    qDebug() << "lastCommissionedFrameNumber: " << lastCommissionedFrameNumber;
    lastCommissionedFrameNumber = currentImageIndex;

    emit onNewImage(cimg);
}

// Executes the play back of the stereo images through reading them in order, creating a CameraImage object and sending the onNewImage signal
// The play back can be stopped by changing the PlaybackState variable state
// When the playback is finished, a finished signal is send (also send when stopping the play back early)
// Stereo images are read using QTs concurrent run to (potentially) reduce file read time
void ImageReader::runStereo() {

    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    // We assume both lists fileNames[0] and fileNames[1] are same length
    while(currentImageIndex < fileNames[0].size()) {
        if (state != PlaybackState::PLAYING) {
            qDebug() << "Image Reader: Run Loop found Stop/Pause signal" ;
            break;
        }

        //qDebug() << "imageReader locking";
        std::chrono::steady_clock::time_point beginProcess = std::chrono::steady_clock::now();
        std::chrono::duration<int, std::milli> elapsedDuration = std::chrono::duration_cast<std::chrono::milliseconds>(beginProcess - startTime);
        int elapsedTime = elapsedDuration.count();

        cv::Mat img, imgSecondary;
        if( quickReadImageStereo(img, imgSecondary, currentImageIndex) &&
            img.data && imgSecondary.data && elapsedTime >= playbackDelay ) {

            if (synchronised) {
                const QMutexLocker locker(imageMutex);
                qDebug() << "imageReader locking";
                qDebug() << "Current image index: " << currentImageIndex;
                runStereoImpl(startTime, elapsedDuration, img, imgSecondary);
                imageProcessed->wakeAll();
                if (state == PlaybackState::PLAYING) {
                    imagePublished->wait(imageMutex);
                }
            } else {
                qDebug() << "Current image index: " << currentImageIndex;
                runStereoImpl(startTime, elapsedDuration, img, imgSecondary);
            }
            currentImageIndex++;

        } else if (!img.data || !imgSecondary.data){
            qDebug() << "Image Reader: Image could not be read, skipping: " << fileNames[0][currentImageIndex] ;
            currentImageIndex++;

        } if (playbackLoop && currentImageIndex == fileNames[0].size()) {
            qDebug() << "ImageReader: end reached, resetting playback, endless looping " ;
            currentImageIndex = 0;
        }
    }
    qDebug() << "Loop ended";

    // Playback loop finished, either due to end of files, or pause/stop action
    if(state != PlaybackState::PAUSED) {
        state = PlaybackState::STOPPED;
        // to signal when we automatically reached the end
        if(currentImageIndex == fileNames[0].size()){
            emit endReached();
            qDebug() << "endReached";
            lastCommissionedFrameNumber = -1; // corner case
        }
        currentImageIndex = 0;
        qDebug() << "finished()";
        emit finished();
    }

    imageProcessed->wakeAll();
    imagePublished->wakeAll();

}

void
ImageReader::runStereoImpl(std::chrono::steady_clock::time_point &startTime, std::chrono::duration<int, std::milli> elapsedDuration,
                           cv::Mat &img, cv::Mat &imgSecondary) {

    startTime += elapsedDuration;
    //std::chrono::steady_clock::time_point beginRead = std::chrono::steady_clock::now();

    //std::cout<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - beginRead).count();

    startTimestamp += playbackDelay;

    //std::cout<<"Reading image: "<< filename ;

    CameraImage cimg;
    cimg.type = CameraImageType::STEREO_IMAGE_FILE;
    cimg.img = img.clone();
    cimg.imgSecondary = imgSecondary.clone();
    //cimg.timestamp = startTimestamp;
    cimg.timestamp = acqTimestamps[currentImageIndex]; // using the file name, not the time of image reading operation
    cimg.frameNumber = currentImageIndex;
    cimg.filename = fileNames[0][currentImageIndex].toStdString();
    img.release();
    imgSecondary.release();

    /*
    if (!noDelay) {
        int durProcess = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - beginProcess).count();
        if (playbackDelay - durProcess > 0) {
            QThread::msleep(playbackDelay - durProcess);
        }
    }
    */
    lastCommissionedFrameNumber = currentImageIndex;

    emit onNewImage(cimg);
}

// Starts the image reader play back in another thread
void ImageReader::start() {

    if(state == PlaybackState::PLAYING)
        return;

//    qDebug()<<"Image Reader: Starting ImageReader thread.";

    state = PlaybackState::PLAYING;
    startTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

    if(stereoMode) {
        playbackProcess = QtConcurrent::run([this]{ return ImageReader::runStereo(); }); // Qt6 compatible
        // playbackProcess = QtConcurrent::run(this, &ImageReader::runStereo);
    } else {
        playbackProcess = QtConcurrent::run([this]{ return ImageReader::run(); }); // Qt6 compatible
        // playbackProcess = QtConcurrent::run(this, &ImageReader::run);
    }
}

// Pause the image play back at the current image, calling start again will proceed at the paused image
void ImageReader::pause() {

    if(state != PlaybackState::PLAYING)
        return;

    qDebug()<<"Image Reader: Pausing ImageReader thread.";
    state = PlaybackState::PAUSED;

    imageProcessed->wakeAll();
    imagePublished->wakeAll();
    playbackProcess.waitForFinished();

}

// Stops the image play back and sets the play position back to start, calling start again will play the first image again
void ImageReader::stop() {
    if(state == PlaybackState::STOPPED)
        return;

    qDebug()<<"Image Reader: Stopping ImageReader thread.";
    state = PlaybackState::STOPPED;
    //currentImageIndex = 0;

    imageProcessed->wakeAll();
    imagePublished->wakeAll();
    playbackProcess.waitForFinished();
}

QString ImageReader::findMostFrequentExtension(const QStringList &fileNameCandidates) {

    // We find the most frequent extension in the folder (which must be the image extension we use)
    // NOTE: Technically speaking, the dot is not part of the extension, so we do not supply that on the output
    std::vector<cv::String> fileExtensions;
    std::vector<int> fileExtensionFreqencies;
    int dotPos = cv::String::npos;
    cv::String currExt;
    //
    for(int c=0; c<fileNameCandidates.size(); c++) {
        dotPos = fileNameCandidates[c].toStdString().find_last_of('.'); // TODO: remove extra conversion and test if works
        if(dotPos != cv::String::npos) {
            currExt = fileNameCandidates[c].toStdString().substr(dotPos+1, fileNameCandidates[c].length()-(dotPos+1));

            auto whereInVector = std::find(fileExtensions.begin(), fileExtensions.end(), currExt); // TODO: remove extra conversion and test if works
            if(whereInVector == fileExtensions.end()) { // if the extension can NOT be found in the vector, add it
                fileExtensions.push_back(currExt);
                fileExtensionFreqencies.push_back(1);
                //std::cout << "Found files in the folder with the following extension = " << currExt ;
            } else {
                fileExtensionFreqencies[(int)(whereInVector-fileExtensions.begin())]++;
            }
        } else {
            // töröljük simán a listából
        }
    }
    int mostFreqIndex = std::max_element(fileExtensionFreqencies.begin(), fileExtensionFreqencies.end())-fileExtensionFreqencies.begin();
    //std::cout << "Freq of the most frequent extension = " << fileExtensionFreqencies[mostFreqIndex] ;
    //std::cout << "The most frequent extension = " << fileExtensions[mostFreqIndex] ;

    return QString::fromStdString(fileExtensions[mostFreqIndex]);
}

// TODO: reorganize to make this function only remove the other items. The most freq. extension search should be in different function
QStringList ImageReader::purgeFileNamesVector(QStringList fileNameCandidates) {

    if(fileNameCandidates.empty())
        return fileNameCandidates;

    // TODO: rewrite to eliminate need for conversion
    std::string passableSuffix = ("." + findMostFrequentExtension(fileNameCandidates)).toStdString();

    int dotPos = cv::String::npos;
    cv::String currExt;
    size_t iterUntil = fileNameCandidates.size()-1;
    size_t iterIndex = 0;
    bool flaggedForDeletion = false;
    while(iterIndex <= iterUntil) {
        //fileNames[iter_index].find(s2) != std::string::npos
        flaggedForDeletion = false;

        dotPos = fileNameCandidates[iterIndex].toStdString().find_last_of('.'); // TODO: remove extra conversion and test if works
        if(dotPos != cv::String::npos) {
            // TODO: remove extra conversion and test if works
            currExt = fileNameCandidates[iterIndex].toStdString().substr(dotPos, fileNameCandidates[iterIndex].length()-(dotPos)); //also handles if the filename ends with dot but there are no characters afterwards

            if(currExt != passableSuffix) {
                flaggedForDeletion = true;
            }
        } else {
            flaggedForDeletion = true;
        }

        if(flaggedForDeletion) {
            qDebug() << "Deleting unusual filename from detected fileNames vector = " << fileNameCandidates[iterIndex] ;
            fileNameCandidates.erase(fileNameCandidates.begin()+iterIndex); //no index increment, because element was deleted, but iter_until needs to decrease
            iterUntil--;
        } else {
            iterIndex++;
        }
    }
    return fileNameCandidates;
}

cv::Mat ImageReader::getStillImageSingle(int frameNumber) {
    cv::Mat img;
    if(fileNames[0].size() > frameNumber) {
        //return cv::imread(fileNames[0][frameNumber].toStdString(), cv::IMREAD_GRAYSCALE);
        quickReadImageSingle(img, frameNumber);
    }
    return img;
}

std::vector<cv::Mat> ImageReader::getStillImageStereo(int frameNumber) {
    cv::Mat img, imgSecondary;
    if(fileNames[0].size() >= frameNumber && fileNames[1].size() > frameNumber) {
        quickReadImageStereo(img, imgSecondary, frameNumber);
    }
    return {img, imgSecondary};
}

void ImageReader::setSynchronised(bool synchronised) {
    ImageReader::synchronised = synchronised;
}



