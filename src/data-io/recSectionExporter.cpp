#include <iostream>
#include <QtCore/qfileinfo.h>
#include <QMessageBox>
#include "recSectionExporter.h"
#include "../supportFunctions.h"

RecSectionExporter::RecSectionExporter(QObject *parent) :
    QObject(parent),
    //writerReady(false),
    applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), parent)) {

    qDebug() << "New RecSectionExporter object created.";

    //qDebug() << fileName;

    /*
    bool pathWriteable = SupportFunctions::preparePath(fileName);
    if(!pathWriteable) {
        QMessageBox MsgBox;
        MsgBox.setText("Recording failure. Could not create path.");
        MsgBox.exec();
    }
    */

    /*
    dataFile = new QFile(fileName);
    bool exists = dataFile->exists(); // NOTE: should never happen

    // Open the file in append mode
    // GB: needed to double check, in case of writing to e.g. C:/ or other admin-only folder, 
    // if the exe was started without admin rights, open would fail, and the null-ed dataFile can cause exception later on
    bool fileWriteable = dataFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    if (!fileWriteable) {
        //std::cout << "Recording failure. Could not open: " << fileName.toStdString() << std::endl;
        delete dataFile;
        dataFile = nullptr;
        // QMessageBox MsgBox;
        // MsgBox.setText(QString::fromStdString("Recording failure. Could not open: " + fileName.toStdString()));
        // MsgBox.exec();
        return;
    }

    textStream = new QTextStream(dataFile);

    if(dataFile && textStream)
        writerReady=true;

    // To not write again a header line to the file when it already existed (appending), check it
    if(!exists)
        *textStream << header << Qt::endl;

    //if(!pathWriteable || !fileWriteable)
    if(!fileWriteable)
        this->deleteLater();
        */
}

RecSectionExporter::~RecSectionExporter() {
    close();
}

// Close the file and filestreams
void RecSectionExporter::close() {
    /*
    if (dataFile){
        dataFile->close();
        dataFile->deleteLater();
    }
    delete textStream;
    delete dataFile;
    dataFile = nullptr;
    textStream = nullptr;
     */

    qInfo() << "RecSectionExporter object deleted.";
}

