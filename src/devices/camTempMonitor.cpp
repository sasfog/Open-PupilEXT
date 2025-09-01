#include "camTempMonitor.h"

CamTempMonitor::CamTempMonitor(Camera *camera) : camera(camera) {
    stereo=false;
    if(camera->getType()==LIVE_SINGLE_CAMERA) {
        //stereo=false;
//        std::cerr << "started camTempMonitor with single camera" << std::endl;
    } else if(camera->getType()==LIVE_STEREO_CAMERA) {
        stereo=true;
//        std::cerr << "started camTempMonitor with stereo camera" << std::endl;
    } //else {
    //    return;
    //}
}
 
bool CamTempMonitor::running() const {
//    std::cerr << "camTempMonitor running" << std::endl;
    return m_running;
}

void CamTempMonitor::run() {
//    std::cerr << "camTempMonitor thread: RUN()" << std::endl;

    m_running = true;
    std::vector<double> temps;
    bool warmupDone = false;
    bool hasDeltaTimeData = false;

    do {
        if(!camera || (camera && !camera->isOpen()))
            continue;
        
        if(!stereo) {
            temps = {static_cast<SingleCamera*>(camera)->getTemperature(), MINIMUM_DEVICE_TEMPERATURE};
        } else {
            temps = static_cast<StereoCamera*>(camera)->getTemperatures();
        }
        emit camTempChecked(temps);

        QThread::msleep(checkIntervalSec*1000); // TODO: this time should be configurable in general settings dialog, as well as the option to track camera temperature or not

        if(warmupDone)
            continue;

        tempChecks.push_back(temps);

        if(!hasDeltaTimeData && checkIntervalSec * tempChecks.size() >= warmupStableDeltaTime) {
            hasDeltaTimeData = true;
            emit cameraWarmupHasDeltaTimeData();
        }

        if(hasDeltaTimeData &&
            temps[0] - tempChecks[tempChecks.size()-(warmupStableDeltaTime/checkIntervalSec)][0] <= warmupStableDeltaTemp &&
            temps[1] - tempChecks[tempChecks.size()-(warmupStableDeltaTime/checkIntervalSec)][1] <= warmupStableDeltaTemp ) {

            float cavg0 = CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE;
            float cavg1 = CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE;
            for(auto chk : tempChecks) {
                cavg0 = (cavg0 + chk[0])/2;
                cavg1 = (cavg1 + chk[1])/2;
            }

            if( camera->getType() == LIVE_SINGLE_CAMERA && cavg0 == CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE ||
                camera->getType() == LIVE_STEREO_CAMERA && (cavg0 == CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE || cavg1 == CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE) ) {
                emit cameraWarmUpReadingsInvalid();
            } else {
                emit cameraWarmedUp();
            }
            warmupDone = true;
        }
    } while(m_running); // must happen here, as it can happen that this thread is scheduled for deletion (set m_running to false) while it is asleep

//    std::cerr << "camTempMonitor thread FINISHING" << std::endl;

    emit finished();
}
 
void CamTempMonitor::setRunning(bool running)
{
//    std::cerr << "camTempMonitor thread setRunning()" << std::endl;

    if (m_running == running)
        return;
 
    m_running = running;
    emit runningChanged(running);
}


