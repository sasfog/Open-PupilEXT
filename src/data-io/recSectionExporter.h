#pragma once

/**
    @authors Gabor Benyei
*/

extern "C" {
#include "libavutil/opt.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <QtCore/QObject>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include "../pupil-detection-methods/Pupil.h"
#include "../pupilDetection.h"
#include "../recEventTracker.h"

#include <QSettings>
#include <QCoreApplication>

class RecSectionExporter : public QObject {
    Q_OBJECT

public:

    explicit RecSectionExporter(QObject *parent = 0);
    ~RecSectionExporter() override;
    void close();

    bool prepareExport(int ws, int hs, int procMode, RecEventTracker *_recEventTracker);

private:

    RecEventTracker *recEventTracker;

    QElapsedTimer drawTimer;
    bool freshStart = true;
    int desiredPlaybackFPS = 30;

    const int EXPORT_DEF_WIDTH = 200;
    const int EXPORT_DEF_HEIGHT = 150;

    QSettings *applicationSettings;

    AVFormatContext *formatContext = nullptr;
    AVCodecContext *codecContext = nullptr;
    //
    AVStream *stream = nullptr;
    AVFrame *avframe = nullptr;
    AVFrame *avframeTmp = nullptr;
    AVFrame *avframeYuv = nullptr;
    SwsContext *swsCtx = nullptr;
    SwsContext *swsGIFCtx = nullptr;
    const AVCodec *codec = nullptr;
    int64_t pts = 0;




    bool uniformPortableImageSize = false;
    bool embedFrameInfo = false;
    bool cropToPDROI = false;
    ProcMode currentProcMode = UNDETERMINED;

    int toFrameNumber = 1;

    float width = 0;
    float height = 0;

    QString header;

    // based on https://stackoverflow.com/a/62122854/11414500 by SO user shabany
    // Last accessed: 2025.11.02. 10:35 CET
    int getGoodFontScale(QString text, float fitInHeight) {
        for(int i=60; i>0; i--) {
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(text.toStdString(), cv::FONT_HERSHEY_PLAIN, (float)i/10.0f, 1, &baseline);
            if(textSize.height < fitInHeight)
                return (float)i/10.0f;
        }
        return 1;
    }

    cv::Mat resizeARAware(const cv::Mat &image);
    void addFrame(cv::Mat image, int64_t at_pts);
    void processImage(CameraImage mimg, std::vector<cv::Rect> ROIs, std::vector<Pupil> Pupils);
    void cleanup();

public slots:

    void onExportAllowedToStart();
    void onExportAllowedToEnd();

    // This also burns in extra data into the frames, if needed
    //void onNewImage(quint64 timestamp, int procMode, const std::vector<cv::Mat> &images, const std::vector<Pupil> &Pupils);
    void onNewImage(CameraImage mimg);
    void onNewImage(CameraImage mimg, int procMode, std::vector<cv::Rect> ROIs, std::vector<Pupil> Pupils);
};
