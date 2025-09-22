#pragma once

/**
    @author Moritz Lode, Gabor Benyei, Attila Boncser
*/

#include <QtCore/QObject>
#include <QtGui/QtGui>
#include <QtXml>
#include "../devices/camera.h"
#include <vector>
#include <algorithm>
#include "quazip/quazip.h"
#include "quazip/quazipfile.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
//#include <libavcodec/version.h>
}

enum PlaybackState { STOPPED=0, PAUSED=1, PLAYING=2 };

/**
    Class to read images from disk

    Supports both single and stereo image recordings, in the format created by this software in recording

    For single camera images, all images are in a single directory, without any other files
    For stereo camera images, two directories exist, 0 for main camera images, 1 for secondary camera images, each image have corresponding filenames

    CAUTION:
    OpenCV's cv::glob is used to list the content of the directory, which returns the files in alphabetically order, a preceeding, zeros are necessary for the correct order
    i.e. 10.jpg and 9.jpg are ordered wrong due to that (1<9), 09.jpg and 10.jpg do not have the problem (0<1)

    CAUTION:
    Depending on the disk read speed, high replay framerates may not be possible due to disk read speed and filesize
*/
class ImageReader : public QObject {
Q_OBJECT


public:

    explicit ImageReader(QString imageSource, int subrecordingNumber, QMutex *imageMutex, QWaitCondition *imagePublished, QWaitCondition *imageProcessed, int playbackSpeed = 30, bool playbackLoop=false, QObject *parent = 0);

    ~ImageReader() override;

    enum ImageReaderStatus {
        IMSTATUS_UNDETERMINED,
        IMSTATUS_OK,
        IMSTATUS_ERROR,
        IMSTATUS_ZIP_INDECISIVE,
        IMSTATUS_ZIP_UNOPENABLE,
        IMSTATUS_VIDEO_UNOPENABLE
    };

    enum ImageReaderSource {
        IMSOURCE_UNDETERMINED,
        IMSOURCE_DIRECTORY,
        IMSOURCE_ZIP,
        IMSOURCE_VIDEO
    };

    struct ZipMultiInfo {
        QString recordingName;
        quint64 recordingLength;
    };

    ImageReaderStatus getImageReaderStatus() {
        return imageReaderStatus;
    }
    ImageReaderSource getImageReaderSource() {
        return imageReaderSource;
    };

    bool isPlaying() {
        return PlaybackState::PLAYING == state;
    }

    bool isPaused() {
        return PlaybackState::PAUSED == state;
    }

    bool isStopped() {
        return PlaybackState::STOPPED == state;
    }

    bool isStereo(){
        return stereoMode;
    }

    int getPlaybackSpeed() {
        return playbackSpeed;
    }

    void setPlaybackSpeed(int fps);

    int getPlaybackLoop() {
        return playbackLoop;
    }

    void setPlaybackLoop(bool loop) {
        playbackLoop = loop;
    }

    cv::Mat getStillImageSingle(int frameNumber);
    std::vector<cv::Mat> getStillImageStereo(int frameNumber);

//    QString getImageDirectoryName() {
//        return imageSourceDir.absolutePath();
//    }
    int getImageWidth() {
        return foundImageWidth;
    }
    int getImageHeight() {
        return foundImageHeight;
    }
    
    int getFrameNumberForTimestamp(uint64_t &timestamp) {
        // Global private var and indexing back from currentImageIndex, this is the fastest I guess.
        // However, there is a conversion from uint64_t to quint64 every time the comparison happens..
        // I could not get around this, as CameraImage employs uint64_t 
        // (I guess because in case of real cameras, it gets the value from Pylon, which uses uint64_t)
        for(imgNumSeekerIdx = acqTimestamps.size() - 1; imgNumSeekerIdx>=0; imgNumSeekerIdx--) {
            if(acqTimestamps[imgNumSeekerIdx] == timestamp) 
                return imgNumSeekerIdx;
        }
        return currentImageIndex; 
    }
    
    uint64_t getTimestampForFrameNumber(int frameNumber) {
        if(acqTimestamps.size() > frameNumber)
            return acqTimestamps[frameNumber];
        else
            return 0;
    }