// partly based on https://stackoverflow.com/a/38997739/11414500 by SO user Sierra
// Last accessed: 2025.11.02. 10:30 CET
bool RecSectionExporter::prepareExport(int ws, int hs, int procMode, RecEventTracker *_recEventTracker) {

    width = ws;
    height = hs;
    currentProcMode = (ProcMode)procMode;
    recEventTracker = _recEventTracker;

    // TODO: not hardcoded. Also, "reduce" is not the proper word here, as the image will be upsized if it is too small
    uniformPortableImageSize = SupportFunctions::readBoolFromQSettings("ExportRecSection.UniformPortableImageSize", true, applicationSettings);
    if(uniformPortableImageSize) {
        width = EXPORT_DEF_WIDTH;
        height = EXPORT_DEF_HEIGHT;
    }

    switch(currentProcMode) {
        case ProcMode::STEREO_IMAGE_ONE_PUPIL:
        case ProcMode::STEREO_IMAGE_TWO_PUPIL:
            width *= 2;
            //height *= 2;
            break;
    }

    // TODO: make width and height for extra markings, etc
    embedFrameInfo = SupportFunctions::readBoolFromQSettings("ExportRecSection.EmbedFrameInfo", true, applicationSettings);
    if(embedFrameInfo) {
        height *= 1.2;
    }

    //includeShownOverlays = SupportFunctions::readBoolFromQSettings("ExportRecSection.IncludeShownOverlays", true, applicationSettings);
    cropToPDROI = SupportFunctions::readBoolFromQSettings("ExportRecSection.CropToPDROI", false, applicationSettings);
    toFrameNumber = applicationSettings->value("ExportRecSection.ToFrameNumber", 2).toInt() - 1;
    desiredPlaybackFPS = applicationSettings->value("ExportRecSection.DesiredPlaybackFPS", 30).toInt();
    if(desiredPlaybackFPS > 30)
        desiredPlaybackFPS = 30;

    QString recentDestFile = applicationSettings->value("ExportRecSection.DestName", "").toString();
    QString destDirectory = applicationSettings->value("ExportRecSection.DestDirectory", "").toString();
    if(recentDestFile.isEmpty() || destDirectory.isEmpty()) {
        return false;
    }
    QString filename = destDirectory + "/" + recentDestFile;

    avformat_alloc_output_context2(&formatContext, nullptr, "gif", filename.toStdString().c_str());
    if (!formatContext) {
        std::cerr << "RecSectionExporter: Could not allocate output context.\n";
        return false;
    }

    codec = avcodec_find_encoder(formatContext->oformat->video_codec);
    if (!codec) {
        std::cerr << "RecSectionExporter: GIF encoder not found.\n";
        avformat_free_context(formatContext);
        return false;
    }

    stream = avformat_new_stream(formatContext, nullptr);
    if (!stream) {
        std::cerr << "RecSectionExporter: Could not create stream.\n";
        avformat_free_context(formatContext);
        formatContext = nullptr;
        return false;
    }
    //stream->time_base = AVRational{1, 30};

    codecContext = avcodec_alloc_context3(codec);
    codecContext->codec_id = formatContext->oformat->video_codec;
    codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    codecContext->width = (int)width;
    codecContext->height = (int)height;
    codecContext->pix_fmt = AV_PIX_FMT_RGB8;
    codecContext->bit_rate = 2000000; // TODO: sure?
    codecContext->time_base = AVRational{1, static_cast<int>(1000)}; // only to specify presentation timestamps
    //codecContext->framerate = AVRational{static_cast<int>(30), 1}; // takes effect at later playback
    codecContext->framerate = AVRational{static_cast<int>(desiredPlaybackFPS), 1}; // takes effect at later playback

    if (codecContext->codec_id == AV_CODEC_ID_H264) {
        av_opt_set(codecContext->priv_data, "preset", "slow", 0);
    }
    if (formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open GIF encoder.\n";
        avcodec_free_context(&codecContext);
        avformat_free_context(formatContext);
        return false;
    }

    avcodec_parameters_from_context(stream->codecpar, codecContext);
    stream->time_base = codecContext->time_base;

    // according to some examples, this could simply be: av_dump_format(formatContext, 0, filePath.data(), 1);
    if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&formatContext->pb, filename.toStdString().c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "RecSectionExporter: Could not open output file.\n";
            avformat_free_context(formatContext);
            formatContext = nullptr;
            return false;
        }
    }

    if (avformat_write_header(formatContext, nullptr) < 0) {
        std::cerr << "RecSectionExporter: Error writing header.\n";
        avformat_free_context(formatContext);
        formatContext = nullptr;
        return false;
    }


    // Here we need 3 frames. Basically, the QImage is firstly extracted in AV_PIX_FMT_BGRA.
    // We need then to convert it to AV_PIX_FMT_RGB8 which is required by the .gif format.
    // If we do that directly, there will be some artefacts and bad effects... to prevent that
    // we convert FIRST AV_PIX_FMT_BGRA into AV_PIX_FMT_YUV420P THEN into AV_PIX_FMT_RGB8.

    avframe = av_frame_alloc();
    avframe->format = codecContext->pix_fmt; // AV_PIX_FMT_RGB8
    avframe->width = codecContext->width;
    avframe->height = codecContext->height;
    av_frame_get_buffer(avframe, 0);
    //
    avframeTmp = av_frame_alloc();
    avframeTmp->format = AV_PIX_FMT_BGRA;
    avframeTmp->width = codecContext->width;
    avframeTmp->height = codecContext->height;
    av_frame_get_buffer(avframeTmp, 0);
    //
    avframeYuv = av_frame_alloc();
    avframeYuv->format = AV_PIX_FMT_YUV420P;
    avframeYuv->width = codecContext->width;
    avframeYuv->height = codecContext->height;
    av_frame_get_buffer(avframeYuv, 0);

    freshStart = true;

    return true;
}

