
#ifndef PUPILEXT_EYEDETECTOR_H
#define PUPILEXT_EYEDETECTOR_H

#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include "../supportFunctions.h"
#include <QRectF>
#include <QString>
#include <QDebug>

/**
 * https://docs.opencv.org/4.x/db/d28/tutorial_cascade_classifier.html
*/
class EyeDetector
{
    private:
    cv::String modelLoc = "./Mi_Models/haarcascade_eye.xml";

    cv::CascadeClassifier eyes_cascade;

    std::vector<cv::Rect> eyes;

    float width, height;
    public:
    explicit EyeDetector(){
        if ( !eyes_cascade.load( modelLoc )){
            qDebug() << "Error loading eyes cascade";
        }
    };

    void generateEyeRoiRs(const QRectF& imageROI, const cv::Mat& image){
        cv::Mat frame_gray;
        cv::equalizeHist( image, frame_gray);
        //float ratio = imageROI.width() / imageROI.height();    
        //cv::resize(frame_gray, frame_gray, cv::Size(64, ratio * 64), 0, 0, cv::INTER_CUBIC);
       
        eyes_cascade.detectMultiScale( frame_gray, eyes);
        qDebug() << "Eyes size: " << eyes.size();
        for (int i = 0; i < eyes.size(); i++){
            qDebug() << "Eyes x: " << eyes[i].x << "Eyes y: " << eyes[i].y << "Eyes w: " << eyes[i].width << "Eyes h: " << eyes[i].height;
            qDebug() << "Image ROI w: " << imageROI.width() << "Image ROI h: " << imageROI.height();
        }
        width = imageROI.width();
        height = imageROI.height();
    };

    QRectF getEyeRoi(int index){
        return QRectF(static_cast<float>(eyes[index].x / width), static_cast<float>(eyes[index].y / height), static_cast<float>(eyes[index].width / width), static_cast<float>(eyes[index].height / height));
    }

    int getRoiSize(){
        return eyes.size();
    }

};

#endif //PUPILEXT_EYEDETECTOR_H