    /*uint64_t getLastCommissionedTimestamp() {
        if(lastCommissionedFrameNumber>0)
            return acqTimestamps[lastCommissionedFrameNumber];
        else 
            return 0;
    }*/
    int getLastCommissionedFrameNumber() {
        if(lastCommissionedFrameNumber>0)
            return lastCommissionedFrameNumber;
        else 
            return 0;
    }

    int getNumImagesTotal() {
        return (int)acqTimestamps.size();
    };

    uint64_t getRecordingDuration() {
        return acqTimestamps[acqTimestamps.size()-1] - acqTimestamps[0];
    }
    void seekInVideoStream(int frameNumber, bool seekBackwards) {

        // NOTE: under current circumstances, it seems that this is ALWAYS the case, as we use no keyframes.
        // Although for any non-scientific grade recording we might want to be able to handle for educative
        // reasons, where there are keyframes, this might be useful still.
   //     int avSeekFlag = AVSEEK_FLAG_BACKWARD;
   ////     int avSeekFlag = 0;
   ////     if(seekBackwards)
   ////         avSeekFlag &= AVSEEK_FLAG_BACKWARD;

        // NOTE: We have to seek by timestamp, not frame. Safer anyhow.
        //  Since the timebase is deliberately the finest we can get, and FPS is not sure,
        //  we need the lookup table of frame numbers for that.

        // IMPORTANT: we need to subtract the first timestamp always, as they were rebased upon
        //  encoding due to possible codec requirements, to always start at 0 at the first frame.

        // TODO: STEREO?
        // TODO: separate function?

        auto ueueue = getTimestampForFrameNumber(frameNumber) - acqTimestamps[0];

        int ret = 0;

        ret += av_seek_frame(fmt_ctx, videoStreamIndices[0],
                                getTimestampForFrameNumber(frameNumber) - acqTimestamps[0],
                                AVSEEK_FLAG_BACKWARD);
        if(stereoMode) {
            ret += av_seek_frame(fmt_ctx, videoStreamIndices[1],
                                 getTimestampForFrameNumber(frameNumber) - acqTimestamps[0],
                                 AVSEEK_FLAG_BACKWARD);
        }
        //int ret = av_seek_frame(fmt_ctx, videoStreamIndices[0],
        //                        getTimestampForFrameNumber(frameNumber) - acqTimestamps[0],
        //                        0);
        if (ret < 0) {
            qDebug() << "Seek failed:" << ret;
        }
        avcodec_flush_buffers(vctx);
    }
    void seekToFrame(int frameNumber, bool seekBackwards) {

        if(imageReaderSource == IMSOURCE_VIDEO) {
            seekInVideoStream(frameNumber, seekBackwards);
        }

        if(frameNumber<0)
            frameNumber=0;
        currentImageIndex = frameNumber;
    }
    QVector<ZipMultiInfo> getFoundZipMultiInfo() {
        return foundZipMultiInfo;
    }
    QString getOfflineEventLogContent() {
        return offlineEventLogContent;
    }
    QString getMetaSnapshotContent() {
        return metaSnapshotContent;
    }
    uint64 getRecLenFromFileNamesList(const QStringList &fileNameCandidates, const QString &extensionString) {
        bool ok;
        QString startTsStr = fileNameCandidates[0];
        QString endTsStr = fileNameCandidates[fileNameCandidates.size()-1];
        startTsStr = startTsStr.mid(startTsStr.lastIndexOf("/")+1, startTsStr.length()-(startTsStr.lastIndexOf("/")+2+extensionString.length()));
        endTsStr = endTsStr.mid(endTsStr.lastIndexOf("/")+1, endTsStr.length()-(endTsStr.lastIndexOf("/")+2+extensionString.length()));
        qDebug() << (endTsStr.toULongLong(&ok, 10) - startTsStr.toULongLong(&ok, 10));
        return (endTsStr.toULongLong(&ok, 10) - startTsStr.toULongLong(&ok, 10));
    }

    void setSynchronised(bool synchronised);

private:

