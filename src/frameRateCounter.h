#pragma once

/**
    @author Moritz Lode, Gabor Benyei
*/

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <iostream>

/**
    Header-only class to calculate the framerate in frames per second for general objects

    Difference to the CameraFrameRateCounter is that the calculated fps are not based on image timestamps but can be anything arbitrary using the signals time

    count(): based on number of calls of this function, framerate is calculated
*/

//#define FPS_COUNTER_OBSERVED_WINDOW 4000
#define FPS_COUNTER_OBSERVED_WINDOW 3000
//#define FPS_COUNTER_TIMEOUT_WINDOW 4000
#define FPS_COUNTER_EMIT_WINDOW 40

// TODO: framecount emit now has less of an important role, as Pupil objects carry the frame number as well.. ?

class FrameRateCounter : public QObject {
Q_OBJECT

public:

    FrameRateCounter(QObject *parent=nullptr): QObject(parent), totalFrameCountB(0), fpsB_(0.0)	{
        connect(&timeout_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
        ownTimer.restart();
        perEmitTimer.restart();
    }

    ~FrameRateCounter() = default;

    double fps() {
        return fpsB_;
    }

    int frameCount() {
        return totalFrameCountB;
    }

    void reset() {
        fpsB_ = 0;
        totalFrameCountB = 0;
    //    ownTimer.restart();
        perEmitTimer.restart();
        emit fps(fpsB_);
        emit framecount(totalFrameCountB);
    }

public slots:

    //void setParentName(std::string name) { _name = name; };

    // Slot that is connected to an arbitrary signal which is supposed to be counted
    void count() {

        //if(_name == "file camera") {
        //    std::cout << "file camera - perEmitTimer - " << QString::number(perEmitTimer.elapsed()).toStdString() << std::endl;
        //}

        countedTimestamps.push_back(ownTimer.elapsed());
        totalFrameCountB++;
//        std::cout << QString::number(ownTimer.elapsed()).toStdString() << std::endl;

        if(perEmitTimer.elapsed() > FPS_COUNTER_EMIT_WINDOW) {
            doCount();

            perEmitTimer.restart();

            //ownTimer.restart();
            timeout_timer.start(FPS_COUNTER_OBSERVED_WINDOW); // TODO: why not restart?
        }
    }

protected:

    QTimer timeout_timer;
    QElapsedTimer ownTimer;
    QElapsedTimer perEmitTimer;
    // unsigned short is okay for even 1 minute.. but we will reset the time on any timeout, and a timeout happens after 4 secs, so it is still fine

//    // NOTE: If everything works fine, all the timestamps should fit on an uint16 between two consecutive emits,
//    //  but in case of a debug build, where breakpoint stops might introduce higher times, we need to use a larger container.
//    //  Still in case of release builds, the largest int to fit in there is the max of uint16,
//    //  and accordingly the longest time that we can wait (in freezed thread) without overflow is ~65 sec.
//#ifdef DEBUG
//    QVector<quint64> countedTimestamps;
//    //QVector<quint16> countedTimestamps;
//#else
//    QVector<quint16> countedTimestamps;
//#endif
    QVector<quint64> countedTimestamps;
    double fpsB_;
    int totalFrameCountB;
    //std::string _name = "default";

private:

    FrameRateCounter(const FrameRateCounter& other) = delete;
    FrameRateCounter& operator=(const FrameRateCounter& rhs);

    void doCount() {
        // remove old items from vector
        int i = 0;
        while(i < countedTimestamps.size()) {
            if(ownTimer.elapsed() - countedTimestamps[i] > FPS_COUNTER_OBSERVED_WINDOW) {
                countedTimestamps.pop_front();
            } else {
                i++;
            }
        }
        //i = countedTimestamps.size();

        if(i > 0)
            fpsB_ =  float(i) / (float(countedTimestamps[i-1] - countedTimestamps[0]) / 1000.0f);
        else
            fpsB_ = 0.0;

        //std::cout << _name << " FPS = " << fpsB_ << std::endl;
        emit fps(fpsB_);
        emit framecount(totalFrameCountB);
    }

private slots:

    // Slot callback that is called when the timeout runs out
    // Due to no new signals being received, the new framerate is calculated which should go to zero
    void onTimeout() {
        //if(ownTimer.elapsed() > FPS_COUNTER_OBSERVED_WINDOW) { // this "if" is useless, the call is already timed
            doCount();
            //ownTimer.restart();
            timeout_timer.start(FPS_COUNTER_OBSERVED_WINDOW);
        //}
    };

signals:

    void fps(double fps);
    void framecount(int framecount);

};
