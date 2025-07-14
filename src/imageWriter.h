#pragma once

/**
    @author Moritz Lode, Gabor Benyei
*/

#include <QCoreApplication>
#include <QtCore/qdir.h>
#include "devices/camera.h"
#include "quazip/quazip.h"
#include "quazip/quazipfile.h"

/**
    Class to write camera images to disk

    Supports recording of single and stereo images

    onNewImage(): received new image and writes it to disk, file writing is performed concurrently for maximal performance

    CAUTION: Chosen image format has a large performance impact due to size and disk write speeds
*/
class ImageWriter : public QObject {
Q_OBJECT

public:

    ImageWriter(QObject *parent = 0);
    ~ImageWriter() override;

    enum ImageWriterStatus {
        IWSTATUS_UNDETERMINED,
        IWSTATUS_OK,
        IWSTATUS_ERROR,
        IWSTATUS_ZIP_UNOPENABLE
    };

    enum ImageWriterTarget {
        IWTARGET_UNDETERMINED,
        IWTARGET_DIRECTORY,
        IWTARGET_ZIP
    };

    // this is yet only used by MetaSnapshotOrganizer
    QString getOpenableDirectoryName() {
        return outputDirectory;
    };

    ImageWriterStatus getImageWriterStatus() {
        return imageWriterStatus;
    }
    ImageWriterTarget getImageWriterTarget() {
        return imageWriterTarget;
    };
    QString getFoundMetaSnapshotContent() {
        return foundMetaSnapshotContent;
    };
    QString getFoundOfflineEventLogContent() {
        return foundOfflineEventLogContent;
    };

private:

    QSettings *applicationSettings;

    QString outputDirectory;
    QString outputZip;
    QString outputZipInnerRootDirectory;

    QString imageWriterFormatString;
    QString imageWriterDataRule;
    bool stereoMode = false;

    QuaZip* imageOutputTargetZip = nullptr;
    QuaZipFile* imageOutputTargetZipInnerFile = nullptr;
    QuaZipFileInfo info;

    bool foundZipAlreadyExist = false;
    QList<QuaZipFileInfo> foundFileInfoList;
    QList<QString> foundFileNameList;
    QString foundMetaSnapshotContent;
    QString foundOfflineEventLogContent;

    QString metaSnapshotFileName;
    QString offlineEventLogFileName;

    std::vector<int> writeParams = std::vector<int>();

    std::vector<uchar> imencodeBuffer;

    ImageWriterStatus imageWriterStatus = IWSTATUS_UNDETERMINED;
    ImageWriterTarget imageWriterTarget = IWTARGET_UNDETERMINED;

public slots:

    //bool isWriting();

    bool prepareForWriting(const QString &imageOutputTarget, bool stereo);
    void stopWriting();
    bool writeMetaSnapshot(const QString &content);
    bool writeOfflineEventLog(const QString &content);

    // void attemptToStop();

    void onNewImage(const CameraImage &img);

signals:

    //void attemptedStopDone();

    // TODO: It could even monitor current resources of the computer, whether there is sufficient memory to
    //  handle disk-writing growth, or just compare the currently pending waiting-to-be-received signals
    //  and say, if it is too much
    void writingFailed();

};
