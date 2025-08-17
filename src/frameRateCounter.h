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

#define FPS_COUNTER_OBSERVED_WINDOW 999
#define FPS_COUNTER_TIMEOUT_WINDOW 1499

// TODO: framecount emit now has less of an important role, as Pupil objects carry the frame number as well.. ?

class FrameRateCounter : public QObject {
Q_OBJECT

public:

    FrameRateCounter(QObject *parent=nullptr): QObject(parent), totalFrameCount(0), secFrameCount(0), fps_(0.0)	{
        connect(&timeout_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
    }

    ~FrameRateCounter() = default;

    double fps() {
        return fps_;
    }

    int frameCount() {
        return totalFrameCount;
    }

    void reset() {
        fps_ = 0;
        totalFrameCount = 0;
        secFrameCount = 0;
        doEmit();
    }

public slots:

    // Slot that is connected to an arbitrary signal which is supposed to be counted
    void count() {
        // Start counting the time so that at each second, a fps value is output
        // The timeout time is needed as if the count slot is not called due to no signals being received, the framerate will not update
        if(totalFrameCount == 0) {
            doEmit();
        }

        // When a second is over, calculate the (average) fps value based on the average frames received in that timeinterval
        if(m_timer.elapsed() > FPS_COUNTER_OBSERVED_WINDOW) {
            fps_ =  float(secFrameCount) / (m_timer.elapsed() / 1000.0f);
            secFrameCount = 0;
            doEmit();
        }

        // Increase the signals received in the one second interval
        totalFrameCount++;
        secFrameCount++;
    }

protected:

    QElapsedTimer m_timer;
    QTimer timeout_timer;

    int totalFrameCount;
    int secFrameCount;

    double fps_;

private:

    FrameRateCounter(const FrameRateCounter& other) = delete;
    FrameRateCounter& operator=(const FrameRateCounter& rhs);

    void doEmit() {
        m_timer.restart();
        timeout_timer.start(FPS_COUNTER_TIMEOUT_WINDOW);
        emit fps(fps_);
        emit framecount(totalFrameCount);
    };

private slots:

    // Slot callback that is called when the timeout runs out
    // Due to no new signals being received, the new framerate is calculated which should go to zero
    void onTimeout() {
        fps_ =  float(secFrameCount) / (m_timer.elapsed() / 1000.0f);
        secFrameCount = 0;
        doEmit();
    };

signals:

    void fps(double fps);
    void framecount(int framecount);

};