void RecSectionExporter::addFrame(cv::Mat image, int64_t at_pts) {

    // When we pass a frame to the encoder, it may keep a reference to it internally;
    // make sure we do not overwrite it here!
    av_frame_make_writable(avframeTmp);

    avframeTmp->data[0] = { image.data };
    //int src_stride[1] = { image.cols * 3 }; // could be just image.step inside

    // Make sure to clear the frame. It prevents a bug that displays only the
    // first captured frame on the GIF export.
    if (avframe) {
        av_frame_free(&avframe);
        avframe = nullptr;
    }
    //
    avframe = av_frame_alloc();
    avframe->format = codecContext->pix_fmt; // AV_PIX_FMT_RGB8
    avframe->width = codecContext->width;
    avframe->height = codecContext->height;
    av_frame_get_buffer(avframe, 0);
    //

    if (avframeYuv) {
        av_frame_free(&avframeYuv);
        avframeYuv = nullptr;
    }
    //
    avframeYuv = av_frame_alloc();
    avframeYuv->format = AV_PIX_FMT_YUV420P;
    avframeYuv->width = codecContext->width;
    avframeYuv->height = codecContext->height;
    av_frame_get_buffer(avframeYuv, 0);
    //

    // Converting BGRA -> YUV420P...
    if (!swsCtx) {
        swsCtx = sws_getContext(width, height,
                                AV_PIX_FMT_BGRA,
                                width, height,
                                AV_PIX_FMT_YUV420P,
                                SWS_BILINEAR, NULL, NULL, NULL);
    }

    // ...then converting YUV420P -> RGB8 (natif GIF format pixel)
    if (!swsGIFCtx) {
        swsGIFCtx = sws_getContext(width, height,
                                   AV_PIX_FMT_YUV420P,
                                   codecContext->width, codecContext->height,
                                   codecContext->pix_fmt,
                                   SWS_BILINEAR, NULL, NULL, NULL);
    }

    // This double scaling prevent some artifacts on the GIF and improve
    // significantly the display quality
    sws_scale(swsCtx,
              (const uint8_t * const *)avframeTmp->data,
              avframeTmp->linesize,
              0,
              codecContext->height,
              avframeYuv->data,
              avframeYuv->linesize);
    sws_scale(swsGIFCtx,
              (const uint8_t * const *)avframeYuv->data,
              avframeYuv->linesize,
              0,
              codecContext->height,
              avframe->data,
              avframe->linesize);

    avframe->pts = at_pts;

    if (avcodec_send_frame(codecContext, avframe) < 0) {
        std::cerr << "Error sending frame.\n";
        //break;
    }

    AVPacket pkt = {0};
    av_init_packet(&pkt);

    while (avcodec_receive_packet(codecContext, &pkt) == 0) {
        pkt.stream_index = stream->index;
        av_packet_rescale_ts(&pkt, codecContext->time_base, stream->time_base);
        av_interleaved_write_frame(formatContext, &pkt);
        av_packet_unref(&pkt);
    }
}

void RecSectionExporter::onNewImage(CameraImage mimg) {
    if(mimg.img.empty()) {
        qDebug() << "Empty frame received with timestamp: " + QString::number(mimg.timestamp).toStdString();
        return;
    }
    if(!avframe) {
        qDebug() << "Avframe already nulled, but receiving frame to save with timestamp: " + QString::number(mimg.timestamp).toStdString();
        return;
    }

    processImage(mimg, std::vector<cv::Rect>(), std::vector<Pupil>());
}

//void RecSectionExporter::onNewImage(quint64 timestamp, int procMode, const std::vector<cv::Mat> &images, const std::vector<Pupil> &Pupils) {
void RecSectionExporter::onNewImage(CameraImage mimg, int procMode, std::vector<cv::Rect> ROIs, std::vector<Pupil> Pupils) {
    if(mimg.img.empty()) {
        qDebug() << "Empty frame received with timestamp: " + QString::number(mimg.timestamp).toStdString();
        return;
    }
    if(!avframe) {
        qDebug() << "Avframe already nulled, but receiving frame to save with timestamp: " + QString::number(mimg.timestamp).toStdString();
        return;
    }

    processImage(mimg, ROIs, Pupils);
}

cv::Mat RecSectionExporter::resizeARAware(const cv::Mat &image) {
    float aspectSrc = (float)image.cols / image.rows;
    float aspectDst = (float)EXPORT_DEF_WIDTH / EXPORT_DEF_HEIGHT;

    int newWidth, newHeight;

    if (aspectSrc > aspectDst) {
        // Source is wider, fit width
        newWidth = EXPORT_DEF_WIDTH;
        newHeight = static_cast<int>(EXPORT_DEF_WIDTH / aspectSrc);
    } else {
        // Source is taller, fit height
        newHeight = EXPORT_DEF_HEIGHT;
        newWidth = static_cast<int>(EXPORT_DEF_HEIGHT * aspectSrc);
    }

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(newWidth, newHeight));

    cv::Mat output(EXPORT_DEF_HEIGHT, EXPORT_DEF_WIDTH, image.type(), cv::Scalar(0,0,0));

    // Compute centering offset
    int x = (EXPORT_DEF_WIDTH - newWidth) / 2;
    int y = (EXPORT_DEF_HEIGHT - newHeight) / 2;

    resized.copyTo(output(cv::Rect(x, y, newWidth, newHeight)));

    return output;
}

