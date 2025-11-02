#ifndef PUPILALGOSIMPLE_PUPIL_H
#define PUPILALGOSIMPLE_PUPIL_H

/*
Copyright (c) 2018, Thiago Santini / University of Tübingen

        Permission is hereby granted, free of charge, to any person obtaining a copy of
this software, source code, and associated documentation files (the "Software")
to use, copy, and modify the Software for academic use, subject to the following
        conditions:

1) The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

2) Modifications to the source code should be made available under
free-for-academic-usage licenses.

For commercial use, please contact the authors.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
        WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 Modified 01.01.2010: Moritz Lode

*/

#include <opencv2/core/types.hpp>
#include "../pDataTypeEnum.h"

//TODO:
// After a quite unpleasant journey I realized that if I include ANY Qt-related header in here, compilation will break:
// \vcpkg_installed\x64-windows\include\oneapi\tbb\profiling.h(229): error C2059: syntax error: ')'
// \vcpkg_installed\x64-windows\include\oneapi\tbb\profiling.h(229): error C2334: unexpected token(s) preceding '{'; skipping apparent function body
// \vcpkg_installed\x64-windows\include\oneapi\tbb\profiling.h(231): error C2059: syntax error: 'const'
// \vcpkg_installed\x64-windows\include\oneapi\tbb\profiling.h(231): error C2334: unexpected token(s) preceding '{'; skipping apparent function body
// Yet this is the reason there is a separate pDataTypeEnum.h and pDataTypes.h, to let them be included separately.
// What could we do?

//#include <QtCore/QString>
//#undef emit
// ...
//#define emit Q_EMIT

#define NO_CONFIDENCE -1.0


class Pupil : public cv::RotatedRect {

public:

    Pupil(const RotatedRect &outline, const float &confidence) :
            RotatedRect(outline),
            confidence(confidence),
            outline_confidence(NO_CONFIDENCE),
            eyelid(0),
            physicalDiameter(-1.0),
            undistortedDiameter(-1.0),
            algorithmName(""),
            eyeIdentity('X'),
            BRISQUEFullImage(-1.0),
            BRISQUEPDROI(-1.0) {
    }

    Pupil(const RotatedRect &outline,
          const float &confidence,
          const float &outline_confidence,
          const float &eyelid,
          const float &physicalDiameter,
          const float &undistortedDiameter) :
            RotatedRect(outline),
            confidence(confidence),
            outline_confidence(outline_confidence),
            eyelid(eyelid),
            physicalDiameter(physicalDiameter),
            undistortedDiameter(undistortedDiameter),
            algorithmName(""),
            eyeIdentity('X'),
            BRISQUEFullImage(-1.0),
            BRISQUEPDROI(-1.0) {
    }

    Pupil(const Pupil &other) :
            RotatedRect(other),
            confidence(other.confidence),
            outline_confidence(other.outline_confidence),
            eyelid(other.eyelid),
            physicalDiameter(other.physicalDiameter),
            undistortedDiameter(other.undistortedDiameter),
            algorithmName(other.algorithmName),
            eyeIdentity(other.eyeIdentity),
            BRISQUEFullImage(other.BRISQUEFullImage),
            BRISQUEPDROI(other.BRISQUEPDROI) {
    }

    Pupil(const RotatedRect &outline) :
            RotatedRect(outline),
            confidence(NO_CONFIDENCE),
            outline_confidence(NO_CONFIDENCE),
            eyelid(0),
            physicalDiameter(-1.0),
            undistortedDiameter(-1.0),
            algorithmName(""),
            eyeIdentity('X'),
            BRISQUEFullImage(-1.0),
            BRISQUEPDROI(-1.0) {
    }

    Pupil() {
        clear();
    }

    ~Pupil() = default;

    float confidence;

    float outline_confidence;

    float eyelid;

    float physicalDiameter;
    float undistortedDiameter;

    std::string algorithmName;
    char eyeIdentity;