    QFuture<void> playbackProcess;
    QMutex mutex;
    QMutex *imageMutex;
    QWaitCondition *imagePublished;
    QWaitCondition *imageProcessed;

    ImageReaderStatus imageReaderStatus = IMSTATUS_UNDETERMINED;
    ImageReaderSource imageReaderSource = IMSOURCE_UNDETERMINED;

    QuaZip* imageSourceZip = nullptr;
    QuaZipFile* imageSourceZipInnerFile = nullptr;
    QuaZipFileInfo info;

    QVector<QStringList> fileNames {QStringList(), QStringList()};

    const QString zipSuffix = "zip";
    const QString videoSuffix = "mkv";
    int currentFrameInfoFileVersion = 1;


    uint64 startTimestamp;
    int playbackSpeed;
    int playbackDelay;

    int currentImageIndex;

    PlaybackState state;

    bool stereoMode;
    bool noDelay;
    bool playbackLoop;
    bool synchronised;

    std::vector<quint64> acqTimestamps;
    int imgNumSeekerIdx = 0;
    int lastCommissionedFrameNumber = -1; 

    int foundImageWidth = 0;
    int foundImageHeight = 0;


    QVector<ZipMultiInfo> foundZipMultiInfo;
    QVector<QString> zS_recordingNames;
    QVector<int> zS_recordingLengths;

    QString offlineEventLogContent;
    QString metaSnapshotContent;
    QString frameInfoFileContent;

    std::vector<int> videoStreamIndices = {};
    AVFormatContext* fmt_ctx = nullptr;
    AVPixelFormat foundPixelFormat = AVPixelFormat::AV_PIX_FMT_NONE;
    const AVCodec* vcodec = nullptr;
    AVCodecContext* vctx = nullptr;
    AVPacket *pkt = nullptr;
    AVFrame *frame = nullptr;
    AVFrame *grayFrame = nullptr;
    SwsContext *swsCtx = nullptr; // yet we have one, and use only one for one recording opened

    void createSwsCtxAndPrepareGrayFrame() {
        if (foundImageWidth <= 0)
            foundImageWidth = frame->width;
        if (foundImageHeight <= 0)
            foundImageHeight = frame->height;
        if(foundPixelFormat == AVPixelFormat::AV_PIX_FMT_NONE)
            foundPixelFormat = (AVPixelFormat) frame->format;

        swsCtx = sws_getContext(
                foundImageWidth, foundImageHeight, foundPixelFormat,
                foundImageWidth, foundImageHeight, AV_PIX_FMT_GRAY8,
                SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        grayFrame->format = AV_PIX_FMT_GRAY8;
        grayFrame->width  = frame->width;
        grayFrame->height = frame->height;
        av_frame_get_buffer(grayFrame, 32);
    };

    void exploreZip(const QString &imageSource, const int &subrecordingNumber);
    QString findMostFrequentExtension(const QStringList &fileNameCandidates);
    QStringList purgeFileNamesVector(QStringList fileNameCandidates);
    std::vector<quint64> extractAcqTimestamps(const QStringList &fileNameCandidates);
    std::vector<quint64> extractAcqTimestampsFromFrameInfoFile(const QString &content);

    bool quickReadImageSingle(cv::Mat &img, const int &imageIndex);
    bool quickReadImageStereo(cv::Mat &img, cv::Mat &imgSecondary, const int &imageIndex);

    void run();
    void runImpl(std::chrono::steady_clock::time_point& startTime, std::chrono::duration<int, std::milli> elapsedDuration, cv::Mat &img);
    void runStereo();
    void runStereoImpl(std::chrono::steady_clock::time_point& startTime, std::chrono::duration<int, std::milli> elapsedDuration, cv::Mat &img, cv::Mat &imgSecondary);

public slots:

    void start();
    void stop();
    void pause();

signals:

    void onNewImage(CameraImage image);
    void finished();

    void paused();

    // NOTE: we need this specific signal, to let imagePlaybackControlDialog know
    // that the playback finished automatically. The dialog alonw only knows about 
    // playback changes that were caused by GUI interactions. Without this signal, it would be clueless
    void endReached();

};