void RecSectionExporter::processImage(CameraImage mimg, std::vector<cv::Rect> ROIs, std::vector<Pupil> Pupils) {

    if(freshStart) {
        drawTimer.start();
        freshStart = false;
    }

//    qDebug() << "Processing image at RecSectionExporter with timestamp: " << mimg.timestamp;

    // use the BGRA, that works
    cv::Mat internalImage((int)height, (int)width, CV_8UC4, cv::Scalar(0,255,0));

    std::vector<cv::Mat> images;
    images.push_back(mimg.img.clone());
    if(mimg.type == STEREO_IMAGE_FILE)
        images.push_back(mimg.imgSecondary.clone());

    for(int i = 0; i < images.size(); i++) {
        cv::Mat tempImage = images[i];
        cv::Mat tempImageRes;
        if (tempImage.channels() == 1)
            cv::cvtColor(tempImage, tempImage, cv::COLOR_GRAY2BGRA);

        if(cropToPDROI) {
            // TODO crop and resize to largest defined height
        }
        if(uniformPortableImageSize) {
            tempImage = resizeARAware(internalImage);
        }
        if(embedFrameInfo) {
            int infoImageHeight = (int)((float)images[i].size().height*0.2f);
            cv::Mat infoImage(infoImageHeight, images[i].size().width, CV_8UC4, cv::Scalar(128,128,128));
            int numRows = 2;
            float fontRowHeight = (float)infoImageHeight/(float)(numRows+1);
            // TODO draw stuff
            //QString text = "F:" + QString::number(mimg.frameNumber) + " T:" + QString::number(mimg.timestamp);
            QStringList texts;
            texts.append("T:" + QString::number(recEventTracker->getTrialAtTimestamp(mimg.timestamp)) +
                "; M:" + recEventTracker->getMessage(mimg.timestamp).messageString);
            texts.append("TS:" + QString::number(mimg.timestamp));
            for(int pp = 0; pp < texts.size(); pp++)
                cv::putText(infoImage,
                            texts[pp].toStdString(), //text
                            cv::Point(infoImage.cols/20.0f, infoImage.rows -(pp*fontRowHeight) -((pp+1)*0.33f*fontRowHeight)),
                            cv::FONT_HERSHEY_PLAIN,
                            getGoodFontScale(texts[pp], fontRowHeight),
                            CV_RGB(0, 0, 0),
                            1);
            cv::vconcat(tempImage, infoImage, tempImage);
        }

        tempImage.copyTo(internalImage(cv::Rect(i*tempImage.size().width, 0, tempImage.size().width, tempImage.size().height)));

    }



    // TODO: "bug" that no matter if we set the presentation timestamps (e.g. for very low desired playback speeds),
    //  the gif will be played e.g. by a we browser at 30 FPS... is it normal?
    //  Should we add extra useless frames in betweeen, to tackle this?
    addFrame(internalImage, drawTimer.elapsed());

    // TODO: properly
    if(mimg.frameNumber >= toFrameNumber)
        cleanup();
}

void RecSectionExporter::onExportAllowedToStart() {

}

void RecSectionExporter::onExportAllowedToEnd() {
    // TODO: currently this has no meaning, as even if called, later we can still receive images here, as it comes from a separate thread
}

void RecSectionExporter::cleanup() {

    // Flush encoder
    avcodec_send_frame(codecContext, nullptr);
    AVPacket pkt = {0};
    av_init_packet(&pkt);
    while (avcodec_receive_packet(codecContext, &pkt) == 0) {
        pkt.stream_index = stream->index;
        av_packet_rescale_ts(&pkt, codecContext->time_base, stream->time_base);
        av_interleaved_write_frame(formatContext, &pkt);
        av_packet_unref(&pkt);
    }

    av_write_trailer(formatContext);

    av_frame_free(&avframe);
    av_frame_free(&avframeTmp);
    av_frame_free(&avframeYuv);
    sws_freeContext(swsCtx);
    sws_freeContext(swsGIFCtx);
    if (!(formatContext->oformat->flags & AVFMT_NOFILE))
        avio_close(formatContext->pb);
    avformat_free_context(formatContext);

    formatContext = nullptr;
    stream = nullptr;
    avframe = nullptr;
    avframeTmp = nullptr;
    avframeYuv = nullptr;
    swsCtx = nullptr;
    swsGIFCtx = nullptr;
    codecContext = nullptr;
}