    float BRISQUEFullImage;
    float BRISQUEPDROI;
    //float BRISQUEPDInternal;

    void clear() {
        angle = -1.0;
        center = { -1.0, -1.0 };
        size = { -1.0, -1.0 };
        confidence = NO_CONFIDENCE;
        outline_confidence = NO_CONFIDENCE;
        eyelid=0;
        physicalDiameter=-1.0;
        undistortedDiameter=-1.0;
        algorithmName="";
        eyeIdentity='X';
        BRISQUEFullImage=-1.0;
        BRISQUEPDROI=-1.0;
        //BRISQUEPDInternal=-1.0;

        // IMPORTANT: if you add anything new, be sure to update the object copy method, and constructors too,
        //  and also modify n_channels in the Permissive variant os LSL streaming channel allocation.
    }

    void resize(const float &xf, const float &yf) {
        center.x *= xf;
        center.y *= yf;
        size.width *= xf;
        size.height *= yf;
    }

    void resize(const float &f) {
        center *= f;
        size *= f;
    }

    void shift( cv::Point2f p ) {
        center += p;
    }

    std::vector<cv::Point2f> rectPoints() {
        cv::Point2f pointsArr[4];
        points(pointsArr);

        std::vector<cv::Point2f> v(std::begin(pointsArr), std::end(pointsArr));

        return v;
    }

    bool valid(const double &confidenceThreshold=NO_CONFIDENCE) const {
        return center.x > 0 &&
               center.y > 0 &&
               size.width > 0 &&
               size.height > 0 &&
               (confidence > confidenceThreshold || outline_confidence > confidenceThreshold);
    }

    bool hasOutline() const {
        return size.width > 0 && size.height > 0;
    }

    float width() const {
        return size.width;
    }

    float height() const {
        return size.height;
    }

    float majorAxis() const {
        return std::max<float>(size.width, size.height);
    }

    float minorAxis() const {
        return std::min<float>(size.width, size.height);
    }

    float diameter() const {
        return majorAxis();
    }

    float circumference() const {
        if(size.width==-1 || size.height==-1) return -1.0;

        float a = 0.5*majorAxis();
        float b = 0.5*minorAxis();
        return CV_PI * abs( 3*(a+b) - sqrt( 10*a*b + 3*( pow(a,2) + pow(b,2) ) ) );
    }

    double getPData(PDataType f) const {
        switch(f) {
            case PDataType::PUPIL_CENTER_X:
                return center.x;
            case PDataType::PUPIL_CENTER_Y:
                return center.y;
            case PDataType::PUPIL_MAJOR:
                return majorAxis();
            case PDataType::PUPIL_MINOR:
                return minorAxis();
            case PDataType::PUPIL_WIDTH:
                return width();
            case PDataType::PUPIL_HEIGHT:
                return height();
            case PDataType::PUPIL_DIAMETER:
                return diameter();
            case PDataType::PUPIL_UNDIST_DIAMETER:
                return undistortedDiameter;
            case PDataType::PUPIL_PHYSICAL_DIAMETER:
                return physicalDiameter;
            case PDataType::PUPIL_CONFIDENCE:
                return confidence;
            case PDataType::PUPIL_OUTLINE_CONFIDENCE:
                return outline_confidence;
            case PDataType::PUPIL_CIRCUMFERENCE:
                return circumference();
            case PDataType::PUPIL_RATIO:
                return (double)majorAxis() / minorAxis();
            case PDataType::PUPIL_ANGLE:
                return angle;
            case PDataType::PUPIL_BRISQUE_FULL_IMAGE:
                return BRISQUEFullImage;
            case PDataType::PUPIL_BRISQUE_PD_ROI:
                return BRISQUEPDROI;
            //case PDataType::PUPIL_BRISQUE_PD_Internal:
            //    return BRISQUEPDInternal;
        }
    }

};

#endif //PUPILALGOSIMPLE_PUPIL_H
