#pragma once

/**
    @author Moritz Lode, Gabor Benyei
*/

#include <QCoreApplication>
#include <QtCore/qdir.h>
#include <QtXml>
#include "../devices/camera.h"
#include "quazip/quazip.h"
#include "quazip/quazipfile.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
//#include <libavcodec/version.h>
}
//#pragma message("avcodec version " AV_STRINGIFY(LIBAVCODEC_VERSION_MAJOR))

/**
    Class to write camera images to disk

    Supports recording of single and stereo images

    onNewImage(): received new image and writes it to disk, file writing is performed concurrently for maximal performance

    CAUTION: Chosen image format has a large performance impact due to size and disk write speeds
*/
class ImageWriter : public QObject {
Q_OBJECT

public:

    struct FrameInfo {
        quint64 timestamp = 0;
        //QChar cameraIdentity = 'X';
    };

    ImageWriter(QObject *parent = 0);
    ~ImageWriter() override;

    enum ImageWriterStatus {
        IWSTATUS_UNDETERMINED,
        IWSTATUS_OK,
        IWSTATUS_ERROR,
        IWSTATUS_ZIP_UNOPENABLE,
        IWSTATUS_VIDEO_START_FAILURE
    };

    enum ImageWriterTarget {
        IWTARGET_UNDETERMINED,
        IWTARGET_DIRECTORY,
        IWTARGET_ZIP,
        IWTARGET_VIDEO
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

    int currentFrameInfoFileVersion = 1;

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

    //uint64_t videoStartTimestamp = 0;
    std::vector<FrameInfo> frameInfos;

    ImageWriterStatus imageWriterStatus = IWSTATUS_UNDETERMINED;
    ImageWriterTarget imageWriterTarget = IWTARGET_UNDETERMINED;

    QString outputVideoFilename;
    AVFormatContext* fmt_ctx = nullptr;
    AVStream* video_st_main = nullptr;
    AVStream* video_st_sec = nullptr;
    //AVStream* data_st = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    const AVCodec* codec = nullptr;
    AVFrame* frame;
    SwsContext* swsCtx = nullptr;
    AVPacket* pkt = nullptr;
    QString videoSidecarContent;

    void writeVideoFrame(const cv::Mat &img, const uint64 &timestamp, QChar cameraIdentity);
    void embedFileWithinVideoBeforeHeader(const QString &content, const QString &fileName);
    void addSubtitleAtTimestamp(const QString &content, const uint64 &timestamp);
    QString generateFrameInfoFileContent();

public slots:

    //bool isWriting();

    bool prepareForWriting(const QString &imageOutputTarget, bool stereo, QSize expectedFrameSize = {0,0}, int expectedFrameRate = 1);
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
