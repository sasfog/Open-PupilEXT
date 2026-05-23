
#include <QtCore/qstandardpaths.h>
#include <QtConcurrent>
#include <opencv2/imgcodecs.hpp>
#include <QtCore/qthreadpool.h>
#include "imageWriter.h"
#include "../supportFunctions.h"

// Creates a new image writer that outputs images in the given directory
// If stereo is true, a stereo directory structure is created in the given directory
ImageWriter::ImageWriter(QObject *parent) :
    QObject(parent),
    //stereoMode(stereo),
    applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), parent)) {

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

#ifdef QT_DEBUG
    //av_log_set_level(AV_LOG_DEBUG);
#endif
}

ImageWriter::~ImageWriter() {
    stopWriting();

    nullZip();
};

void ImageWriter::nullZip() {
    // NOTE: For some reason, if these were in stopWriting(), they can set
    //  the imageOutputTargetZipInnerFile to nullptr AFTER the code has checked for it being non-null in
    //  onNewImage, practically causing an access violation. This is a workaround
    delete imageOutputTargetZipInnerFile;
    delete imageOutputTargetZip;
    imageOutputTargetZipInnerFile = nullptr;
    imageOutputTargetZip = nullptr;
}

bool ImageWriter::prepareForWriting(const QString& imageOutputTarget, bool stereo, QSize expectedFrameSize, int expectedFrameRate) {

    // TODO: is this advised like this, if we are inside a thread ?
    imageWriterFormatString = applicationSettings->value("imageWriter.imageSequence.chosenFormat", "tiff").toString();

    if(imageWriterFormatString == "png") {
        int pngCompression = applicationSettings->value("imageWriter.imageSequence.png.compression", "0").toInt();
        writeParams = {cv::IMWRITE_PNG_COMPRESSION , pngCompression};
        qDebug() << "PNG compression: " << pngCompression;
    } else if(imageWriterFormatString == "jpeg") {
        int jpegQuality = applicationSettings->value("imageWriter.imageSequence.jpeg.quality", "100").toInt();
        writeParams = {cv::IMWRITE_JPEG_QUALITY, jpegQuality};
        qDebug() << "JPEG quality: " << jpegQuality;
    } else if(imageWriterFormatString == "webp") {
        int webpQuality = applicationSettings->value("imageWriter.imageSequence.webp.quality", "100").toInt();
        writeParams = {cv::IMWRITE_WEBP_QUALITY, webpQuality};
        qDebug() << "WEBP quality:" << webpQuality;
    } else {
        writeParams = std::vector<int>();
    }

    stereoMode = stereo;

    // TODO: there may be folders whose name ends with zip or .zip .. ?
    qDebug() << "imageOutputTarget = " << imageOutputTarget;

    if (imageOutputTarget.endsWith(".zip")) {
        imageWriterTarget = IWTARGET_ZIP;

        // This is needed due to the workaround (see that method for explanation)
        if(imageOutputTargetZipInnerFile)
            nullZip();

        outputZip = imageOutputTarget;
        outputZipInnerRootDirectory = imageOutputTarget;
        outputZipInnerRootDirectory.chop(4);
        outputZipInnerRootDirectory = outputZipInnerRootDirectory.mid(outputZipInnerRootDirectory.lastIndexOf("/") + 1,
                                                                      outputZipInnerRootDirectory.length() -
                                                                      (outputZipInnerRootDirectory.lastIndexOf("/") +
                                                                       1));
        //qDebug() << outputZipInnerRootDirectory;
        // These need to be set here already, becuase we would like to extract the content already of these files, if we can
        metaSnapshotFileName = outputZipInnerRootDirectory + "/imagerec_meta.xml";
        offlineEventLogFileName = outputZipInnerRootDirectory + "/offline_event_log.xml";

        foundZipAlreadyExist = QFile(imageOutputTarget).exists();
        imageOutputTargetZip = new QuaZip(imageOutputTarget);

        auto openMode = QuaZip::mdCreate;
        if (foundZipAlreadyExist) {
            openMode = QuaZip::mdAdd;

            // IMPORTANT: We need to carefully make a copy of all the filenames found in the file upon opening,
            //  because later we cannot access it without closing the file beforehand. I do not know if this is
            //  meant to be a bug or a feature, but certainly this is the case for the version I tested with, QuaZip 1-4.
            QuaZip fcz(imageOutputTarget);
            if (!fcz.open(QuaZip::mdUnzip)) {
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
            if (ok) {
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
            if (ok) {
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

    } else if (imageOutputTarget.endsWith(".mkv")) {

        int ok = 0; // here the ok means GOOD. It is ffmpeg's return code for good
        //videoStartTimestamp = 0;
        frameInfos.clear();
        videoSidecarContent = "<VideoSidecar>\n";

        imageWriterTarget = IWTARGET_VIDEO;

        outputVideoFilename = imageOutputTarget;

        // Allocate output context
        ok = avformat_alloc_output_context2(&fmt_ctx, nullptr, nullptr, outputVideoFilename.toStdString().c_str());
        if(ok != 0) {
            imageWriterStatus = IWSTATUS_VIDEO_START_FAILURE;
            return false;
        }

        // TODO: encoding to match gray8 etc. and file format

        AVCodecID UcodecID;
        AVPixelFormat UpixelFormat;
        frame = av_frame_alloc();

        //if(imageOutputTarget.endsWith(".mkv"))
        //else
        //    throw ...

        // TODO: rather QMap?
        const QString iwVideoCodec = applicationSettings->value("imageWriter.video.chosenCodec", "CODEC_ID_FFV1").toString();
        if(iwVideoCodec == "AV_CODEC_ID_PRORES") {
            UcodecID = AV_CODEC_ID_PRORES;
            UpixelFormat = AV_PIX_FMT_YUV444P10LE; // have to use color. does not compress color, but supported by ProRes
        //} else if(iwVideoCodec == "AV_CODEC_ID_PNG") {
        //    UcodecID = AV_CODEC_ID_PNG;
        //    UpixelFormat = AV_PIX_FMT_GRAY8; // for CV_8UC1
        //}
        } else if(iwVideoCodec == "AV_CODEC_ID_MPEG4") {
            UcodecID = AV_CODEC_ID_MPEG4;
            UpixelFormat = AV_PIX_FMT_YUV420P; // have to use color. does compress color, worst option
        } else if(iwVideoCodec == "AV_CODEC_ID_MJPEG") {
            UcodecID = AV_CODEC_ID_MJPEG;
            UpixelFormat = AV_PIX_FMT_YUV444P; // have to use color. does not compress color, but supported by MJPEG
        } else { /*if(iwVideoCodec == "AV_CODEC_ID_FFV1")*/
            UcodecID = AV_CODEC_ID_FFV1; // lossless. can work with avi/mkv only afaik
            UpixelFormat = AV_PIX_FMT_GRAY8; // for CV_8UC1
        }

        int iwVideoPngCompression = applicationSettings->value("imageWriter.video.png.compression", "0").toInt();
        int iwVideoMJpegQuality = applicationSettings->value("imageWriter.video.mjpeg.quality", "100").toInt();


        codec = avcodec_find_encoder(UcodecID);
        video_st_main = avformat_new_stream(fmt_ctx, codec);
        if(stereoMode)
            video_st_sec = avformat_new_stream(fmt_ctx, codec);

        //data_st = avformat_new_stream(fmt_ctx, nullptr);
        //data_st->codecpar->codec_type = AVMEDIA_TYPE_SUBTITLE;
        //data_st->codecpar->codec_id   = AV_CODEC_ID_TEXT;

        // Configure codec context
        codec_ctx = avcodec_alloc_context3(codec);
        //codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
        //if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        //    codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        //}
        av_dict_set(&fmt_ctx->metadata, "cues", "1", 0); // DEV to evade broken index problems

        if(iwVideoCodec == "AV_CODEC_ID_PRORES") {
            int iwVideoProResQuality = applicationSettings->value("imageWriter.video.prores.quality", "2").toInt();
            if(iwVideoProResQuality < 2)
                iwVideoProResQuality = 2;
            if(iwVideoProResQuality > 31)
                iwVideoProResQuality = 31;
            codec_ctx->flags |= AV_CODEC_FLAG_QSCALE;
            codec_ctx->global_quality = FF_QP2LAMBDA * iwVideoProResQuality;

            av_opt_set_int(codec_ctx->priv_data, "profile", 2, 0); // ProRes 422 HQ enough
        //} else if(iwVideoCodec == "AV_CODEC_ID_PNG") {
        //    UcodecID = AV_CODEC_ID_PNG;
        //    UpixelFormat = AV_PIX_FMT_GRAY8; // for CV_8UC1
        //}
        } else if(iwVideoCodec == "AV_CODEC_ID_MPEG4") {
            int iwVideoMPEG4Quality = applicationSettings->value("imageWriter.video.mpeg4.quality", "2").toInt();
            if(iwVideoMPEG4Quality < 2)
                iwVideoMPEG4Quality = 2;
            if(iwVideoMPEG4Quality > 31)
                iwVideoMPEG4Quality = 31;
            codec_ctx->flags |= AV_CODEC_FLAG_QSCALE;
            codec_ctx->global_quality = FF_QP2LAMBDA * iwVideoMPEG4Quality;
        } else if(iwVideoCodec == "AV_CODEC_ID_MJPEG") {
            int iwVideoMJpegQuality = applicationSettings->value("imageWriter.video.mjpeg.quality", "2").toInt();
            if(iwVideoMJpegQuality < 2)
                iwVideoMJpegQuality = 2;
            if(iwVideoMJpegQuality > 31)
                iwVideoMJpegQuality = 31;
            codec_ctx->flags |= AV_CODEC_FLAG_QSCALE;
            codec_ctx->global_quality = FF_QP2LAMBDA * iwVideoMJpegQuality;
        } else if(iwVideoCodec == "AV_CODEC_ID_FFV1") {
            int iwVideoFFV1Coder = applicationSettings->value("imageWriter.video.ffv1.coder", "1").toInt();
            if(iwVideoFFV1Coder != 0 && iwVideoFFV1Coder != 1)
                iwVideoFFV1Coder = 1;
            int iwVideoFFV1Context = applicationSettings->value("imageWriter.video.ffv1.context", "1").toInt();
            if(iwVideoFFV1Context != 0 && iwVideoFFV1Context != 1)
                iwVideoFFV1Context = 1;
            // use level 3 as it has all tweaks possible
            av_opt_set_int(codec_ctx->priv_data, "level", 3, 0);
            av_opt_set_int(codec_ctx->priv_data, "coder", iwVideoFFV1Coder, 0);
            av_opt_set_int(codec_ctx->priv_data, "context", iwVideoFFV1Context, 0);
            // write per-slice CRCs for safer archival
            av_opt_set_int(codec_ctx->priv_data, "slicecrc", 1, 0);
            // use auto for multithreading for slices
            codec_ctx->thread_count = 0;
        }

        codec_ctx->codec_id = UcodecID;
        codec_ctx->width = expectedFrameSize.width();   // e.g. from cv::Mat.cols
        codec_ctx->height = expectedFrameSize.height(); // cv::Mat.rows
        //codec_ctx->time_base = AVRational{1, expectedFrameRate};
        codec_ctx->time_base = AVRational{1, 1000}; // because all timestamps are in milliseconds // TODO us
        //data_st->time_base = AVRational{1, 1000}; // because all timestamps are in milliseconds // TODO us
        codec_ctx->framerate = AVRational{expectedFrameRate, 1};
        codec_ctx->pix_fmt = UpixelFormat;

        ok = avcodec_open2(codec_ctx, codec, nullptr);
        if(ok == 0) ok += avcodec_parameters_from_context(video_st_main->codecpar, codec_ctx);
        if(stereoMode && ok == 0) ok += avcodec_parameters_from_context(video_st_sec->codecpar, codec_ctx);
        if(ok == 0) ok += avio_open(&fmt_ctx->pb, outputVideoFilename.toStdString().c_str(), AVIO_FLAG_WRITE);
        if(ok == 0) ok += avformat_write_header(fmt_ctx, nullptr);

        if(ok != 0) {
            imageWriterStatus = IWSTATUS_VIDEO_START_FAILURE;
            return false;
        }

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

    // NOTE: This is a safety measure. Although it is still possible that an onNewImage just reaches a portion of code
    //  that uses one of the pointers that are nulled in this method. If that happens, an access violation will occur.
    imageWriterStatus = IWSTATUS_UNDETERMINED;

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
        // NOTE: These were moved to the destructor of ImageWriter. For some reason, if they are here, they can set
        //  the imageOutputTargetZipInnerFile to nullptr AFTER the code has checked for it being non-null in
        //  onNewImage, practically causing an access violation. As that cannot be caught anyway, the current workaround
        //  is to move the nullptr setting to the destructor of this class, because there we can be almost completely
        //  sure that no onNewImage happens after it.
//        nullZip()
    } else if(imageWriterTarget == IWTARGET_VIDEO) {
        if(!fmt_ctx)
            return;

        videoSidecarContent = videoSidecarContent.append(generateFrameInfoFileContent());
        videoSidecarContent = videoSidecarContent.append("</VideoSidecar>");

        QString videoSidecarFilename = outputVideoFilename.mid(0, outputVideoFilename.lastIndexOf(".")) + ".xml";
        QFile *dataFile = new QFile(videoSidecarFilename);
        if (!dataFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
            std::cout << "Recording failure. Could not open: " << videoSidecarFilename.toStdString() << std::endl;
            delete dataFile;
            dataFile = nullptr;
            //return false;
        }
        QTextStream *textStream = new QTextStream(dataFile);
        *textStream << videoSidecarContent;
        std::cout << videoSidecarFilename.toStdString() << std::endl;
        if(dataFile) {
            dataFile->close();
            dataFile->deleteLater();
        }
        delete textStream;
        delete dataFile;
        dataFile = nullptr;
        textStream = nullptr;

        // Flush encoder
        avcodec_send_frame(codec_ctx, nullptr);
        while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
            av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_unref(pkt);
        }
        //av_frame_unref(frame);

        av_write_trailer(fmt_ctx);
        avio_close(fmt_ctx->pb);

        av_frame_free(&frame);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
        video_st_main = nullptr;
        video_st_sec = nullptr;
        //data_st = nullptr;
        sws_freeContext(swsCtx);
        //
        avcodec_free_context(&codec_ctx);
        // codec = nullptr; // singleton inside, no need to free or set to NULL
        //
        av_packet_free(&pkt); // not to be confused with av_packet_unref(), that has to be done often, this not
    }
    // imageWriterStatus = ImageWriter::IWSTATUS_UNDETERMINED; // moved earlier
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

    // DEV
//    bool canAcceptNewImages = true;

    bool ok = true;
    //qDebug() << "Saving image at recording timestamp: " << QString::number(img.timestamp);

    QString fileName;
//    if(!canAcceptNewImages) {
    if(imageWriterStatus != IWSTATUS_OK) {
        ok = false;
    } else if (imageWriterTarget == IWTARGET_ZIP) {

        if(imageOutputTargetZipInnerFile == nullptr) {
            ok = false;
        } else if (stereoMode) {
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
            cv::imencode(('.'+imageWriterFormatString).toStdString(), img.imgS, imencodeBuffer, writeParams);
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
            //QtConcurrent::run(cv::imwrite, filepathSecondary.toStdString(), img.imgS, writeParams);
            ok &= cv::imwrite(fileName.toStdString(), img.imgS, writeParams);
        } else {
            // Write every image over a thread pool managed by QT, this way nothing blocks and we can write images very fast (cpu heavy)
            fileName = outputDirectory + "/" + QString::number(img.timestamp) + "." + imageWriterFormatString;
            //QtConcurrent::run(cv::imwrite, filepath.toStdString(), img.img, writeParams);
            ok &= cv::imwrite(fileName.toStdString(), img.img, writeParams);
        }

    } else if(imageWriterTarget == IWTARGET_VIDEO) {

        //if(videoStartTimestamp == 0)
        //    videoStartTimestamp = img.timestamp;
        // TODO: yet this is just assigning the same value, but later I plan to add microsec resolution, that
        //  will likely cause the stereo timestamps to differ even for "one" stereo frame. This is already an
        //  important to do task, as the quantization error (fall into one or the next ms-bin) causes stereo timing problems.
        frameInfos.push_back({img.timestamp});
        //frameInfos.push_back({img.timestamp, 'M'});
        writeVideoFrame(img.img, img.timestamp, 'M');
        if(stereoMode) {
            //frameInfos.push_back({img.timestamp, 'S'});
            writeVideoFrame(img.imgS, img.timestamp, 'S');
        }

        //addSubtitleAtTimestamp(QString::number(img.timestamp), img.timestamp);
    }

    if (!ok) {
        // For the special case when an onNewImage arrived after the stopWriting. This time we omit the explicit GUI warning message
        if(imageWriterStatus == IWSTATUS_UNDETERMINED) {
            qDebug() << "An image arrived at onNewImage of imageWriter, although writing has been stopped before (or in the meanwhile).";
            qDebug() << "Image was dropped. Its timestamp was: " << QString::number(img.timestamp);
        } else {
            emit writingFailed();
        }
    }
}

void ImageWriter::writeVideoFrame(const cv::Mat &img, const uint64 &timestamp, QChar cameraIdentity) {

    frame->format = codec_ctx->pix_fmt;
    frame->width  = codec_ctx->width;
    frame->height = codec_ctx->height;
    int bufferSuccess = av_frame_get_buffer(frame, 32);

    if(bufferSuccess != 0)
        throw std::runtime_error("Could not get buffer for writing frame..."); // runtime_error = exception with string message

    // Lets see if we need to convert the color or not. Some codecs need us to use "color"
    if(codec_ctx->pix_fmt != AV_PIX_FMT_GRAY8) {
        if(!swsCtx) {
            swsCtx = sws_getContext(
                    img.cols, img.rows, AV_PIX_FMT_GRAY8,   // src
                    img.cols, img.rows, codec_ctx->pix_fmt, // dst (e.g. YUV420P)
                    SWS_BILINEAR, nullptr, nullptr, nullptr
            );
        }
        uint8_t* inData[1] = { img.data };
        int inLinesize[1]  = { (int)img.step };
        sws_scale(swsCtx, inData, inLinesize, 0, img.rows, frame->data, frame->linesize);
    } else {
        memcpy(frame->data[0], img.data, img.cols * img.rows);
    }

    // TODO: NOT CREATE ONE SWSCONTEXT EVERY TIME

    // TODO: RESET TIME ZERO TO COUNT NEW.. NOT SURE IF WE CAN USE time_base of 1 ms.
    //  Otherwise, just add a "dummy" timestamp here, and use real timestamp in the accompanying annotation file
    //frame->pts = img.timestamp - videoStartTimestamp; // should always be positive ofc
    frame->pts = timestamp - frameInfos[0].timestamp;

    // Encode
    avcodec_send_frame(codec_ctx, frame);
    //AVPacket pkt;
    pkt = av_packet_alloc(); // this does the "new ..." and init too
    while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
        if(cameraIdentity == 'M')
            pkt->stream_index = video_st_main->index;
        else
            pkt->stream_index = video_st_sec->index;
        av_interleaved_write_frame(fmt_ctx, pkt);
        av_packet_unref(pkt);
    }
    av_frame_unref(frame);
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
    } else if(imageWriterTarget == IWTARGET_VIDEO) {
        videoSidecarContent = videoSidecarContent.append(content);
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
    } else if(imageWriterTarget == IWTARGET_VIDEO) {
        videoSidecarContent = videoSidecarContent.append(content);
    } else {
        qDebug() << "Error during writing offline event log.";
        return false;
    }
    return ok;
}

void ImageWriter::embedFileWithinVideoBeforeHeader(const QString &content, const QString &fileName) {
    int contentSize = content.toStdString().length();

    AVStream* att = avformat_new_stream(fmt_ctx, nullptr);
    if (!att) {
        qWarning("Failed to allocate attachment stream");
        return;
    }

    att->codecpar->codec_type = AVMEDIA_TYPE_ATTACHMENT;
    att->codecpar->codec_id   = AV_CODEC_ID_NONE;
    att->codecpar->codec_tag = 0;

    att->codecpar->extradata = (uint8_t*)av_malloc(contentSize + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!att->codecpar->extradata) {
        qWarning("Failed to allocate memory for attachment extradata");
        return;
    }
    memcpy(att->codecpar->extradata, content.toUtf8().constData(), contentSize);
    memset(att->codecpar->extradata + contentSize, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    att->codecpar->extradata_size = contentSize;

    //att->codecpar->extradata = (uint8_t*)av_malloc(contentSize + AV_INPUT_BUFFER_PADDING_SIZE);
    //memcpy(att->codecpar->extradata, content.toUtf8().constData(), contentSize);
    //memset(att->codecpar->extradata + contentSize, 0, AV_INPUT_BUFFER_PADDING_SIZE); // padding
    //att->codecpar->extradata_size = contentSize;

    //att->codecpar->extradata  = (uint8_t*)av_malloc(contentSize);
    //memcpy(att->codecpar->extradata, content.toStdString().c_str(), contentSize);
    //att->codecpar->extradata_size = contentSize;

    av_dict_set(&att->metadata, "filename", fileName.toStdString().c_str(), 0);
    av_dict_set(&att->metadata, "mimetype", "application/json", 0);
}

void ImageWriter::addSubtitleAtTimestamp(const QString &content, const uint64 &timestamp) {

    /*
    pkt = av_packet_alloc(); // this does the "new ..." and init too
    pkt->data = (uint8_t*)content.data(); // content.toStdString().c_str()
    pkt->size = content.size();
    pkt->stream_index = data_st->index;
    pkt->pts = pkt->dts = timestamp - frameInfos[0].timestamp; // IMPORTANT subtraction to rebase at 0
    pkt->duration = 1;

    av_interleaved_write_frame(fmt_ctx, pkt);
    av_packet_unref(pkt);
     */
}

QString ImageWriter::generateFrameInfoFileContent() {

    QByteArray textContent;

    QDomDocument document;
    QDomElement root;

    root = document.createElement("FrameInfoFile");
    document.appendChild(root);
    qDebug() << "Creating a fresh XML. Version: " << QString::number(currentFrameInfoFileVersion);
    //std::cout << root.nodeName().toStdString() << std::endl;

    if (!root.hasAttribute("Version") || root.attribute("Version","1").toUShort() < currentFrameInfoFileVersion)
    {
        // TODO: Safer logic for this? Also at reading
        root.setAttribute("Version", QString::number(currentFrameInfoFileVersion));
    }

    QDomElement currObj;

    for (size_t i = 0; i < frameInfos.size(); i++) {
        currObj = document.createElement("FrameInfo");
        currObj.setAttribute("TimestampMs", QString::number(frameInfos[i].timestamp));
        //currObj.setAttribute("CameraIdentity", frameInfos[i].cameraIdentity);
        root.appendChild(currObj);
    }

    // NOTE: search intervals are only inclusive on the left, but exclusive on the right. Consider this
    // TODO: clear file even if appended, as new XML is flushed into it

    return document.toString();
}