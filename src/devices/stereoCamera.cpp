
#include "stereoCamera.h"
#include "camTempMonitor.h"
#include <QThread>
#include <QDebug>

#ifdef USE_PYLON
#include <pylon/TlFactory.h>

// TODO:
//  - remove attachCameras methods, and make them into the open() method
//  - converge single camera and stereo camera so much that the same settings dialog could act on any of them

// Creates a new stereo camera
// The stereo camera is implemented using Pylon's CBaslerUniversalInstantCameraArray
// A stereo camera consists of two cameras configured to receive hardware trigger signals
// Stereo camera images handled using a StereoCameraImageEventHandler
StereoCamera::StereoCamera(QObject* parent) : Camera(parent),
            cameras(2),
            frameCounter(new CameraFrameRateCounter(parent)),
            cameraCalibration(new StereoCameraCalibration()),
            calibrationThread(new QThread()),
            lineSource("Line1") {

    settingsDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    if(!settingsDirectory.exists()) {
// mkdir(".") DOES NOT WORK ON MACOS, ONLY WINDOWS. (Reported on MacOS 12.7.6 and Windows 10)
//        settingsDirectory.mkdir(".");
        QDir().mkpath(settingsDirectory.absolutePath());
    }

    // Calibration thread working
    cameraCalibration->moveToThread(calibrationThread);
    calibrationThread->start();
    calibrationThread->setPriority(QThread::HighPriority);

    connect(frameCounter, SIGNAL(fps(double)), this, SIGNAL(fps(double)));
    connect(frameCounter, SIGNAL(framecount(int)), this, SIGNAL(framecount(int)));
}

/*
// Creates a stereo camera and attaches the two given Pylon camera device information
StereoCamera::StereoCamera(const CDeviceInfo &diMain, const CDeviceInfo &diSecondary, QObject* parent)
        : StereoCamera(parent) {

    cameras[0].Attach(CTlFactory::GetInstance().CreateDevice(diMain));
    cameras[1].Attach(CTlFactory::GetInstance().CreateDevice(diSecondary));
}

// Creates a stereo camera and attaches the two given Pylon device names (fullnames)
StereoCamera::StereoCamera(const QString &friendlyNameMain, const QString &friendlyNameSecondary, QObject* parent) {

    auto diMain = CDeviceInfo().SetFullName(friendlyNameMain.toStdString().c_str());
    auto diSecondary = CDeviceInfo().SetFullName(friendlyNameSecondary.toStdString().c_str());

    // TODO: LOOKUP

    StereoCamera(diMain, diSecondary, parent);

}
 */

// Destroys the stereo camera
// Closes the camera array
StereoCamera::~StereoCamera() {
    if(cameras.IsOpen()) {
        safelyCloseCameras();
    }
    delete cameraImageEventHandler;
    if (cameraCalibration != nullptr)
        cameraCalibration->deleteLater();
    if (calibrationThread != nullptr) {
        calibrationThread->quit();
        calibrationThread->deleteLater();
    }
}

void StereoCamera::genericExceptionOccured(const GenericException &e) {
    QThread::msleep(1000);
    std::cerr << "A Pylon exception occurred." << std::endl<< e.GetDescription() << std::endl;
    if (cameras[0].IsCameraDeviceRemoved() || cameras[1].IsCameraDeviceRemoved()) {
        emit cameraDeviceRemoved();
//        cameras.Close();
//        cameras.DetachDevice();
//        cameras.DestroyDevice();
        safelyCloseCameras();
    }
}

// Attaches the main and secondary cameras to the camera array, based on their given device information
void StereoCamera::attachCameras(const QString &friendlyNameMain, const QString &friendlyNameSecondary) {

    auto diMain = CDeviceInfo().SetFriendlyName(friendlyNameMain.toStdString().c_str());
    auto diSecondary = CDeviceInfo().SetFriendlyName(friendlyNameSecondary.toStdString().c_str());

    // TODO: LOOKUP HERE, NOT IN STEREO CAMERA SETTINGS DIALOG

    attachCameras(diMain, diSecondary);
}

// Attaches the main and secondary cameras to the camera array, based on their given device information
void StereoCamera::attachCameras(const CDeviceInfo &diMain, const CDeviceInfo &diSecondary) {

    // If cameras are already attached to the array, remove them
    if(cameras.GetSize() > 0) {
        if(cameras.IsGrabbing())
            cameras.StopGrabbing();
        cameras.Close();
        cameras.DetachDevice();
        cameras.DestroyDevice();
//        safelyCloseCameras();
    }

    CTlFactory& TlFactory = CTlFactory::GetInstance();
    //IGigETransportLayer* pTl = dynamic_cast<IGigETransportLayer*>(TlFactory.CreateTl( Pylon::BaslerGigEDeviceClass ));

    cameras[0].Attach(TlFactory.CreateDevice(diMain));
    cameras[1].Attach(TlFactory.CreateDevice(diSecondary));

    std::cout<<"Attached Camera0:" << cameras[0].GetDeviceInfo().GetFriendlyName() << std::endl;
    std::cout<<"Attached Camera1:" << cameras[1].GetDeviceInfo().GetFriendlyName() << std::endl << std::endl;
}

// Opens the stereo camera through its corresponding camera array
// If the stereo camera is already open, it is first closed and then reopened again
// CAUTION: Its important for the stereo cameras to be in sync, that the stereo camera is first opened, and only then the hardware trigger source is started
// If the hardware triggers are started before opening the camera, the camera images will not be in sync due to the sequential opening of the camera
// (one camera will receive a trigger signal before the other)
void StereoCamera::open(bool enableHardwareTrigger) {

    if(cameras.GetSize() < 2) {
        std::cerr << "StereoCamera: must have two cameras connected."<< std::endl;
        return;
    }

    if(cameras.IsOpen()) {
        if(cameras.IsGrabbing())
            cameras.StopGrabbing();
        cameras.Close();
//        safelyCloseCameras();
    }

    try {
        // Register the configurations of the cameras

        cameraConfigurationEventHandler0 = new CameraConfigurationEventHandler();
        cameraConfigurationEventHandler1 = new CameraConfigurationEventHandler();
        connect(cameraConfigurationEventHandler0, SIGNAL(cameraDeviceRemoved()), this, SIGNAL(cameraDeviceRemoved()));
        connect(cameraConfigurationEventHandler1, SIGNAL(cameraDeviceRemoved()), this, SIGNAL(cameraDeviceRemoved()));
        cameras[0].RegisterConfiguration(cameraConfigurationEventHandler0, RegistrationMode_ReplaceAll, Cleanup_Delete);
        cameras[1].RegisterConfiguration(cameraConfigurationEventHandler1, RegistrationMode_ReplaceAll, Cleanup_Delete);

        // Setting both to receive hardware trigger signals on the given line source
        // NOTE: always true except when emulated cameras are used
        if(enableHardwareTrigger) {
            hardwareTriggerConfiguration0 = new HardwareTriggerConfiguration(lineSource.toStdString().c_str());
            hardwareTriggerConfiguration1 = new HardwareTriggerConfiguration(lineSource.toStdString().c_str());
            cameras[0].RegisterConfiguration(hardwareTriggerConfiguration0, RegistrationMode_Append, Cleanup_Delete);
            cameras[1].RegisterConfiguration(hardwareTriggerConfiguration1, RegistrationMode_Append, Cleanup_Delete);
//            cameras[0].TriggerActivation.TrySetValue(TriggerActivation_RisingEdge);
//            cameras[1].TriggerActivation.TrySetValue(TriggerActivation_RisingEdge);
//            cameras[0].TriggerMode.TrySetValue(TriggerMode_On);
//            cameras[1].TriggerMode.TrySetValue(TriggerMode_On);
        }

        cameraImageEventHandler = new StereoCameraImageEventHandler(this->parent());
        connect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), this, SIGNAL(onNewGrabResult(CameraImage)));
        connect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), frameCounter, SLOT(count(CameraImage)));
        //connect(cameraImageEventHandler, SIGNAL(needsTimeSynchronization()), this, SLOT(resynchronizeTime()));
        connect(cameraImageEventHandler, SIGNAL(imagesSkipped()), this, SIGNAL(imagesSkipped()));

        // Register the image event handler
        // IMPORTANT: For both cameras the same handler object is registered, as the handler must receive both main and secondary images to create a single stereo camera image
        cameras[0].RegisterImageEventHandler(cameraImageEventHandler, RegistrationMode_ReplaceAll, Cleanup_None); // Cleanup_None as its deleted in unregister
        cameras[1].RegisterImageEventHandler(cameraImageEventHandler, RegistrationMode_ReplaceAll, Cleanup_None);

        cameras.Open();

        // GIGE ticks to timestamp conversion initialization, NOTE: THIS HAS TO HAPPEN AFTER CAMERA IS OPENED
        // NOTE: currently we rely on the assumption that both cameras use the same ticks for counting.
        if (cameras[0].GetDeviceInfo().GetTLType() == "BaslerGigE" || cameras[0].GetDeviceInfo().GetTLType() == "GEV") {
//            qDebug() << "camera.GevTimestampTickFrequency.GetValue() = ";
//            qDebug() << camera.GevTimestampTickFrequency.GetValue();
            cameraImageEventHandler->setTickFreq(cameras[0].GevTimestampTickFrequency.GetValue());
        }

        // "Just to be sure" checks.Particularly useful for GigE which can get stuck when something illegal is queried
        if(cameras[0].ExposureAuto.IsReadable() && cameras[0].ExposureAuto.GetValue() != ExposureAutoEnums::ExposureAuto_Off && cameras[0].ExposureAuto.IsWritable())
            cameras[0].ExposureAuto.TrySetValue(ExposureAutoEnums::ExposureAuto_Off);
        if(cameras[1].ExposureAuto.IsReadable() && cameras[1].ExposureAuto.GetValue() != ExposureAutoEnums::ExposureAuto_Off && cameras[1].ExposureAuto.IsWritable())
            cameras[1].ExposureAuto.TrySetValue(ExposureAutoEnums::ExposureAuto_Off);
        if(cameras[0].ExposureMode.IsReadable() && cameras[0].ExposureMode.GetValue() != ExposureModeEnums::ExposureMode_Timed && cameras[0].ExposureMode.IsWritable())
            cameras[0].ExposureMode.TrySetValue(ExposureModeEnums::ExposureMode_Timed);
        if(cameras[1].ExposureMode.IsReadable() && cameras[1].ExposureMode.GetValue() != ExposureModeEnums::ExposureMode_Timed && cameras[1].ExposureMode.IsWritable())
            cameras[1].ExposureMode.TrySetValue(ExposureModeEnums::ExposureMode_Timed);

        if(cameras[0].GainAuto.IsReadable() && cameras[0].GainAuto.GetValue() != GainAutoEnums::GainAuto_Off && cameras[0].GainAuto.IsWritable())
            cameras[0].GainAuto.TrySetValue(GainAutoEnums::GainAuto_Off);
        if(cameras[1].GainAuto.IsReadable() && cameras[1].GainAuto.GetValue() != GainAutoEnums::GainAuto_Off && cameras[1].GainAuto.IsWritable())
            cameras[1].GainAuto.TrySetValue(GainAutoEnums::GainAuto_Off);
        if(cameras[0].GainSelector.IsReadable() && cameras[0].GainSelector.IsWritable()) {
            if(cameras[0].GainSelector.GetValue() != GainSelectorEnums::GainSelector_AnalogAll)
                cameras[0].GainSelector.TrySetValue(GainSelectorEnums::GainSelector_AnalogAll);
            else if(cameras[0].GainSelector.GetValue() != GainSelectorEnums::GainSelector_DigitalAll)
                cameras[0].GainSelector.TrySetValue(GainSelectorEnums::GainSelector_DigitalAll);
        }
        if(cameras[1].GainSelector.IsReadable() && cameras[1].GainSelector.IsWritable()) {
            if(cameras[1].GainSelector.GetValue() != GainSelectorEnums::GainSelector_AnalogAll)
                cameras[1].GainSelector.TrySetValue(GainSelectorEnums::GainSelector_AnalogAll);
            else if(cameras[1].GainSelector.GetValue() != GainSelectorEnums::GainSelector_DigitalAll)
                cameras[1].GainSelector.TrySetValue(GainSelectorEnums::GainSelector_DigitalAll);
        }

//        // DEBUG HELP
//        GenApi::INodeMap& nodemap = camera.GetNodeMap();
//        GenApi_3_1_Basler_pylon_v3::NodeList_t Nodes;
//        nodemap.GetNodes(Nodes);
//        for(int i=0; i<Nodes.size(); i++)
//            qInfo() << Nodes[i]->GetName();

        // Safety check for GigE
        // We should be able to read something very basic, e.g. ROI height
        if(!cameras[0].Height.IsReadable() || !cameras[1].Height.IsReadable()) {
            qCritical() << "Device is likely stuck. Resetting...";
            safelyCloseCameras();

            emit manualDeviceResetNecessary();
        }

        // Synchronize the camera time to the system time
        synchronizeTime();
        
        cameraImageEventHandler->setTimeSynchronization(cameraMainTime, cameraSecondaryTime, systemTime);

        cameras[0].PixelFormat.TrySetValue(PixelFormat_Mono8);
        cameras[1].PixelFormat.TrySetValue(PixelFormat_Mono8);
        enableSensorLevelBinningIfPossible();

        // Load calibration if existing
        if(!cameraCalibration->isCalibrated()) {
            // If we already used this camera before, a config file may exists
            loadCalibrationFile();
        }

        CIntegerParameter heartbeat0( cameras[0].GetTLNodeMap(), "HeartbeatTimeout" );
        CIntegerParameter heartbeat1( cameras[1].GetTLNodeMap(), "HeartbeatTimeout" );
        heartbeat0.TrySetValue( 1000, IntegerValueCorrection_Nearest );
        heartbeat1.TrySetValue( 1000, IntegerValueCorrection_Nearest );

        // TODO: Revise if this check is needed at ll! If really needed, make it hang on a
        //  thread, or whatever, but do not obstruct grabbing if not needed to
        //if (cameras[0].CanWaitForFrameTriggerReady() && cameras[1].CanWaitForFrameTriggerReady()) {
            startGrabbing();
        //} else {
        //    qDebug() << "Cameras can not be queried whether it is ready to accept the next frame trigger.";
        //}

        // Rewrite these properties in the cameras in order to be able to read ResultingFramerate later
        if( cameras.GetSize()>0 &&
            cameras[0].AcquisitionFrameRateEnable.IsReadable() &&
            cameras[0].AcquisitionFrameRateEnable.IsWritable() &&
            cameras[0].AcquisitionFrameRateEnable.GetValue() ) {

            cameras[0].AcquisitionFrameRateEnable.TrySetValue(false);
        }
        if( cameras.GetSize()>1 &&
            cameras[1].AcquisitionFrameRateEnable.IsReadable() &&
            cameras[1].AcquisitionFrameRateEnable.IsWritable() &&
            cameras[1].AcquisitionFrameRateEnable.GetValue() ) {

            cameras[1].AcquisitionFrameRateEnable.TrySetValue(false);
        }

    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

// Synchronize the camera to system time
// At the given point in time, both camera and system time are recorded, which is then used to convert between them for later image timestamps
void StereoCamera::synchronizeTime() {

    if(cameras.GetSize() < 2) {
        std::cerr << "StereoCamera: must have two cameras connected."<< std::endl;
        return;
    }

    if (!isEmulated()){
        cameras[0].TimestampLatch.Execute();
        cameras[1].TimestampLatch.Execute();
    }
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> epoche = std::chrono::time_point<std::chrono::system_clock>{};


    if (!isEmulated()){
        cameraMainTime = static_cast<uint64>(cameras[0].TimestampLatchValue.GetValue());
        cameraSecondaryTime = static_cast<uint64>(cameras[1].TimestampLatchValue.GetValue());
    }
    else {
        cameraMainTime = static_cast<uint64>(start.time_since_epoch().count());
        cameraSecondaryTime = static_cast<uint64>(start.time_since_epoch().count());
    }
    

    systemTime  = std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count();
    std::time_t startTime = std::chrono::system_clock::to_time_t(start);
    std::time_t epochTime = std::chrono::system_clock::to_time_t(epoche);

    qInfo() << "Camera Synchronize Time";
    qInfo() << "=========================";
    qInfo() << "Timestamp Camera Main: " << cameraMainTime;
    qInfo() << "Timestamp Camera Secondary: " << cameraSecondaryTime;
    qInfo() << "Timestamp System: " << systemTime;
    qInfo() << "System Epoch: " << std::ctime(&epochTime);
    qInfo() << "System Time: " << std::ctime(&startTime);
    qInfo() << "Time from Epoch (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    qInfo() << "Time from Epoch (us): " << std::chrono::duration_cast<std::chrono::microseconds>(start.time_since_epoch()).count();
    qInfo() << "Time from Epoch (ns): " << std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count();
    qInfo() << "=========================";
}


bool StereoCamera::isOpen() {
    return cameras.IsOpen();
}

// Close the stereo camera and release all Pylon resources
void StereoCamera::close() {

    qDebug() << "StereoCamera: Releasing pylon resources.";
    cameras.StopGrabbing();

//    for(int i = 0; i<cameras.GetSize(); i++) {
//        cameras[i].DeregisterImageEventHandler(cameraImageEventHandler);
//    }

//    cameras.Close();
    safelyCloseCameras();
}

// Current exposure time value of the main camera
int StereoCamera::getExposureTimeValue() {
    try {
        if (cameras.GetSize() > 0 && cameras[0].ExposureTime.IsReadable()) {
            return cameras[0].ExposureTime.GetValue();
        } else if (cameras.GetSize() > 0 && cameras[0].ExposureTimeAbs.IsReadable()) {
            return cameras[0].ExposureTimeAbs.GetValue();
        } else if (cameras.GetSize() > 0 && cameras[0].ExposureTimeRaw.IsReadable()) {
            return cameras[0].ExposureTimeRaw.GetValue();
        } else if (cameras.GetSize() > 0 && cameras[0].BslEffectiveExposureTime.IsReadable()) {
            return cameras[0].BslEffectiveExposureTime.GetValue();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// Minimal possible exposure time value of the main camera
int StereoCamera::getExposureTimeMin() {
    try {
        if (cameras.GetSize() > 0 && cameras[0].ExposureTime.IsReadable()) {
            return cameras[0].ExposureTime.GetMin();
        } else if (cameras.GetSize() > 0 && cameras[0].ExposureTimeAbs.IsReadable()) {
            return cameras[0].ExposureTimeAbs.GetMin();
        } if (cameras.GetSize() > 0 && cameras[0].ExposureTimeRaw.IsReadable()) {
            return cameras[0].ExposureTimeRaw.GetMin();
        } else if (cameras.GetSize() > 0 && cameras[0].BslEffectiveExposureTime.IsReadable()) {
            return cameras[0].BslEffectiveExposureTime.GetMin();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// Maximal possible exposure time value of the main camera
int StereoCamera::getExposureTimeMax() {
    try {
        if (cameras.GetSize() > 0 && cameras[0].ExposureTime.IsReadable()) {
            return cameras[0].ExposureTime.GetMax();
        } else if (cameras.GetSize() > 0 && cameras[0].ExposureTimeAbs.IsReadable()) {
            return cameras[0].ExposureTimeAbs.GetMax();
        } else if (cameras.GetSize() > 0 && cameras[0].ExposureTimeRaw.IsReadable()) {
            return cameras[0].ExposureTimeRaw.GetMax();
        } else if (cameras.GetSize() > 0 && cameras[0].BslEffectiveExposureTime.IsReadable()) {
            return cameras[0].BslEffectiveExposureTime.GetMax();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// Current Gain value of the main camera
double StereoCamera::getGainValue() {
    try {
        if (cameras.GetSize() > 0 && cameras[0].Gain.IsReadable()) {
            return cameras[0].Gain.GetValue();
        } else if (cameras.GetSize() > 0 && cameras[0].GainAbs.IsReadable()) {
            return cameras[0].GainAbs.GetValue();
        } else if (cameras.GetSize() > 0 && cameras[0].GainRaw.IsReadable()) {
            return cameras[0].GainRaw.GetValue();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// Minimal possible Gain value of the main camera
double StereoCamera::getGainMin() {
    try {
        if (cameras.GetSize() > 0 && cameras[0].Gain.IsReadable()) {
            return cameras[0].Gain.GetMin();
        } else if (cameras.GetSize() > 0 && cameras[0].GainAbs.IsReadable()) {
            return cameras[0].GainAbs.GetMin();
        } else if (cameras.GetSize() > 0 && cameras[0].GainRaw.IsReadable()) {
            return cameras[0].GainRaw.GetMin();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// Maximal possible Gain value of the main camera
double StereoCamera::getGainMax() {
    try {
        if (cameras.GetSize() > 0 && cameras[0].Gain.IsReadable()) {
            return cameras[0].Gain.GetMax();
        } else if (cameras.GetSize() > 0 && cameras[0].GainAbs.IsReadable()) {
            return cameras[0].GainAbs.GetMax();
        } else if (cameras.GetSize() > 0 && cameras[0].GainRaw.IsReadable()) {
            return cameras[0].GainRaw.GetMax();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// Sets the Gain value of the main and secondary camera
void StereoCamera::setGainValue(double value) {
    try {
        if(cameras.GetSize() != 2)
            return;

        if (    (cameras[0].Gain.IsReadable() && cameras[1].Gain.IsReadable()) ||
                (cameras[0].GainAbs.IsReadable() && cameras[1].GainAbs.IsReadable()) ||
                (cameras[0].GainRaw.IsReadable() && cameras[1].GainRaw.IsReadable())
                )  {
            if (getGainMax() < value)
                value = getGainMax();
            else if (getGainMin() > value)
                value = getGainMin();
        }

        // TODO: do this properly, and add a GUI tickbox for Continous auto vs Auto once and the spinbox.
        //  Also correct Aravis implementation for this
        GenApi_3_1_Basler_pylon_v3::INodeMap& nodemap0 = cameras[0].GetNodeMap();
        //CEnumParameter(nodemap, "ExposureAuto").TrySetValue("Continuous");
        CEnumParameter(nodemap0, "GainAuto").TrySetValue("Off");
        GenApi_3_1_Basler_pylon_v3::INodeMap& nodemap1 = cameras[1].GetNodeMap();
        //CEnumParameter(nodemap, "ExposureAuto").TrySetValue("Continuous");
        CEnumParameter(nodemap1, "GainAuto").TrySetValue("Off");

        if (cameras[0].Gain.IsWritable() && cameras[1].Gain.IsWritable()) {
            cameras[0].Gain.TrySetValue(value);
            cameras[1].Gain.TrySetValue(value);
        } else if (cameras[0].GainAbs.IsWritable() && cameras[1].GainAbs.IsWritable()) {
            cameras[0].GainAbs.TrySetValue(value);
            cameras[1].GainAbs.TrySetValue(value);
        } else if (cameras[0].GainRaw.IsWritable() && cameras[1].GainRaw.IsWritable()) {
            cameras[0].GainRaw.TrySetValue(value);
            cameras[1].GainRaw.TrySetValue(value);
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }

}

// Sets the exposure time value of the main and secondary camera
void StereoCamera::setExposureTimeValue(int value) {
    try {
        if(cameras.GetSize() != 2)
            return;

        if (    (cameras[0].ExposureTime.IsReadable() && cameras[1].ExposureTime.IsReadable()) ||
                (cameras[0].ExposureTimeAbs.IsReadable() && cameras[1].ExposureTimeAbs.IsReadable()) ||
                (cameras[0].ExposureTimeRaw.IsReadable() && cameras[1].ExposureTimeRaw.IsReadable()) ||
                (cameras[0].BslEffectiveExposureTime.IsReadable() && cameras[1].BslEffectiveExposureTime.IsReadable())
                ) {
            if (getExposureTimeMax() < value)
                value = getExposureTimeMax();
            else if (getExposureTimeMin() > value)
                value = getExposureTimeMin();
        }

        // TODO: do this properly, and add a GUI tickbox for Continous auto vs Auto once and the spinbox.
        //  Also correct Aravis implementation for this
        GenApi_3_1_Basler_pylon_v3::INodeMap& nodemap0 = cameras[0].GetNodeMap();
        //CEnumParameter(nodemap, "ExposureAuto").TrySetValue("Continuous");
        CEnumParameter(nodemap0, "ExposureAuto").TrySetValue("Off");
        GenApi_3_1_Basler_pylon_v3::INodeMap& nodemap1 = cameras[1].GetNodeMap();
        //CEnumParameter(nodemap, "ExposureAuto").TrySetValue("Continuous");
        CEnumParameter(nodemap1, "ExposureAuto").TrySetValue("Off");

        if (cameras[0].ExposureTime.IsWritable() && cameras[1].ExposureTime.IsWritable()) {
            cameras[0].ExposureTime.TrySetValue(value);
            cameras[1].ExposureTime.TrySetValue(value);
        } else if (cameras[0].ExposureTimeAbs.IsWritable() && cameras[1].ExposureTimeAbs.IsWritable()) {
            cameras[0].ExposureTimeAbs.TrySetValue(value);
            cameras[1].ExposureTimeAbs.TrySetValue(value);
        } else if (cameras[0].ExposureTimeRaw.IsWritable() && cameras[1].ExposureTimeRaw.IsWritable()) {
            cameras[0].ExposureTimeRaw.TrySetValue(value);
            cameras[1].ExposureTimeRaw.TrySetValue(value);
        } else if (cameras[0].BslEffectiveExposureTime.IsWritable() && cameras[1].BslEffectiveExposureTime.IsWritable()) {
            cameras[0].BslEffectiveExposureTime.TrySetValue(value);
            cameras[1].BslEffectiveExposureTime.TrySetValue(value);
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

// Loads the camera settings of the main camera and sets its values to both the main and secondary camera
// Reads the Basler specific camera setting file format
// Camera settings are automatically saved in the applications settings directory (Path visible in the about window)
void StereoCamera::loadMainFromFile(const QString &filename) {

    bool wasOpen = cameras.IsOpen();
    if(!wasOpen) {
        cameras.Open();
    }

    try {
        CFeaturePersistence::Load(filename.toStdString().c_str(), &cameras[0].GetNodeMap(), true);
    } catch (const GenericException &e) {
        // Error handling.
        std::cerr << "An exception occurred: " << e.GetDescription() << std::endl;
    }

    // Set the main and second camera with same settings
    setExposureTimeValue(cameras[0].ExposureTime.GetValue());
    setGainValue(cameras[0].Gain.GetValue());

    if(isEnabledAcquisitionFrameRate()) {
        setAcquisitionFPSValue(cameras[0].AcquisitionFrameRate.GetValue());
    }
    if(!wasOpen) {
        cameras.Close();
//        safelyCloseCameras();
    }
}

// Saves the main camera settings to file
// Uses the Basler specific file format
void StereoCamera::saveMainToFile(const QString &filename) {
    try {
        CFeaturePersistence::Save(filename.toStdString().c_str(), &cameras[0].GetNodeMap());
    } catch (const GenericException &e) {
        // Error handling.
        std::cerr << "An exception occurred: " << e.GetDescription() << std::endl;
    }
}

// Reads if a image acquisition frame rate is enabled
// The value of the image acquisition may overwrite hardware trigger framerates
// Assumes that main and secondary camera have the same settings
bool StereoCamera::isEnabledAcquisitionFrameRate() {
    try {
        if (isAcquisitionFrameRateAvailable()) {
            return cameras[0].AcquisitionFrameRateEnable.GetValue();
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return false;
}

bool StereoCamera::isAcquisitionFrameRateAvailable() {
    try {
        // TODO: IF THE CAMERA TYPE IS ON WHITELIST, FORCEFULLY RETURN TRUE
        //  e.g. daA1280-54um returns false properly, but still it works surprisingly if we forcefully set
        //  aquisition framerate. So a whitelist could be used, and that camera added to it at least

        if (cameras.GetSize() > 0 && cameras[0].AcquisitionFrameRateEnable.IsReadable() && cameras[0].AcquisitionFrameRateEnable.IsWritable()) {
            return true;
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return false;
}

bool StereoCamera::isEmulated()
{
    if (cameras.GetSize() == 2){
//        String_t device1_name = cameras[0].GetDeviceInfo().GetModelName();
//        String_t device2_name = cameras[1].GetDeviceInfo().GetModelName();
        QString device1_name = QString(cameras[0].GetDeviceInfo().GetModelName().c_str());
        QString device2_name = QString(cameras[1].GetDeviceInfo().GetModelName().c_str());
//        qDebug() << QString(device1_name) << QString(device2_name);
        return (device1_name.toLower().contains("emu") || device2_name.toLower().contains("emu"));
//        return ((device1_name.find("Emu") != String_t::npos) || (device2_name.find("Emu") != String_t::npos));
        // TODO: make this "is emulated" friendly name check coherent with the one used in mainwindow.cpp
    }
    else return false;
    
}

// Enables image acquisition frame rate for both cameras
void StereoCamera::enableAcquisitionFrameRate(bool enabled) {
    try {
        if(cameras.GetSize() < 2) {
            return;
        }

        if (cameras[0].AcquisitionFrameRateEnable.IsWritable() && cameras[1].AcquisitionFrameRateEnable.IsWritable()) {
            cameras[0].AcquisitionFrameRateEnable.TrySetValue(enabled);
            cameras[1].AcquisitionFrameRateEnable.TrySetValue(enabled);
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

// Sets the value of the image acquisition frame rate for both cameras
void StereoCamera::setAcquisitionFPSValue(int value) {
    try {
        if(cameras.GetSize() < 2) {
            return;
        }

        if (cameras[0].AcquisitionFrameRateAbs.IsWritable() && cameras[1].AcquisitionFrameRateAbs.IsWritable()) {
            cameras[0].AcquisitionFrameRateAbs.TrySetValue(value);
            cameras[1].AcquisitionFrameRateAbs.TrySetValue(value);
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    
}

// Reads and returns the value of the image acquisition frame rate for the main camera
// Assumes that both cameras are set the same through the above function setAcquisitionFPSValue
int StereoCamera::getAcquisitionFPSValue() {
    try {
        if (cameras.GetSize() > 0 && cameras[0].AcquisitionFrameRate.IsReadable()) {
            return cameras[0].AcquisitionFrameRate.GetValue();
        } else if (cameras.GetSize() > 0 && cameras[0].AcquisitionFrameRateAbs.IsReadable()) {
            return cameras[0].AcquisitionFrameRateAbs.GetValue();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// Minimal possible image acquisition frame rate of the main camera
int StereoCamera::getAcquisitionFPSMin() {
    try {
        if (cameras.GetSize() > 0 && cameras[0].AcquisitionFrameRate.IsReadable()) {
            return cameras[0].AcquisitionFrameRate.GetMin();
        } else if (cameras.GetSize() > 0 && cameras[0].AcquisitionFrameRateAbs.IsReadable()) {
            return cameras[0].AcquisitionFrameRateAbs.GetMin();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// Maximal possible image acquisition frame rate of the main camera
// This may be influenced by the current camera settings and its value may change
int StereoCamera::getAcquisitionFPSMax() {
    try {
        if (cameras.GetSize() > 0 && cameras[0].AcquisitionFrameRate.IsReadable()) {
            return cameras[0].AcquisitionFrameRate.GetMax();
        } else if (cameras.GetSize() > 0 && cameras[0].AcquisitionFrameRateAbs.IsReadable()) {
            return cameras[0].AcquisitionFrameRateAbs.GetMax();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// Camera frame rate resulting by the current camera settings
double StereoCamera::getResultingFrameRateValue() {
    try {
        if (cameras.GetSize() > 0 && cameras[0].ResultingFrameRate.IsReadable()) {
            return cameras[0].ResultingFrameRate.GetValue();
        } else if (cameras.GetSize() > 0 && cameras[0].ResultingFrameRateAbs.IsReadable()) {
            return cameras[0].ResultingFrameRateAbs.GetValue();
        } else if (cameras.GetSize() > 0 && cameras[0].BslResultingAcquisitionFrameRate.IsReadable()) {
            return cameras[0].BslResultingAcquisitionFrameRate.GetValue();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

StereoCameraCalibration *StereoCamera::getCameraCalibration() {
    return cameraCalibration;
}

// Performs automatically setting of the Gain value based on the current camera image
// Main camera is used to automatically find a Gain value, this value is then applied to the secondary camera
void StereoCamera::autoGainOnce() {

    if(cameras.GetSize() < 2) {
        return;
    }

    // We set Gain value of both cameras by first auto Gain once for the main camera and then set the resulting value for the second
    try {

        if(!cameras.IsOpen()) {
            cameras.Open();
        }

        cameras.StopGrabbing();

        // Turn test image off.
        cameras[0].TestImageSelector.TrySetValue(TestImageSelector_Off);
        cameras[0].TestPattern.TrySetValue(TestPattern_Off);

        // Only area scan cameras support auto functions.
        if (cameras[0].DeviceScanType.GetValue() == DeviceScanType_Areascan) {
            // Cameras based on SFNC 2.0 or later, e.g., USB cameras
            if (cameras[0].GetSfncVersion() >= Sfnc_2_0_0) {
                // All area scan cameras support luminance control.
                // Carry out luminance control by using the "once" gain auto function.
                // For demonstration purposes only, set the gain to an initial value. TODO

                qDebug() << "Starting AutoGain...";

                //camera.Gain.SetToMaximum();
                cameras[0].Gain.TrySetToMaximum();


                if (!cameras[0].GainAuto.IsWritable()) {
                    qDebug() << "The camera does not support Gain Auto.";
                    return;
                }

                // Maximize the grabbed image area of interest (Image AOI).
                cameras[0].OffsetX.TrySetToMinimum();
                cameras[0].OffsetY.TrySetToMinimum();
                cameras[0].Width.TrySetToMaximum();
                cameras[0].Height.TrySetToMaximum();

                if (cameras[0].AutoFunctionROISelector.IsWritable()) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    // Set the Auto Function ROI for luminance statistics.
                    // We want to use ROI1 for gathering the statistics

                    cameras[0].AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI1);
                    cameras[0].AutoFunctionROIUseBrightness.TrySetValue(true);   // ROI 1 is used for brightness control
                    cameras[0].AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI2);
                    cameras[0].AutoFunctionROIUseBrightness.TrySetValue(false);   // ROI 2 is not used for brightness control

                    // Set the ROI (in this example the complete sensor is used)
                    cameras[0].AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI1);  // configure ROI 1
                    cameras[0].AutoFunctionROIOffsetX.TrySetValue(cameras[0].OffsetX.GetMin());
                    cameras[0].AutoFunctionROIOffsetY.TrySetValue(cameras[0].OffsetY.GetMin());
                    cameras[0].AutoFunctionROIWidth.TrySetValue(cameras[0].Width.GetMax());
                    cameras[0].AutoFunctionROIHeight.TrySetValue(cameras[0].Height.GetMax());
                }

                if (cameras[0].GetSfncVersion() >= Sfnc_2_0_0) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    // Set the target value for luminance control.
                    // A value of 0.3 means that the target brightness is 30 % of the maximum brightness of the raw pixel value read out from the sensor.
                    // A value of 0.4 means 40 % and so forth.
                    cameras[0].AutoTargetBrightness.TrySetValue(0.3);

                    // We are going to try GainAuto = Once.

                    qDebug() << "Trying 'GainAuto = Once'.";
                    qDebug() << "Initial Gain = " << cameras[0].Gain.GetValue();

                    // Set the gain ranges for luminance control.
                    cameras[0].AutoGainLowerLimit.TrySetValue(cameras[0].Gain.GetMin());
                    cameras[0].AutoGainUpperLimit.TrySetValue(cameras[0].Gain.GetMax());
                }

                cameras[0].GainAuto.TrySetValue(GainAuto_Once);

                // When the "once" mode of operation is selected,
                // the parameter values are automatically adjusted until the related image property
                // reaches the target value. After the automatic parameter value adjustment is complete, the auto
                // function will automatically be set to "off" and the new parameter value will be applied to the
                // subsequently grabbed images.

                int n = 0;
                while (cameras[0].GainAuto.GetValue() != GainAuto_Off) {
                    CBaslerUniversalGrabResultPtr ptrGrabResult;
                    cameras[0].GrabOne( 5000, ptrGrabResult);
                    ++n;
                    //Make sure the loop is exited.
                    if (n > 100) {
                        throw TIMEOUT_EXCEPTION( "The adjustment of auto gain did not finish.");
                    }
                }

                qDebug() << "GainAuto went back to 'Off' after " << n << " frames.";
                if(cameras[0].Gain.IsReadable()) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    qDebug() << "Final Gain = " << cameras[0].Gain.GetValue();
                }

                // set Gain value for second camera
                cameras[1].Gain.TrySetValue(cameras[0].Gain.GetValue());

                startGrabbing();
            }
        } else {
            std::cerr << "Only area scan cameras support auto functions." << std::endl;
        }
    } catch (const TimeoutException &e) {
        // Auto functions did not finish in time.
        // Maybe the cap on the lens is still on or there is not enough light.
        std::cerr << "A timeout has occurred: " << std::endl << e.GetDescription() << std::endl;
        std::cerr << "Please make sure you remove the cap from the camera lens before running auto gain." << std::endl;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

// Performs automatically setting of the exposure time value based on the current main camera image
// Main camera is used to automatically find a exposure time, this value is then applied to the secondary camera
void StereoCamera::autoExposureOnce() {

    if(cameras.GetSize() < 2) {
        return;
    }

    // We set Exposure value of both cameras by first auto Exposure once for the main camera and then set the resulting value for the second
    try {

        if(!cameras.IsOpen()) {
            cameras.Open();
        }

        cameras.StopGrabbing();

        // Turn test image off.
        cameras[0].TestImageSelector.TrySetValue(TestImageSelector_Off);
        cameras[0].TestPattern.TrySetValue(TestPattern_Off);

        // Only area scan cameras support auto functions.
        if (cameras[0].DeviceScanType.GetValue() == DeviceScanType_Areascan) {
            // Cameras based on SFNC 2.0 or later, e.g., USB cameras
            if (cameras[0].GetSfncVersion() >= Sfnc_2_0_0) {
                // For demonstration purposes only, set the exposure time to an initial value.
                cameras[0].ExposureTime.TrySetToMinimum();
                // Carry out luminance control by using the "once" exposure auto function.


                if (!cameras[0].ExposureAuto.IsWritable())
                {
                    qDebug() << "The camera does not support Exposure Auto.";
                    return;
                }

                // Maximize the grabbed area of interest (Image AOI).
                cameras[0].OffsetX.TrySetToMinimum();
                cameras[0].OffsetY.TrySetToMinimum();
                cameras[0].Width.TrySetToMaximum();
                cameras[0].Height.TrySetToMaximum();

                if (cameras[0].AutoFunctionROISelector.IsWritable())
                {
                    // Set the Auto Function ROI for luminance statistics.
                    // We want to use ROI1 for gathering the statistics
                    cameras[0].AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI1);
                    cameras[0].AutoFunctionROIUseBrightness.TrySetValue(true);   // ROI 1 is used for brightness control
                    cameras[0].AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI2);
                    cameras[0].AutoFunctionROIUseBrightness.TrySetValue(false);   // ROI 2 is not used for brightness control

                    // Set the ROI (in this example the complete sensor is used)
                    cameras[0].AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI1);  // configure ROI 1
                    cameras[0].AutoFunctionROIOffsetX.TrySetValue(cameras[0].OffsetX.GetMin());
                    cameras[0].AutoFunctionROIOffsetY.TrySetValue(cameras[0].OffsetY.GetMin());
                    cameras[0].AutoFunctionROIWidth.TrySetValue(cameras[0].Width.GetMax());
                    cameras[0].AutoFunctionROIHeight.TrySetValue(cameras[0].Height.GetMax());
                }

                if (cameras[0].GetSfncVersion() >= Sfnc_2_0_0) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    // Set the target value for luminance control.
                    // A value of 0.3 means that the target brightness is 30 % of the maximum brightness of the raw pixel value read out from the sensor.
                    // A value of 0.4 means 40 % and so forth.
                    cameras[0].AutoTargetBrightness.TrySetValue(0.3);

                    // Try ExposureAuto = Once.
                    qDebug() << "Trying 'ExposureAuto = Once'.";
                    qDebug() << "Initial exposure time = ";
                    qDebug() << cameras[0].ExposureTime.GetValue() << " us";

                    // Set the exposure time ranges for luminance control.
                    cameras[0].AutoExposureTimeLowerLimit.TrySetValue(cameras[0].AutoExposureTimeLowerLimit.GetMin());
                    cameras[0].AutoExposureTimeUpperLimit.TrySetValue(cameras[0].AutoExposureTimeLowerLimit.GetMax());

                    cameras[0].ExposureAuto.TrySetValue(ExposureAuto_Once);
                }

                // When the "once" mode of operation is selected,
                // the parameter values are automatically adjusted until the related image property
                // reaches the target value. After the automatic parameter value adjustment is complete, the auto
                // function will automatically be set to "off", and the new parameter value will be applied to the
                // subsequently grabbed images.
                int n = 0;
                while (cameras[0].ExposureAuto.GetValue() != ExposureAuto_Off)
                {
                    CBaslerUniversalGrabResultPtr ptrGrabResult;
                    cameras[0].GrabOne(5000, ptrGrabResult);
                    ++n;

                    //Make sure the loop is exited.
                    if (n > 100) {
                        throw TIMEOUT_EXCEPTION( "The adjustment of auto exposure did not finish.");
                    }
                }

                qDebug() << "ExposureAuto went back to 'Off' after " << n << " frames.";
                qDebug() << "Final exposure time = ";

                if (cameras[0].ExposureTime.IsReadable()) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    qDebug() << cameras[0].ExposureTime.GetValue() << " us";
                }

                // Set value of second camera
                cameras[1].ExposureTime.TrySetValue(cameras[0].ExposureTime.GetValue());

                startGrabbing();
            }
        } else {
            std::cerr << "Only area scan cameras support auto functions." << std::endl;
        }
    } catch (const TimeoutException &e) {
        // Auto functions did not finish in time.
        // Maybe the cap on the lens is still on or there is not enough light.
        std::cerr << "A timeout has occurred: " << e.GetDescription() << std::endl;
        std::cerr << "Please make sure you remove the cap from the camera lens before running this sample." << std::endl;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

// The current used linesource as the hardware trigger source
// The same linesource is used for both cameras
// When the linesource changes, the camera must be closed and opened again
QString StereoCamera::getLineSource() {
    return lineSource;
}

// Sets the linesource used as the hardware trigger source
// The same linesource is used for both cameras
// When the linesource changes, the camera must be closed and opened again
void StereoCamera::setLineSource(QString value) {
    lineSource = value;
}

CameraImageType StereoCamera::getType() {
    return CameraImageType::LIVE_STEREO_CAMERA;
}

void StereoCamera::startGrabbing() {
    if (cameras.IsOpen() && !cameras.IsGrabbing()) {
        cameras.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
    }
    qDebug() << "Grabbing started";
}

void StereoCamera::stopGrabbing() {
    if (cameras.IsOpen() && cameras.IsGrabbing()) {
        cameras.StopGrabbing();
    }
    qDebug() << "Grabbing stopped";
}

// Returns a list of the friendly device names of the connected cameras
std::vector<QString> StereoCamera::getFriendlyNames() {
    std::vector<QString> names;

    for(int i = 0; i<cameras.GetSize(); i++) {
        names.push_back(QString(cameras[i].GetDeviceInfo().GetFriendlyName()));
    }

    return names;
}

// Returns the calibration filename describing a calibration setting consisting of the two cameras and the configured calibration pattern
QString StereoCamera::getCalibrationFilename() {
    if(cameras.GetSize() == 2) {
        return settingsDirectory.filePath(
                getFriendlyNames()[0] + "_" + getFriendlyNames()[1] + "_stereo_calibration_" +
                QString::number(cameraCalibration->getPattern()) + "_" +
                QString::number(cameraCalibration->getSquareSize()) + "_" +
                QString::number(cameraCalibration->getBoardSize().width + 1) + "x" +
                QString::number(cameraCalibration->getBoardSize().height + 1) + ".xml");
    }

    return QString();
}

// Loads the calibration file defined by getCalibrationFilename() if it exists
void StereoCamera::loadCalibrationFile() {
    QString configFile = getCalibrationFilename();
    configFile.replace(" ", "");
    if (QFile::exists(configFile)) {
        qDebug() << "Found calibration file in settings directory. Loading: " << configFile.toStdString();
        cameraCalibration->loadFromFile(configFile.toStdString().c_str());
    }
}

// Synchronize the camera and system time and update the image event handler
void StereoCamera::resynchronizeTime() {
    if(cameras.IsOpen()) {
        std::cout<<"Resynchronizing Camera Time..."<<std::endl;
        synchronizeTime();
        cameraImageEventHandler->setTimeSynchronization(cameraMainTime, cameraSecondaryTime, systemTime);
    }
}


int StereoCamera::getImageROIwidth() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].Width.IsReadable() || !cameras[1].Width.IsReadable()) {
            return 0;
        }

        int val0 = (int)cameras[0].Width.GetValue();
        int val1 = (int)cameras[1].Width.GetValue();
        if(val0 != val1) {
            std::cout<<"Image acquisition ROI width of the two cameras are not the same. Now resetting both to the lower value."<<std::endl;
            int minVal = (val0 < val1) ? val0 : val1;
            cameras[0].Width.TrySetValue(minVal);
            cameras[1].Width.TrySetValue(minVal);
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int StereoCamera::getImageROIheight() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].Height.IsReadable() || !cameras[1].Height.IsReadable()) {
            return 0;
        }

        int val0 = (int) cameras[0].Height.GetValue();
        int val1 = (int) cameras[1].Height.GetValue();
        if (val0 != val1) {
            qDebug() << "Image acquisition ROI height of the two cameras are not the same. Now resetting both to the lower value.";
            int minVal = (val0 < val1) ? val0 : val1;
            cameras[0].Height.TrySetValue(minVal);
            cameras[1].Height.TrySetValue(minVal);
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int StereoCamera::getImageROIoffsetX() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].OffsetX.IsReadable() || !cameras[1].OffsetX.IsReadable()) {
            return 0;
        }

        int val0 = (int) cameras[0].OffsetX.GetValue();
        int val1 = (int) cameras[1].OffsetX.GetValue();
        if (val0 != val1) {
            qDebug() << "Image acquisition ROI offsetX of the two cameras are not the same. Now resetting both to the lower value.";
            int minVal = (val0 < val1) ? val0 : val1;
            cameras[0].OffsetX.TrySetValue(minVal);
            cameras[1].OffsetX.TrySetValue(minVal);
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int StereoCamera::getImageROIoffsetXInc() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].OffsetX.IsReadable() || !cameras[1].OffsetX.IsReadable()) {
            return 0;
        }

        int val0 = (int) cameras[0].OffsetX.GetInc();
        int val1 = (int) cameras[1].OffsetX.GetInc();
        if (val0 != val1) {
            // if not same, use max
            val0 = (val0 > val1) ? val0 : val1;
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int StereoCamera::getImageROIoffsetY() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].OffsetY.IsReadable() || !cameras[1].OffsetY.IsReadable()) {
            return 0;
        }

        int val0 = (int) cameras[0].OffsetY.GetValue();
        int val1 = (int) cameras[1].OffsetY.GetValue();
        if (val0 != val1) {
            qDebug() << "Image acquisition ROI offsetY of the two cameras are not the same. Now resetting both to the lower value.";
            int minVal = (val0 < val1) ? val0 : val1;
            cameras[0].OffsetY.TrySetValue(minVal);
            cameras[1].OffsetY.TrySetValue(minVal);
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int StereoCamera::getImageROIoffsetYInc() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].OffsetY.IsReadable() || !cameras[1].OffsetY.IsReadable()) {
            return 0;
        }

        int val0 = (int) cameras[0].OffsetY.GetInc();
        int val1 = (int) cameras[1].OffsetY.GetInc();
        if (val0 != val1) {
            // if not same, use max
            val0 = (val0 > val1) ? val0 : val1;
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// NOTE: Binning affects this
int StereoCamera::getImageROIwidthMax() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].WidthMax.IsReadable() || !cameras[1].WidthMax.IsReadable()) {
            return 0;
        }

        // Classic/U/L GigE cameras
        //  int val0 = (int)cameras[0].Width.GetMax();
        //  int val1 = (int)cameras[1].Width.GetMax();
        // other cameras
        int val0 = (int) cameras[0].WidthMax.GetValue();
        int val1 = (int) cameras[1].WidthMax.GetValue();
        if (val0 != val1) {
            qDebug() << "Image acquisition ROI max width of the two cameras are not the same. Now using the lower (safer) value.";
            if (val0 > val1)
                val0 = val1;
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int StereoCamera::getImageROIwidthInc() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].Width.IsReadable() || !cameras[1].Width.IsReadable()) {
            return 0;
        }

        int val0 = (int) cameras[0].Width.GetInc();
        int val1 = (int) cameras[1].Width.GetInc();
        if (val0 != val1) {
            qDebug() << "Image acquisition ROI width increment of the two cameras are not the same. Now using the higher (safer) value.";
            if (val0 < val1)
                val0 = val1;
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 16;
}

// NOTE: Binning affects this
int StereoCamera::getImageROIheightMax() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].HeightMax.IsReadable() || !cameras[1].HeightMax.IsReadable()) {
            return 0;
        }

        // Classic/U/L GigE cameras
        //  int val0 = (int)cameras[0].Height.GetMax();
        //  int val1 = (int)cameras[1].Height.GetMax();
        // other cameras
        int val0 = (int) cameras[0].HeightMax.GetValue();
        int val1 = (int) cameras[1].HeightMax.GetValue();
        if (val0 != val1) {
            qDebug() << "Image acquisition ROI max height of the two cameras are not the same. Now using the lower (safer) value.";
            if (val0 > val1)
                val0 = val1;
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int StereoCamera::getImageROIheightInc() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].Height.IsReadable() || !cameras[1].Height.IsReadable()) {
            return 0;
        }

        int val0 = (int) cameras[0].Height.GetInc();
        int val1 = (int) cameras[1].Height.GetInc();
        if (val0 != val1) {
            qDebug() << "Image acquisition ROI height increment of the two cameras are not the same. Now using the higher (safer) value.";
            if (val0 < val1)
                val0 = val1;
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 16;
}

QRectF StereoCamera::getImageROI(){
    return QRectF(getImageROIoffsetX(),getImageROIoffsetY(),getImageROIwidth(), getImageROIheight());
}

bool StereoCamera::isBinningAvailable() {

    bool val = false;

    try {
        bool wasGrabbing = false;
        if(cameras.IsGrabbing()) {
            wasGrabbing = true;
            stopGrabbing();
        }

        // IMPORTANT: there are camera models (e.g. Basler puA1280-54um) where the Pylon API will tell that the
        //  Horizontal Binning levels are minimum 1 and maxumim 2, however the Vertical Binning level is only 1.
        //  This is not an error in the APi, it is because the camera only supports the following binning
        //  combinations (H x V): 1x1, 2x1, 2x2 and the vertical 2 option will only show if the Horizontal
        //  is set to 2 already, as the 1x1 is not supported. So the y axis binning does not have to be checked here.

        if( cameras[0].BinningHorizontal.IsReadable() &&
            cameras[0].BinningHorizontal.IsWritable() &&
            (cameras[0].BinningHorizontal.GetMax() != cameras[0].BinningHorizontal.GetMin()) &&
            cameras[0].BinningVertical.IsReadable() &&
            cameras[0].BinningVertical.IsWritable() &&
            (cameras[0].BinningVertical.GetMax() != cameras[0].BinningVertical.GetMin()) ) {

            val = true;
        }

        if(wasGrabbing) {
            startGrabbing();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }

    return val;
}

int StereoCamera::getBinningVal() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].BinningHorizontal.IsReadable() ||
            !cameras[1].BinningHorizontal.IsReadable()) {
            return 1;
        }

        int b0 = (int) cameras[0].BinningHorizontal.GetValue();
        int b1 = (int) cameras[1].BinningHorizontal.GetValue();
        if (b0 != b1) {
            qDebug() << "Image acquisition horizontal binning of the two cameras are not the same. Now resetting both to the lower value.";
            setBinningVal(b0);
        }
        return b0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 1;
}

int StereoCamera::getBinningMax() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].BinningHorizontal.IsReadable() ||
            !cameras[1].BinningHorizontal.IsReadable()) {
            return 1;
        }

        int b0 = (int) cameras[0].BinningHorizontal.GetMax();
        int b1 = (int) cameras[1].BinningHorizontal.GetMax();
        if (b0 != b1) {
            qDebug() << "Image acquisition horizontal binning maximum of the two cameras are not the same.";
            return 1;
        }
        return b0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 1;
}

bool StereoCamera::isTemperatureReadingSupported() {
    try {
        cameras[0].DeviceTemperatureSelector.TrySetValue(Basler_UniversalCameraParams::DeviceTemperatureSelectorEnums::DeviceTemperatureSelector_Coreboard);
        cameras[1].DeviceTemperatureSelector.TrySetValue(Basler_UniversalCameraParams::DeviceTemperatureSelectorEnums::DeviceTemperatureSelector_Coreboard);

        if( cameras[0].DeviceTemperature.IsReadable() && cameras[0].DeviceTemperature.GetValue() > CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE &&
            cameras[1].DeviceTemperature.IsReadable() && cameras[1].DeviceTemperature.GetValue() > CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE ) {

            return true;
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return false;
}

std::vector<double> StereoCamera::getTemperatures() {
    std::vector<double> temperatures = {CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE, CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE};
    try {
        if (cameras.GetSize() != 2)
            return temperatures;
        if (!cameras[0].DeviceTemperature.IsReadable() || !cameras[1].DeviceTemperature.IsReadable())
            return temperatures;

        // DEV
        //qDebug() << cameras[0].GetValue(Basler_UniversalCameraParams::PLCamera::DeviceModelName);
        //qDebug() << cameras[1].GetValue(Basler_UniversalCameraParams::PLCamera::DeviceModelName);

        // this line is only needed in ace 2, boost, and dart IMX Cameras
        // NOTE: SENSOR TEMP (and maybe others too) CAN NOT BE MEASURED WHILE GRABBING, but coreboard is OK anytime
        cameras[0].DeviceTemperatureSelector.TrySetValue(
                Basler_UniversalCameraParams::DeviceTemperatureSelectorEnums::DeviceTemperatureSelector_Coreboard);
        cameras[1].DeviceTemperatureSelector.TrySetValue(
                Basler_UniversalCameraParams::DeviceTemperatureSelectorEnums::DeviceTemperatureSelector_Coreboard);

        temperatures[0] = cameras[0].DeviceTemperature.GetValue();
        temperatures[1] = cameras[1].DeviceTemperature.GetValue();
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return temperatures;
}

bool StereoCamera::isGrabbing()
{
    return cameras.IsGrabbing();
}

void StereoCamera::enableSensorLevelBinningIfPossible() {

    // Binning***Mode_Average setting is only possible if the BinningSelector setting is not in Sensor mode
    //  and Binning***Mode_Sum only really has a meaning if BinningSelector_Sensor is the case, to compensate
    //  image brightness lost due to smaller area per "pixel", i.e. to retain same brightness even if switched
    //  to binning level 2 or 4 later.
    //  So we set Binning***Mode_Sum only if BinningSelector_Sensor is the case, and to
    //  Binning***Mode_Average if BinningSelector is set otherwise.

    // Try enable sensor level binning, to improve max possible FPS
    if( cameras[0].BinningSelector.IsWritable() && cameras[0].BinningSelector.CanSetValue(BinningSelector_Sensor) &&
        cameras[1].BinningSelector.IsWritable() && cameras[1].BinningSelector.CanSetValue(BinningSelector_Sensor) ) {

        if( cameras[0].BinningHorizontalMode.IsWritable() && cameras[0].BinningHorizontalMode.CanSetValue(BinningHorizontalMode_Sum) &&
            cameras[1].BinningHorizontalMode.IsWritable() && cameras[1].BinningHorizontalMode.CanSetValue(BinningHorizontalMode_Sum) ) {

            cameras[0].BinningHorizontalMode.TrySetValue(BinningHorizontalMode_Sum);
            cameras[1].BinningHorizontalMode.TrySetValue(BinningHorizontalMode_Sum);
        }
        if( cameras[0].BinningVerticalMode.IsWritable() && cameras[0].BinningVerticalMode.CanSetValue(BinningVerticalMode_Sum) &&
            cameras[1].BinningVerticalMode.IsWritable() && cameras[1].BinningVerticalMode.CanSetValue(BinningVerticalMode_Sum) ) {

            cameras[0].BinningVerticalMode.TrySetValue(BinningVerticalMode_Sum);
            cameras[1].BinningVerticalMode.TrySetValue(BinningVerticalMode_Sum);
        }

        cameras[0].BinningSelector.TrySetValue(BinningSelector_Sensor);
        cameras[1].BinningSelector.TrySetValue(BinningSelector_Sensor);
    } else {
        if( cameras[0].BinningHorizontalMode.IsWritable() && cameras[0].BinningHorizontalMode.CanSetValue(BinningHorizontalMode_Average) &&
            cameras[1].BinningHorizontalMode.IsWritable() && cameras[1].BinningHorizontalMode.CanSetValue(BinningHorizontalMode_Average) ) {

            cameras[0].BinningHorizontalMode.TrySetValue(BinningHorizontalMode_Average);
            cameras[1].BinningHorizontalMode.TrySetValue(BinningHorizontalMode_Average);
        }
        if( cameras[0].BinningVerticalMode.IsWritable() && cameras[0].BinningVerticalMode.CanSetValue(BinningVerticalMode_Average) &&
            cameras[1].BinningVerticalMode.IsWritable() && cameras[1].BinningVerticalMode.CanSetValue(BinningVerticalMode_Average) ) {

            cameras[0].BinningVerticalMode.TrySetValue(BinningVerticalMode_Average);
            cameras[1].BinningVerticalMode.TrySetValue(BinningVerticalMode_Average);
        }
    }
}

// NOTE: grabbing "pause" is necessary for setting binning
bool StereoCamera::setBinningVal(int value) {
    if (cameras.GetSize() != 2 || !cameras[0].BinningHorizontal.IsReadable() || !cameras[1].BinningHorizontal.IsReadable()) {
        return false;
    }
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    // IMPORTANT: Horizontal Binning has to be set first

    // in case of our Basler cameras here, only mode=1,2,4 are only valid values
    if(isBinningAvailable()) {

        enableSensorLevelBinningIfPossible();

        if(value==2 || value==3) {
            success = cameras[0].BinningHorizontal.TrySetValue(2) &&
                cameras[0].BinningVertical.TrySetValue(2) &&
                cameras[1].BinningHorizontal.TrySetValue(2) &&
                cameras[1].BinningVertical.TrySetValue(2);
            qDebug() << "Setting binning to 2 on both axes";
        } else if(value==4) {
            success = cameras[0].BinningHorizontal.TrySetValue(4) &&
                cameras[0].BinningVertical.TrySetValue(4) &&
                cameras[1].BinningHorizontal.TrySetValue(4) &&
                cameras[1].BinningVertical.TrySetValue(4);
            qDebug() << "Setting binning to 4 on both axes";
        } else { //if(value==1) {
            success = cameras[0].BinningHorizontal.TrySetValue(1) &&
                cameras[0].BinningVertical.TrySetValue(1) &&
                cameras[1].BinningHorizontal.TrySetValue(1) &&
                cameras[1].BinningVertical.TrySetValue(1);
            qDebug() << "Setting binning to 1 (no binning) on both axes";
        }
    }
    startGrabbing();
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool StereoCamera::setImageROIwidth(int width) {
    if (cameras.GetSize() != 2 || !cameras[0].BinningHorizontal.IsReadable() || !cameras[1].BinningHorizontal.IsReadable()) {
        return false;
    }

    //qDebug() << "Setting both cameras Image ROI width=" << std::to_string(width);
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
    int maxWidth = getImageROIwidthMax();
    int offsetX = getImageROIoffsetX();

    if(width < cameras[0].Width.GetMin())
        width = cameras[0].Width.GetMin();
    int modVal = width % cameras[0].Width.GetInc();
    if(modVal != 0)
        width -= modVal;
    int bestWidth = (offsetX+width > maxWidth) ? maxWidth-offsetX-((maxWidth-offsetX) % cameras[0].Width.GetInc()) : width;
//    int bestWidth = (offsetX >= maxWidth-16) ? 16 : width;

    if (cameras[0].Width.IsWritable() && cameras[1].Width.IsWritable() ) {
        success = cameras[0].Width.TrySetValue(bestWidth) &&
            cameras[1].Width.TrySetValue(bestWidth);
    }
    startGrabbing();
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool StereoCamera::setImageROIheight(int height) {
    if (cameras.GetSize() != 2 || !cameras[0].BinningHorizontal.IsReadable() || !cameras[1].BinningHorizontal.IsReadable()) {
        return false;
    }

    //qDebug() << "Setting both cameras Image ROI height=" << std::to_string(height);
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
    int maxHeight = getImageROIheightMax();
    int offsetY = getImageROIoffsetY();

    if(height < cameras[0].Height.GetMin())
        height = cameras[0].Height.GetMin();
    int modVal = height % cameras[0].Height.GetInc();
    if(modVal != 0)
        height -= modVal;
    int bestHeight = (offsetY+height > maxHeight) ? maxHeight-offsetY-((maxHeight-offsetY) % cameras[0].Height.GetInc()) : height;
//    int bestHeight = (offsetY >= maxHeight-16) ? 16 : height;

    if (cameras[0].Height.IsWritable() && cameras[1].Height.IsWritable() ) {
        success = cameras[0].Height.TrySetValue(bestHeight) &&
            cameras[1].Height.TrySetValue(bestHeight);
    }
    startGrabbing();
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool StereoCamera::setImageROIoffsetX(int offsetX) {
    if (cameras.GetSize() != 2 || !cameras[0].BinningHorizontal.IsReadable() || !cameras[1].BinningHorizontal.IsReadable()) {
        return false;
    }

    //qDebug() << "Setting Image ROI offsetX=" << std::to_string(offsetX);
    bool success = false;

    // NOTE: LIKELY the offsetX and offsetY (unlike height and width) could even be set on most cameras during
    //  grabbing as well, but it is not guaranteed. So yet we simply use the safest solution, stop, then change, then
    //  (re)start grabbing. But if you really want to play around with this to achieve a sliding window ROI or whatever
    //  for highspeed eye detection (the way SMI likely does this anyway), feel free to try. Expo timing could fail btw
    if(cameras.IsGrabbing())
        stopGrabbing();

    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
    int maxWidth = getImageROIwidthMax();
    int width = getImageROIwidth();

    if(maxWidth - offsetX < cameras[0].OffsetX.GetInc())
        offsetX = maxWidth - cameras[0].OffsetX.GetInc();
    int modVal = offsetX % cameras[0].OffsetX.GetInc();
    if(modVal != 0)
        offsetX -= modVal;

    if (width + offsetX <= maxWidth && cameras[0].OffsetX.IsWritable() && cameras[1].OffsetX.IsWritable() ) {
        success = cameras[0].OffsetX.TrySetValue(offsetX) &&
            cameras[1].OffsetX.TrySetValue(offsetX);
    }
    startGrabbing();
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool StereoCamera::setImageROIoffsetY(int offsetY) {
    if (cameras.GetSize() != 2 || !cameras[0].BinningHorizontal.IsReadable() || !cameras[1].BinningHorizontal.IsReadable()) {
        return false;
    }
    
    //qDebug() << "Setting Image ROI offsetY=" << std::to_string(offsetY);
    bool success = false;

    // NOTE: LIKELY the offsetX and offsetY (unlike height and width) could even be set on most cameras during
    //  grabbing as well, but it is not guaranteed. So yet we simply use the safest solution, stop, then change, then
    //  (re)start grabbing. But if you really want to play around with this to achieve a sliding window ROI or whatever
    //  for highspeed eye detection (the way SMI likely does this anyway), feel free to try. Expo timing could fail btw
    if(cameras.IsGrabbing())
        stopGrabbing();

    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
    int maxHeight = getImageROIheightMax();;
    int height = getImageROIheight();;

    if(maxHeight - offsetY < cameras[0].OffsetY.GetInc())
        offsetY = maxHeight - cameras[0].OffsetY.GetInc();
    int modVal = offsetY % cameras[0].OffsetY.GetInc();
    if(modVal != 0)
        offsetY -= modVal;

    if (height + offsetY <= maxHeight && cameras[0].OffsetY.IsWritable() && cameras[1].OffsetY.IsWritable() ) {
        success = cameras[0].OffsetY.TrySetValue(offsetY) &&
            cameras[1].OffsetY.TrySetValue(offsetY);
    }
    startGrabbing();
    return success;
}

//bool StereoCamera::setImageROIwidthEmu(int width) {
//    if (cameras.GetSize() != 2) {
//        return false;
//    }
//
//    //qDebug() << "Setting both cameras Image ROI width=" << std::to_string(width);
//    bool success = false;
//
//    if(cameras.IsGrabbing())
//        stopGrabbing();
//
//    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
//    int maxWidth = getImageROIwidthMax();
//    int offsetX = getImageROIoffsetX();
//
//    // NOTE: here .GetInc() are not used.. but could be?
//    if(width < 16)
//        width=16;
//    int modVal=width%16;
//    if(modVal != 0)
//        width -= modVal;
//    int bestWidth = (offsetX+width > maxWidth) ? maxWidth-offsetX-((maxWidth-offsetX)%16) : width;
////    int bestWidth = (offsetX >= maxWidth-16) ? 16 : width;
//
//    if (cameras[0].Width.IsWritable() && cameras[1].Width.IsWritable() ) {
//        success = cameras[0].Width.TrySetValue(bestWidth) &&
//            cameras[1].Width.TrySetValue(bestWidth);
//    }
//    startGrabbing();
//    return success;
//}
//
//// NOTE: grabbing "pause" is necessary for setting image ROI
//bool StereoCamera::setImageROIheightEmu(int height) {
//    if (cameras.GetSize() != 2) {
//        return false;
//    }
//
//    //qDebug() << "Setting both cameras Image ROI height=" << std::to_string(height);
//    bool success = false;
//
//    if(cameras.IsGrabbing())
//        stopGrabbing();
//
//    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
//    int maxHeight = getImageROIheightMax();
//    int offsetY = getImageROIoffsetY();
//
//    // NOTE: here .GetInc() are not used.. but could be?
//    if(height < 16)
//        height=16;
//    int modVal=height%16;
//    if(modVal != 0)
//        height -= modVal;
//    int bestHeight = (offsetY+height > maxHeight) ? maxHeight-offsetY-((maxHeight-offsetY)%16) : height;
////    int bestHeight = (offsetY >= maxHeight-16) ? 16 : height;
//
//    if (cameras[0].Height.IsWritable() && cameras[1].Height.IsWritable() ) {
//        success = cameras[0].Height.TrySetValue(bestHeight) &&
//            cameras[1].Height.TrySetValue(bestHeight);
//    }
//    startGrabbing();
//    return success;
//}
//
//// NOTE: grabbing "pause" is necessary for setting image ROI
//bool StereoCamera::setImageROIoffsetXEmu(int offsetX) {
//    if (cameras.GetSize() != 2) {
//        return false;
//    }
//
//    //"Setting Image ROI offsetX=" << std::to_string(offsetX);
//    bool success = false;
//
//    if(cameras.IsGrabbing())
//        stopGrabbing();
//
//    int maxWidth = getImageROIwidthMax();;
//    int width = getImageROIwidth();;
//
//    // NOTE: here .GetInc() are not used.. but could be?
//    if(maxWidth - offsetX < 16)
//        offsetX = maxWidth - 16;
//    int modVal=offsetX%16;
//    if(modVal != 0)
//        offsetX -= modVal;
//
//    if (width + offsetX <= maxWidth && cameras[0].OffsetX.IsWritable() && cameras[1].OffsetX.IsWritable() ) {
//        success = cameras[0].OffsetX.TrySetValue(offsetX) &&
//            cameras[1].OffsetX.TrySetValue(offsetX);
//    }
//    startGrabbing();
//    return success;
//}
//
//// NOTE: grabbing "pause" is necessary for setting image ROI
//bool StereoCamera::setImageROIoffsetYEmu(int offsetY) {
//    if (cameras.GetSize() != 2) {
//        return false;
//    }
//
//    //qDebug() << "Setting Image ROI offsetY=" << std::to_string(offsetY);
//    bool success = false;
//
//    if(cameras.IsGrabbing())
//        stopGrabbing();
//
//    int maxHeight = getImageROIheightMax();
//    int height = getImageROIheight();
//
//    // NOTE: here .GetInc() are not used.. but could be?
//    if(maxHeight - offsetY < 16)
//        offsetY = maxHeight - 16;
//    int modVal=offsetY%16;
//    if(modVal != 0)
//        offsetY -= modVal;
//
//    if (height + offsetY <= maxHeight && cameras[0].OffsetY.IsWritable() && cameras[1].OffsetY.IsWritable() ) {
//        success = cameras[0].OffsetY.TrySetValue(offsetY) &&
//            cameras[1].OffsetY.TrySetValue(offsetY);
//    }
//    startGrabbing();
//    return success;
//}

void StereoCamera::safelyCloseCameras() {
    if(!cameras.IsOpen())
        return;

    if(cameras.IsGrabbing())
        cameras.StopGrabbing();
    cameras.Close();

    if(cameraImageEventHandler) {
        disconnect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), this,
                   SIGNAL(onNewGrabResult(CameraImage)));
        disconnect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), frameCounter,
                   SLOT(count(CameraImage)));
        //disconnect(cameraImageEventHandler, SIGNAL(needsTimeSynchronization()), this, SLOT(resynchronizeTime()));
        disconnect(cameraImageEventHandler, SIGNAL(imagesSkipped()), this, SIGNAL(imagesSkipped()));
        cameras[0].DeregisterImageEventHandler(cameraImageEventHandler);
        cameras[1].DeregisterImageEventHandler(cameraImageEventHandler);
//        cameraImageEventHandler->DestroyImageEventHandler(); //
        cameraImageEventHandler = nullptr;
    }

    if(cameraConfigurationEventHandler0) {
        disconnect(cameraConfigurationEventHandler0, SIGNAL(cameraDeviceRemoved()), this,
                   SIGNAL(cameraDeviceRemoved()));
        cameras[0].DeregisterConfiguration(hardwareTriggerConfiguration0);
        cameras[0].DeregisterConfiguration(cameraConfigurationEventHandler0);
        hardwareTriggerConfiguration0 = nullptr;
        cameraConfigurationEventHandler0 = nullptr;
    }

    if(cameraConfigurationEventHandler1) {
        disconnect(cameraConfigurationEventHandler1, SIGNAL(cameraDeviceRemoved()), this,
                   SIGNAL(cameraDeviceRemoved()));
        cameras[1].DeregisterConfiguration(hardwareTriggerConfiguration1);
        cameras[1].DeregisterConfiguration(cameraConfigurationEventHandler1);
        hardwareTriggerConfiguration1 = nullptr;
        cameraConfigurationEventHandler1 = nullptr;
    }
}

#else

StereoCamera::StereoCamera(QObject* parent) : Camera(parent),
                                              cameras(2),
                                              frameCounter(new CameraFrameRateCounter(parent)),
                                              cameraCalibration(new StereoCameraCalibration()),
                                              calibrationThread(new QThread()),
                                              lineSource("Line1") {

    settingsDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    if(!settingsDirectory.exists()) {
// mkdir(".") DOES NOT WORK ON MACOS, ONLY WINDOWS. (Reported on MacOS 12.7.6 and Windows 10)
//        settingsDirectory.mkdir(".");
        QDir().mkpath(settingsDirectory.absolutePath());
    }

    // Calibration thread working
    cameraCalibration->moveToThread(calibrationThread);
    calibrationThread->start();
    calibrationThread->setPriority(QThread::HighPriority);

    connect(frameCounter, SIGNAL(fps(double)), this, SIGNAL(fps(double)));
    connect(frameCounter, SIGNAL(framecount(int)), this, SIGNAL(framecount(int)));
}

// Destroys the stereo camera
// Closes the camera array
StereoCamera::~StereoCamera() {
    /*
    if(cameras.IsOpen()) {
        safelyCloseCameras();
    }
    delete cameraImageEventHandler;
     */
    if (cameraCalibration != nullptr)
        cameraCalibration->deleteLater();
    if (calibrationThread != nullptr) {
        calibrationThread->quit();
        calibrationThread->deleteLater();
    }
}

void StereoCamera::resizeStreamBuffer() {

    // TODO: UNNECESSARY
    stopGrabbing();

    GError *error = nullptr;

    // Create the stream object with callback
    // TODO: is it sure that using the same stream for two different cameras can work freely?
    callbackData.stream = arv_camera_create_stream(cameras[0], cameraImageEventHandler->stream_callback, &callbackData, &error);
    callbackData.stream = arv_camera_create_stream(cameras[1], cameraImageEventHandler->stream_callback, &callbackData, &error);

    if (ARV_IS_STREAM (callbackData.stream)) {
        int i;
        size_t payload;

        // Retrieve the payload size for buffer creation
        error = nullptr;
        // TODO: we assume here that payload size is the same for both cameras. Should be.
        payload = arv_camera_get_payload(cameras[0], &error);
        if(!error) {
            // TODO: should be a huge number, e.g. 20-50 ?
            for (i = 0; i < 20; i++)
                arv_stream_push_buffer(callbackData.stream, arv_buffer_new(payload, NULL));
        }
    }

    if(error) {
        wrappedErrorOccured(error);
    }

}

// TODO
void StereoCamera::genericExceptionOccured(const std::exception &e, const GError &lastAravisError) {
    //QThread::msleep(1000);
    std::cerr << "An Aravis exception occurred." << std::endl<< e.what() << std::endl;

    // TODO: sketchy check if the device was removed or not
    // TODO: which camera doe this check for..? Both? Any?
    bool deviceRemoved = false;
    if(QString::fromStdString(lastAravisError.message).toLower().contains("remov")) {
        deviceRemoved = true;
    }

    genericExceptionOccured(e, deviceRemoved);
}

void StereoCamera::genericExceptionOccured(const std::exception &e, bool deviceRemoved) {
    //QThread::msleep(1000);
    std::cerr << "An Aravis exception occurred." << std::endl<< e.what() << std::endl;

    // logic: only quick cleanup if the device got removed. If not, we will keep running

    if (deviceRemoved) {
        emit cameraDeviceRemoved();
        arv_shutdown();
        g_clear_object (&cameras[0]);
        g_clear_object (&cameras[1]);
        cameras[0] = nullptr;
        cameras[1] = nullptr;
    }
}

// Attaches the main and secondary cameras to the camera array, based on their given device information
void StereoCamera::attachCameras(const QString &friendlyNameMain, const QString &friendlyNameSecondary) {
    /*

    auto diMain = CDeviceInfo().SetFriendlyName(friendlyNameMain.toStdString().c_str());
    auto diSecondary = CDeviceInfo().SetFriendlyName(friendlyNameSecondary.toStdString().c_str());

    // TODO: LOOKUP HERE, NOT IN STEREO CAMERA SETTINGS DIALOG

    attachCameras(diMain, diSecondary);
     */
}

// Attaches the main and secondary cameras to the camera array, based on their given device information
void StereoCamera::attachCameras(const ArvDevice &diMain, const ArvDevice &diSecondary) {
    /*

    // If cameras are already attached to the array, remove them
    if(cameras.GetSize() > 0) {
        if(cameras.IsGrabbing())
            cameras.StopGrabbing();
        cameras.Close();
        cameras.DetachDevice();
        cameras.DestroyDevice();
//        safelyCloseCameras();
    }

    CTlFactory& TlFactory = CTlFactory::GetInstance();
    //IGigETransportLayer* pTl = dynamic_cast<IGigETransportLayer*>(TlFactory.CreateTl( Pylon::BaslerGigEDeviceClass ));

    cameras[0].Attach(TlFactory.CreateDevice(diMain));
    cameras[1].Attach(TlFactory.CreateDevice(diSecondary));

    std::cout<<"Attached Camera0:" << cameras[0].GetDeviceInfo().GetFriendlyName() << std::endl;
    std::cout<<"Attached Camera1:" << cameras[1].GetDeviceInfo().GetFriendlyName() << std::endl << std::endl;
     */
}

// Opens the stereo camera through its corresponding camera array
// If the stereo camera is already open, it is first closed and then reopened again
// CAUTION: Its important for the stereo cameras to be in sync, that the stereo camera is first opened, and only then the hardware trigger source is started
// If the hardware triggers are started before opening the camera, the camera images will not be in sync due to the sequential opening of the camera
// (one camera will receive a trigger signal before the other)
void StereoCamera::open(bool enableHardwareTrigger) {
    /*

    if(cameras.GetSize() < 2) {
        std::cerr << "StereoCamera: must have two cameras connected."<< std::endl;
        return;
    }

    if(cameras.IsOpen()) {
        if(cameras.IsGrabbing())
            cameras.StopGrabbing();
        cameras.Close();
//        safelyCloseCameras();
    }

    try {
        // Register the configurations of the cameras

        ////////////////////////////////////////////////
        // ENABLING HARDWARE TRIGGER - IF IT IS NOT AN EMULATED CAMERA

    GError *error = nullptr;

    qDebug() << "SingleCamera: Enabling Hardware trigger to line source: " + lineSource << " to state: " << state;

    frameCounter->reset();

    try {

        stopGrabbing();

        ////auto a = arv_camera_get_acquisition_mode(camera, &error);
        ////auto t = arv_camera_get_trigger_source(camera, &error);
        //bool isSoftwareTriggerSupported = arv_camera_is_software_trigger_supported(camera, &error);
        auto device0 = arv_camera_get_device(cameras[0]);
        auto device1 = arv_camera_get_device(cameras[1]);

        // TODO: set line source if not set
        //setLineSource(lineSource);

        //if(state)
        arv_device_set_string_feature_value(device0, "TriggerSelector", "FrameStart", &error);
        if(!error) arv_device_set_string_feature_value(device1, "TriggerSelector", "FrameStart", &error);
        //else
        //    arv_device_set_string_feature_value(device, "TriggerSelector", "AcquisitionStart", &error);

        if(error){
            qDebug() << "Could not set TriggerSelector to value FrameStart.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }

        //auto b = arv_acquisition_mode_from_string("Continuous");
        //if(!error)
        error = nullptr;
        arv_camera_set_acquisition_mode(cameras[0], ARV_ACQUISITION_MODE_CONTINUOUS, &error);
        arv_camera_set_acquisition_mode(cameras[1], ARV_ACQUISITION_MODE_CONTINUOUS, &error);
        if(error){
            qDebug() << "Could not set ARV_ACQUISITION_MODE_CONTINUOUS.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }

        std::string stateStr = (state) ? "On" : "Off";
        error = nullptr;
        arv_device_set_string_feature_value(device0, "TriggerMode", stateStr.c_str(), &error);
        arv_device_set_string_feature_value(device1, "TriggerMode", stateStr.c_str(), &error);
        if(error){
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }

        if(!state) {
            error = nullptr;
            arv_camera_software_trigger(cameras[0], &error);
            if(!error) arv_camera_software_trigger(cameras[1], &error);
            qDebug() << "Started software triggering.";
        }
        //if(!error) arv_camera_set_trigger_source(camera, , &error);
        if(!error){
            hardwareTriggerEnabled = state;
        } else {
            qDebug() << "Could not set hardware/software triggering.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }

        //camera.Open();

        // NOTE: Sure we need this here as well?
        synchronizeTime();
        cameraImageEventHandler->setTimeSynchronization(cameraTime, systemTime);

        // NOTE: For some reason it seems with GigE specifically, we need to have this
        resizeStreamBuffer();

        startGrabbing();

        // TODO: for some reason we need this workaround to get things started
        //setImageROIwidth(getImageROIwidth());

        // DEV
        //    auto temp = arv_camera_get_integer(camera, "Width", &error);
        //    arv_camera_set_integer(camera, "Width", temp, &error);

        //stopGrabbing();
        //startGrabbing();

        //if(!state) {
        //    arv_camera_software_trigger(camera, &error);
        //}
        ////if(!error) arv_camera_set_trigger_source(camera, , &error);
        //if(!error){
        //    hardwareTriggerEnabled = state;
        //} else {
        //    qDebug() << "Could not set hardware/software triggering.";
        //    qDebug() << "Error during aravis API call. Message: " << error->message;
        //}

    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }

        ////////////////////////////////////////////////


        cameraConfigurationEventHandler0 = new CameraConfigurationEventHandler();
        cameraConfigurationEventHandler1 = new CameraConfigurationEventHandler();
        connect(cameraConfigurationEventHandler0, SIGNAL(cameraDeviceRemoved()), this, SIGNAL(cameraDeviceRemoved()));
        connect(cameraConfigurationEventHandler1, SIGNAL(cameraDeviceRemoved()), this, SIGNAL(cameraDeviceRemoved()));
        cameras[0].RegisterConfiguration(cameraConfigurationEventHandler0, RegistrationMode_ReplaceAll, Cleanup_Delete);
        cameras[1].RegisterConfiguration(cameraConfigurationEventHandler1, RegistrationMode_ReplaceAll, Cleanup_Delete);

        // Setting both to receive hardware trigger signals on the given line source
        // NOTE: always true except when emulated cameras are used
        if(enableHardwareTrigger) {
            hardwareTriggerConfiguration0 = new HardwareTriggerConfiguration(lineSource);
            hardwareTriggerConfiguration1 = new HardwareTriggerConfiguration(lineSource);
            cameras[0].RegisterConfiguration(hardwareTriggerConfiguration0, RegistrationMode_Append, Cleanup_Delete);
            cameras[1].RegisterConfiguration(hardwareTriggerConfiguration1, RegistrationMode_Append, Cleanup_Delete);
//            cameras[0].TriggerActivation.SetValue(TriggerActivation_RisingEdge);
//            cameras[1].TriggerActivation.SetValue(TriggerActivation_RisingEdge);
//            cameras[0].TriggerMode.SetValue(TriggerMode_On);
//            cameras[1].TriggerMode.SetValue(TriggerMode_On);
        }

        cameraImageEventHandler = new StereoCameraImageEventHandler(this->parent());
        connect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), this, SIGNAL(onNewGrabResult(CameraImage)));
        connect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), frameCounter, SLOT(count(CameraImage)));
        //connect(cameraImageEventHandler, SIGNAL(needsTimeSynchronization()), this, SLOT(resynchronizeTime()));
        connect(cameraImageEventHandler, SIGNAL(imagesSkipped()), this, SIGNAL(imagesSkipped()));

        // Register the image event handler
        // IMPORTANT: For both cameras the same handler object is registered, as the handler must receive both main and secondary images to create a single stereo camera image
        cameras[0].RegisterImageEventHandler(cameraImageEventHandler, RegistrationMode_ReplaceAll, Cleanup_None); // Cleanup_None as its deleted in unregister
        cameras[1].RegisterImageEventHandler(cameraImageEventHandler, RegistrationMode_ReplaceAll, Cleanup_None);

        cameras.Open();

        // Synchronize the camera time to the system time
        synchronizeTime();

        cameraImageEventHandler->setTimeSynchronization(cameraMainTime, cameraSecondaryTime, systemTime);

        cameras[0].PixelFormat.SetValue(PixelFormat_Mono8);
        cameras[1].PixelFormat.SetValue(PixelFormat_Mono8);

        // Load calibration if existing
        if(!cameraCalibration->isCalibrated()) {
            // If we already used this camera before, a config file may exists
            loadCalibrationFile();
        }

        CIntegerParameter heartbeat0( cameras[0].GetTLNodeMap(), "HeartbeatTimeout" );
        CIntegerParameter heartbeat1( cameras[1].GetTLNodeMap(), "HeartbeatTimeout" );
        heartbeat0.TrySetValue( 1000, IntegerValueCorrection_Nearest );
        heartbeat1.TrySetValue( 1000, IntegerValueCorrection_Nearest );

        // TODO: Revise if this check is needed at ll! If really needed, make it hang on a
        //  thread, or whatever, but do not obstruct grabbing if not needed to
        //if (cameras[0].CanWaitForFrameTriggerReady() && cameras[1].CanWaitForFrameTriggerReady()) {
            startGrabbing();
        //} else {
        //    qDebug() << "Camera can not be queried whether it is ready to accept the next frame trigger.";
        //}

        // Rewrite these properties in the cameras in order to be able to read ResultingFramerate later
        if( cameras.GetSize()>0 &&
            cameras[0].AcquisitionFrameRateEnable.IsReadable() &&
            cameras[0].AcquisitionFrameRateEnable.IsWritable() &&
            cameras[0].AcquisitionFrameRateEnable.GetValue() ) {

            cameras[0].AcquisitionFrameRateEnable.TrySetValue(false);
        }
        if( cameras.GetSize()>1 &&
            cameras[1].AcquisitionFrameRateEnable.IsReadable() &&
            cameras[1].AcquisitionFrameRateEnable.IsWritable() &&
            cameras[1].AcquisitionFrameRateEnable.GetValue() ) {

            cameras[1].AcquisitionFrameRateEnable.TrySetValue(false);
        }

    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
     */
}

// Synchronize the camera to system time
// At the given point in time, both camera and system time are recorded, which is then used to convert between them for later image timestamps
void StereoCamera::synchronizeTime() {
    /*

    if(cameras.GetSize() < 2) {
        std::cerr << "StereoCamera: must have two cameras connected."<< std::endl;
        return;
    }

    if (!isEmulated()){
        cameras[0].TimestampLatch.Execute();
        cameras[1].TimestampLatch.Execute();
    }
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> epoche = std::chrono::time_point<std::chrono::system_clock>{};


    if (!isEmulated()){
        cameraMainTime = static_cast<uint64>(cameras[0].TimestampLatchValue.GetValue());
        cameraSecondaryTime = static_cast<uint64>(cameras[1].TimestampLatchValue.GetValue());
    }
    else {
        cameraMainTime = static_cast<uint64>(start.time_since_epoch().count());
        cameraSecondaryTime = static_cast<uint64>(start.time_since_epoch().count());
    }


    systemTime  = std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count();
    std::time_t startTime = std::chrono::system_clock::to_time_t(start);
    std::time_t epochTime = std::chrono::system_clock::to_time_t(epoche);

    qInfo() << "Camera Synchronize Time";
    qInfo() << "=========================";
    qInfo() << "Timestamp Camera Main: " << cameraMainTime;
    qInfo() << "Timestamp Camera Secondary: " << cameraSecondaryTime;
    qInfo() << "Timestamp System: " << systemTime;
    qInfo() << "System Epoch: " << std::ctime(&epochTime);
    qInfo() << "System Time: " << std::ctime(&startTime);
    qInfo() << "Time from Epoch (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    qInfo() << "Time from Epoch (us): " << std::chrono::duration_cast<std::chrono::microseconds>(start.time_since_epoch()).count();
    qInfo() << "Time from Epoch (ns): " << std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count();
    qInfo() << "=========================";
     */
}


bool StereoCamera::isOpen() {
    return (cameras[0] != nullptr && cameras[1] != nullptr);
    // TODO: check if any is closed but the other one is not...?
    //return cameras.IsOpen();
}

// Close the stereo camera and release all Pylon resources
void StereoCamera::close() {

    qDebug() << "StereoCamera: Releasing resources.";

    // TODO: might not necessarily happen here
    stopGrabbing();

    // // NOTE: already done in stopGrabbing
    // arv_stream_stop_thread(callbackData.stream, true);
    // g_clear_object(&callbackData.stream);

    g_clear_object(&cameras[0]);
    g_clear_object(&cameras[1]);
    cameras[0] = nullptr;
    cameras[1] = nullptr;

    /*

    qDebug() << "StereoCamera: Releasing pylon resources.";
    cameras.StopGrabbing();

//    for(int i = 0; i<cameras.GetSize(); i++) {
//        cameras[i].DeregisterImageEventHandler(cameraImageEventHandler);
//    }

//    cameras.Close();
     */

    // TODO?
//    safelyCloseCameras();
}

// Current exposure time value of the main camera
int StereoCamera::getExposureTimeValue() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool canGet = arv_camera_is_exposure_time_available(cameras[0], &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            error = nullptr;
            val = (int)round(arv_camera_get_exposure_time(cameras[0], &error));
        }
        if(error) {
            qDebug() << "Could not get exposure time value.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// Minimal possible exposure time value of the main camera
int StereoCamera::getExposureTimeMin() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool canGet = arv_camera_is_exposure_time_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_exposure_time_available(cameras[1], &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            error = nullptr;
            arv_camera_get_exposure_time_bounds(cameras[0], &valMin, &valMax, &error);
            // extra checks could happen here
            val = (int)round(valMin);

            if(!error) {
                valMin = 0;
                valMax = 0;
                error = nullptr;
                arv_camera_get_exposure_time_bounds(cameras[1], &valMin, &valMax, &error);
                // extra checks could happen here
                int val1 = (int) round(valMin);
                val = std::max(val, val1); // safer
            }
        }
        if(error) {
            qDebug() << "Could not get exposure time minimum value.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// Maximal possible exposure time value of the main camera
int StereoCamera::getExposureTimeMax() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool canGet = arv_camera_is_exposure_time_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_exposure_time_available(cameras[1], &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            error = nullptr;
            arv_camera_get_exposure_time_bounds(cameras[0], &valMin, &valMax, &error);
            // extra checks could happen here
            val = (int)round(valMax);

            if(!error) {
                valMin = 0;
                valMax = 0;
                error = nullptr;
                arv_camera_get_exposure_time_bounds(cameras[1], &valMin, &valMax, &error);
                // extra checks could happen here
                int val1 = (int) round(valMax);
                val = std::min(val, val1); // safer
            }
        }
        if(error) {
            qDebug() << "Could not get exposure time maximum value.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// Current Gain value of the main camera
double StereoCamera::getGainValue() {
    GError *error = nullptr;
    double val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool canGet = arv_camera_is_gain_available(cameras[0], &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            error = nullptr;
            val = arv_camera_get_gain(cameras[0], &error);
        }
        if(error) {
            qDebug() << "Could not get gain value.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// Minimal possible Gain value of the main camera
double StereoCamera::getGainMin() {
    GError *error = nullptr;
    double val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool canGet = arv_camera_is_gain_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_gain_available(cameras[1], &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            error = nullptr;
            arv_camera_get_gain_bounds(cameras[0], &valMin, &valMax, &error);
            // extra checks could happen here
            val = valMin;

            if(!error) {
                valMin = 0;
                valMax = 0;
                error = nullptr;
                arv_camera_get_gain_bounds(cameras[1], &valMin, &valMax, &error);
                // extra checks could happen here
                double val1 = valMin;
                val = std::max(val, val1); // safer
            }
        }
        if(error) {
            qDebug() << "Could not get gain minimum value.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// Maximal possible Gain value of the main camera
double StereoCamera::getGainMax() {
    GError *error = nullptr;
    double val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool canGet = arv_camera_is_gain_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_gain_available(cameras[1], &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            error = nullptr;
            arv_camera_get_gain_bounds(cameras[0], &valMin, &valMax, &error);
            // extra checks could happen here
            val = valMax;

            if(!error) {
                valMin = 0;
                valMax = 0;
                error = nullptr;
                arv_camera_get_gain_bounds(cameras[1], &valMin, &valMax, &error);
                // extra checks could happen here
                double val1 = valMax;
                val = std::min(val, val1); // safer
            }
        }
        if(error) {
            qDebug() << "Could not get gain minimum value.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// Sets the Gain value of the main and secondary camera
void StereoCamera::setGainValue(double value) {
    GError *error = nullptr;
    try {
        bool canGet = arv_camera_is_gain_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_gain_available(cameras[1], &error);
        double valMin = 0;
        double valMax = 0;
        if(canGet) {
            error = nullptr;
            arv_camera_get_gain_bounds(cameras[0], &valMin, &valMax, &error);
            double valMin1 = 0;
            double valMax1 = 0;
            if(!error) arv_camera_get_gain_bounds(cameras[1], &valMin1, &valMax1, &error);
            valMin = std::max(valMin, valMin1); // safer
            valMax = std::min(valMax, valMax1); // safer
        }
        if(error) {
            qDebug() << "Could not get gain value bounds.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(value <= valMax && value >= valMin) {
            error = nullptr;
            arv_camera_set_gain(cameras[0], value, &error);
            if(!error) arv_camera_set_gain(cameras[1], value, &error);
            if(error) {
                qDebug() << "Could not set gain value.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
}

// Sets the exposure time value of the main and secondary camera
void StereoCamera::setExposureTimeValue(int value) {
    GError *error = nullptr;
    try {
        bool canGet = arv_camera_is_exposure_time_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_exposure_time_available(cameras[1], &error);
        double valMin = 0;
        double valMax = 0;
        if(canGet) {
            error = nullptr;
            arv_camera_get_exposure_time_bounds(cameras[0], &valMin, &valMax, &error);
            double valMin1 = 0;
            double valMax1 = 0;
            if(!error) arv_camera_get_exposure_time_bounds(cameras[1], &valMin1, &valMax1, &error);
            valMin = std::max(valMin, valMin1); // safer
            valMax = std::min(valMax, valMax1); // safer
        }
        if(error) {
            qDebug() << "Could not get exposure time value bounds.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(value <= valMax && value >= valMin) {
            error = nullptr;
            arv_camera_set_exposure_time(cameras[0], (double)value, &error);
            if(!error) arv_camera_set_exposure_time(cameras[1], (double)value, &error);
            if(error) {
                qDebug() << "Could not set exposure time value.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
}

// Loads the camera settings of the main camera and sets its values to both the main and secondary camera
// Reads the Basler specific camera setting file format
// Camera settings are automatically saved in the applications settings directory (Path visible in the about window)
void StereoCamera::loadMainFromFile(const QString &filename) {
    // TODO: It seems aravis does not yet support saving and loading all features. We could iterate through the map
    //  and save what we can, then restore all upon opening, but this requires further larger efforts. Yet unsupported
    /*
    bool wasOpen = cameras.IsOpen();
    if(!wasOpen) {
        cameras.Open();
    }

    try {
        CFeaturePersistence::Load(filename, &cameras[0].GetNodeMap(), true);
    } catch (const GenericException &e) {
        // Error handling.
        std::cerr << "An exception occurred: " << e.GetDescription() << std::endl;
    }

    // Set the main and second camera with same settings
    setExposureTimeValue(cameras[0].ExposureTime.GetValue());
    setGainValue(cameras[0].Gain.GetValue());

    if(isEnabledAcquisitionFrameRate()) {
        setAcquisitionFPSValue(cameras[0].AcquisitionFrameRate.GetValue());
    }
    if(!wasOpen) {
        cameras.Close();
//        safelyCloseCameras();
    }
     */
}

// Saves the main camera settings to file
// Uses the Basler specific file format
void StereoCamera::saveMainToFile(const QString &filename) {
    // TODO: It seems aravis does not yet support saving and loading all features. We could iterate through the map
    //  and save what we can, then restore all upon opening, but this requires further larger efforts. Yet unsupported
    /*
    try {
        CFeaturePersistence::Save(filename, &cameras[0].GetNodeMap());
    } catch (const GenericException &e) {
        // Error handling.
        std::cerr << "An exception occurred: " << e.GetDescription() << std::endl;
    }
     */
}

// Reads if a image acquisition frame rate is enabled
// The value of the image acquisition may overwrite hardware trigger framerates
// Assumes that main and secondary camera have the same settings
bool StereoCamera::isEnabledAcquisitionFrameRate() {
    GError *error = nullptr;
    bool val = false;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        val = arv_camera_get_frame_rate_enable(cameras[0], &error);
        if(error) {
            qDebug() << "Could not get whether acquisition frame rate setting is enabled or not.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

bool StereoCamera::isEmulated() {
    // TODO
    return (getFriendlyNames()[0].toLower().contains("emu") ||
            getFriendlyNames()[1].toLower().contains("emu"));
}

void StereoCamera::enableAcquisitionFrameRate(bool enabled) {
    GError *error = nullptr;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return;

    try {
        arv_camera_set_frame_rate_enable(cameras[0], enabled, &error);
        if(!error) arv_camera_set_frame_rate_enable(cameras[1], enabled, &error);
        if(error) {
            qDebug() << "Could not set acquisition frame rate enabled/disabled.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
}

void StereoCamera::setAcquisitionFPSValue(int value) {
    GError *error = nullptr;
    try {
        bool canGet = arv_camera_is_frame_rate_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_frame_rate_available(cameras[1], &error);
        if(error) {
            qDebug() << "Acquisition framerate not available.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(canGet) {
            error = nullptr;
            arv_camera_set_frame_rate(cameras[0], (double)value, &error);
            if(!error) arv_camera_set_frame_rate(cameras[1], (double)value, &error);
            if(error) {
                qDebug() << "Could not set acquisition framerate.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
}

// Reads and returns the value of the image acquisition frame rate for the main camera
// Assumes that both cameras are set the same through the above function setAcquisitionFPSValue
int StereoCamera::getAcquisitionFPSValue() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool canGet = arv_camera_is_frame_rate_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_frame_rate_available(cameras[1], &error);
        if(error) {
            qDebug() << "Acquisition framerate not available.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(canGet) {
            // NOTE: rounding here
            error = nullptr;
            // TODO ?
            val = (int)round(arv_camera_get_frame_rate(cameras[0], &error));
            if(error) {
                qDebug() << "Could not get acquisition framerate.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// Minimal possible image acquisition frame rate of the main camera
int StereoCamera::getAcquisitionFPSMin() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool canGet = arv_camera_is_frame_rate_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_frame_rate_available(cameras[1], &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            error = nullptr;
            arv_camera_get_frame_rate_bounds(cameras[0], &valMin, &valMax, &error);
            // extra checks could happen here
            val = (int)round(valMin);

            if(!error) {
                valMin = 0;
                valMax = 0;
                error = nullptr;
                arv_camera_get_frame_rate_bounds(cameras[1], &valMin, &valMax, &error);
                // extra checks could happen here
                int val1 = valMin;
                val = std::max(val, val1); // safer
            }
        }
        if(error) {
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// Maximal possible image acquisition frame rate of the main camera
// This may be influenced by the current camera settings and its value may change
int StereoCamera::getAcquisitionFPSMax() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool canGet = arv_camera_is_frame_rate_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_frame_rate_available(cameras[1], &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            error = nullptr;
            arv_camera_get_frame_rate_bounds(cameras[0], &valMin, &valMax, &error);
            // extra checks could happen here
            val = (int)round(valMax);

            if(!error) {
                valMin = 0;
                valMax = 0;
                error = nullptr;
                arv_camera_get_frame_rate_bounds(cameras[1], &valMin, &valMax, &error);
                // extra checks could happen here
                int val1 = valMax;
                val = std::min(val, val1); // safer
            }
        }
        if(error) {
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// Camera frame rate resulting by the current camera settings
double StereoCamera::getResultingFrameRateValue() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        //auto temp = arv_camera_get_integer(camera, "ResultingFrameRate", &error);
        GValue v = G_VALUE_INIT;
        //g_value_init(&v, G_TYPE_DOUBLE);


        // TODO both?
        if(arv_device_is_feature_available(arv_camera_get_device(cameras[0]), "ResultingFrameRate", NULL)) {
            arv_device_get_feature_value(arv_camera_get_device(cameras[0]), "ResultingFrameRate", &v, &error);
        } else if(arv_device_is_feature_available(arv_camera_get_device(cameras[0]), "ResultingFrameRateAbs", NULL)) {
            arv_device_get_feature_value(arv_camera_get_device(cameras[0]), "ResultingFrameRateAbs", &v, &error);
        } else {
            qDebug() << "Resulting framerate values are not available.";
            qDebug() << "Could not obtain resulting framerate value. This camera might not support it.";
            qDebug() << "Falling back to acquisition framerate value.";

            return getAcquisitionFPSValue();
        }

        if(error) {
            qDebug() << "Could not obtain resulting framerate value. This camera might not support it.";
            qDebug() << "Error during aravis API call. Message: " << error->message;

            // TODO
        }
        auto temp = g_value_get_double(&v);
        // additional checks could come here
        if(error) {
            qDebug() << "Could neither get resulting framerate, nor acquisition framerate.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else {
            val = (int)temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

StereoCameraCalibration *StereoCamera::getCameraCalibration() {
    return cameraCalibration;
}


bool StereoCamera::isAutoGainAvailable() {
    GError *error = nullptr;
    bool val = false;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        val = arv_camera_is_gain_auto_available(cameras[0], &error);
        if(!error) val &= arv_camera_is_gain_auto_available(cameras[1], &error);
        if(error) {
            qDebug() << "Could not get whether auto gain setting is available, assuming not.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

bool StereoCamera::isAutoExposureAvailable() {
    GError *error = nullptr;
    bool val = false;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        val = arv_camera_is_exposure_auto_available(cameras[0], &error);
        if(!error) val &= arv_camera_is_exposure_auto_available(cameras[0], &error);
        if(error) {
            qDebug() << "Could not get whether auto exposure setting is available, assuming not.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// Performs automatically setting of the Gain value based on the current camera image
// Main camera is used to automatically find a Gain value, this value is then applied to the secondary camera
void StereoCamera::autoGainOnce() {

    // TODO: When auto gain/exposure is applied, it can result in DIFFERENT gain/expo values
    //  for the two cameras... it that okay? Should not we set one, and then set the other likewise?

    // TODO: support continuous auto gain! (but only gain!)

    try {
        GError *error = nullptr;

        if(!cameras[0] || !cameras[1]) {
            return;
        }

        stopGrabbing();

        bool isAuto = arv_camera_is_gain_auto_available(cameras[0], &error);
        if(!error) isAuto &= arv_camera_is_gain_auto_available(cameras[1], &error);

        if(error) {
            qDebug() << "Auto gain is not available.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(isAuto) {
            error = nullptr;
            arv_camera_set_gain_auto(cameras[0], ArvAuto::ARV_AUTO_ONCE, &error);
            if(!error) arv_camera_set_gain_auto(cameras[1], ArvAuto::ARV_AUTO_ONCE, &error);
            if(error) {
                qDebug() << "Could not et auto gain.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }

    // NOTE: For some reason it seems in case of auto functions with GigE specifically, we need to have this
    resizeStreamBuffer();

    startGrabbing();
}

// Performs automatically setting of the exposure time value based on the current main camera image
// Main camera is used to automatically find a exposure time, this value is then applied to the secondary camera
void StereoCamera::autoExposureOnce() {

    // TODO: When auto gain/exposure is applied, it can result in DIFFERENT gain/expo values
    //  for the two cameras... it that okay? Should not we set one, and then set the other likewise?

    try {
        GError *error = nullptr;

        if(!cameras[0] || !!cameras[1]) {
            return;
        }

        stopGrabbing();

        bool isAuto = arv_camera_is_exposure_auto_available(cameras[0], &error);
        if(!error) isAuto &= arv_camera_is_exposure_auto_available(cameras[1], &error);

        if(error) {
            qDebug() << "Auto exposure is not available.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(isAuto) {
            error = nullptr;
            arv_camera_set_exposure_time_auto(cameras[0], ArvAuto::ARV_AUTO_ONCE, &error);
            if(!error) arv_camera_set_exposure_time_auto(cameras[1], ArvAuto::ARV_AUTO_ONCE, &error);
            if(error) {
                qDebug() << "Could not set auto exposure.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }

    // NOTE: For some reason it seems in case of auto functions with GigE specifically, we need to have this
    resizeStreamBuffer();

    startGrabbing();
}

// The current used linesource as the hardware trigger source
// The same linesource is used for both cameras
// When the linesource changes, the camera must be closed and opened again
QString StereoCamera::getLineSource() {

    GError *error = nullptr;
    //QString val = "";

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        //return val;
        return lineSource;

    try {
        // otherwise we could just query the "TriggerSource" GenICam feature value
        QString temp = QString(arv_camera_get_trigger_source(cameras[0], &error));
        QString temp1 = ";";
        if(!error)
            temp1 = QString(arv_camera_get_trigger_source(cameras[1], &error));
        if(!error && temp != temp1)
            setLineSource(temp);

        // additional checks could come here
        if(error) {
            qDebug() << "Could not get trigger source.";
            wrappedErrorOccured(error);
        } else {
            lineSource = temp;
            //val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    //return val;

    return lineSource;
}

// Sets the linesource used as the hardware trigger source
// The same linesource is used for both cameras
// When the linesource changes, the camera must be closed and opened again
void StereoCamera::setLineSource(QString value) {
    GError *error = nullptr;
    //QString val = "";

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return;

    try {
        // otherwise we could just set the "TriggerSource" GenICam feature value
        arv_camera_set_trigger_source(cameras[0], value.toStdString().c_str(), &error);
        if(!error) arv_camera_set_trigger_source(cameras[1], value.toStdString().c_str(), &error);

        if(error) {
            qDebug() << "Could not set trigger source.";
            wrappedErrorOccured(error);
        } else {
            lineSource = value;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
}

CameraImageType StereoCamera::getType() {
    return CameraImageType::LIVE_STEREO_CAMERA;
}

void StereoCamera::startGrabbing() {
    if (isGrabbing())
        return;

    GError *error = nullptr;

    // TODO: set continous grabbing mode, if not set
//    arv_camera_set_acquisition_mode(camera, ArvAcquisitionMode::ARV_ACQUISITION_MODE_CONTINUOUS, &error);

    //if(!error)
    callbackData.aboutToStopGrabbing = false;
////    arv_stream_start_thread(callbackData.stream);
//    arv_camera_start_acquisition(camera, &error);

    arv_camera_start_acquisition(cameras[0], &error);
    if(!error) arv_camera_start_acquisition(cameras[1], &error);
    //arv_stream_try_pop_buffer(callbackData.stream);
    if(error) {
        qDebug() << "Could not start grabbing.";
        // TODO: stop grabbing explicitly?
        wrappedErrorOccured(error);
    } else {
        isGrabbingV = true;
        qDebug() << "Started grabbing!";
    }
    // TODO: here something is wrong in case of gv
    //  arv_gv_stream_start_thread: assertion 'priv->thread == NULL' failed
    arv_stream_start_thread(callbackData.stream);

    // pylon version
    //camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
}

void StereoCamera::stopGrabbing() {
    if (!isGrabbing())
        return;

    GError *error = nullptr;

    callbackData.aboutToStopGrabbing = true;

//    arv_stream_stop_thread(callbackData.stream, delete_buffers);
//    //g_clear_object (&callbackData.stream);
//    //callbackData.stream = NULL;
////    arv_stream_set_emit_signals(callbackData.stream, FALSE);
////    g_object_unref(callbackData.stream);
//    //g_clear_object (&callbackData.stream);
//    callbackData.stream = nullptr;
//    arv_camera_stop_acquisition(camera, &error);
    // TODO: for some reason it causes errors like the following:
    //  ** (process:8760): CRITICAL **: ...: arv_uv_stream_stop_thread: assertion 'priv->thread == NULL' failed
    //  ** (process:8760): CRITICAL **: ...: arv_uv_stream_start_thread: assertion 'priv->thread == NULL' failed
    //  although stop_thread is called, etc. Possible solution?

    arv_camera_stop_acquisition(cameras[0], &error);
    if(!error) arv_camera_stop_acquisition(cameras[1], &error);
    if(error) {
        qDebug() << "Could not gracefully stop grabbing.";
        qDebug() << "Error during aravis API call. Message: " << error->message;
        qDebug() << "Falling back to abort call.";
        arv_camera_abort_acquisition(cameras[0], NULL);
        arv_camera_abort_acquisition(cameras[1], NULL);
    }
    gboolean delete_buffers = true;
    arv_stream_stop_thread(callbackData.stream, delete_buffers);
    isGrabbingV = false;

    // TODO: tell the image event handler to drop all pending stereo frames (reset content and timestamp of all pending)
    cameraImageEventHandler->dropAllPending();

    qDebug() << "Stopped grabbing!";

    // pylon version
    //if (camera.IsOpen() && camera.IsGrabbing())
    //    camera.StopGrabbing();
}

std::vector<QString> StereoCamera::getFriendlyNames() {
    std::vector<QString> names;

    GError *error = nullptr;
    QString val = "";
    try {
        QString vendorName, deviceModel, serialNumber = "";

        vendorName = arv_camera_get_vendor_name(cameras[0], &error);
        if(!error) deviceModel = arv_camera_get_model_name(cameras[0], &error);
        if(!error) serialNumber = arv_camera_get_device_serial_number(cameras[0], &error);
        val = vendorName + " " + deviceModel + " (" + serialNumber + ")";
        names.push_back(val);

        vendorName = "";
        deviceModel= "";
        serialNumber = "";

        vendorName = arv_camera_get_vendor_name(cameras[1], &error);
        if(!error) deviceModel = arv_camera_get_model_name(cameras[1], &error);
        if(!error) serialNumber = arv_camera_get_device_serial_number(cameras[1], &error);
        val = vendorName + " " + deviceModel + " (" + serialNumber + ")";
        names.push_back(val);

    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }

    return names;
}

QString StereoCamera::getCalibrationFilename() {

    QString val = "";

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    val = settingsDirectory.filePath(
                getFriendlyNames()[0] + "_" + getFriendlyNames()[1] + "_stereo_calibration_" +
                QString::number(cameraCalibration->getPattern()) + "_" +
                QString::number(cameraCalibration->getSquareSize()) + "_" +
                QString::number(cameraCalibration->getBoardSize().width + 1) + "x" +
                QString::number(cameraCalibration->getBoardSize().height + 1) + ".xml");
    return val;
}

void StereoCamera::loadCalibrationFile() {

    QString configFile = getCalibrationFilename();
    configFile.replace(" ", "");
    if (QFile::exists(configFile)) {
        qDebug() << "Found calibration file in settings directory. Loading: " << configFile.toStdString();
        cameraCalibration->loadFromFile(configFile.toStdString().c_str());
    }
}

// TODO -----------------
// Synchronize the camera and system time and update the image event handler
void StereoCamera::resynchronizeTime() {
    /*
    if(cameras.IsOpen()) {
        std::cout<<"Resynchronizing Camera Time..."<<std::endl;
        synchronizeTime();
        cameraImageEventHandler->setTimeSynchronization(cameraMainTime, cameraSecondaryTime, systemTime);
    }
    */
}


int StereoCamera::getImageROIwidth() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        gint temp = arv_camera_get_integer(cameras[0], "Width", &error);
        gint temp1 = 1;
        if(!error) temp1 = arv_camera_get_integer(cameras[1], "Width", &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI Width.";
            wrappedErrorOccured(error);
        } else {
            if(temp != temp1) {
                setImageROIwidth(std::min(temp, temp1));
            }
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int StereoCamera::getImageROIheight() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        gint temp = arv_camera_get_integer(cameras[0], "Height", &error);
        gint temp1 = 1;
        if(!error) temp1 = arv_camera_get_integer(cameras[1], "Height", &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI Height.";
            wrappedErrorOccured(error);
        } else {
            if(temp != temp1) {
                setImageROIheight(std::min(temp, temp1));
            }
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int StereoCamera::getImageROIoffsetX() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        gint temp = arv_camera_get_integer(cameras[0], "OffsetX", &error);
        gint temp1 = 1;
        if(!error) temp1 = arv_camera_get_integer(cameras[1], "OffsetX", &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI OffsetX.";
            wrappedErrorOccured(error);
        } else {
            if(temp != temp1) {
                setImageROIoffsetX(std::min(temp, temp1));
            }
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int StereoCamera::getImageROIoffsetXInc() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        auto temp = arv_camera_get_x_offset_increment(cameras[0], &error);
        gint temp1 = 0;
        if(!error) temp1 = arv_camera_get_x_offset_increment(cameras[1], &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI OffsetX increment.";
            wrappedErrorOccured(error);
        } else {
            val = std::max(temp, temp1); // safer
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int StereoCamera::getImageROIoffsetY() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        gint temp = arv_camera_get_integer(cameras[0], "OffsetY", &error);
        gint temp1 = 1;
        if(!error) temp1 = arv_camera_get_integer(cameras[1], "OffsetY", &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI OffsetY.";
            wrappedErrorOccured(error);
        } else {
            if(temp != temp1) {
                setImageROIoffsetY(std::min(temp, temp1));
            }
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int StereoCamera::getImageROIoffsetYInc() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        auto temp = arv_camera_get_y_offset_increment(cameras[0], &error);
        gint temp1 = 0;
        if(!error) temp1 = arv_camera_get_y_offset_increment(cameras[1], &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI OffsetY increment.";
            wrappedErrorOccured(error);
        } else {
            val = std::max(temp, temp1); // safer
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// NOTE: Binning affects this
int StereoCamera::getImageROIwidthMax() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        gint valMin = 0;
        gint valMax = 0;
        arv_camera_get_width_bounds(cameras[0], &valMin, &valMax, &error);
        // additional checks could come here

        gint valMin1 = 0;
        gint valMax1 = 0;
        if(!error) {
            arv_camera_get_width_bounds(cameras[1], &valMin1, &valMax1, &error);
        }

        // NOTE: the Aravis library provides the maximum with the offset already subtracted.
        //  But in camera settings GUI, etc we want to know the max possible value, and the
        //  offset is already taken care of separately. So just add that.
        val = std::min(valMax, valMax1) + getImageROIoffsetX();

        if(error) {
            qDebug() << "Could not get image acquisition ROI Width maximum.";
            wrappedErrorOccured(error);
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int StereoCamera::getImageROIwidthInc() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        auto temp = arv_camera_get_width_increment(cameras[0], &error);
        gint temp1 = 0;
        if(!error) temp1 = arv_camera_get_width_increment(cameras[1], &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI width increment.";
            wrappedErrorOccured(error);
        } else {
            val = std::max(temp, temp1); // safer
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// NOTE: Binning affects this
int StereoCamera::getImageROIheightMax() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        gint valMin = 0;
        gint valMax = 0;
        arv_camera_get_height_bounds(cameras[0], &valMin, &valMax, &error);
        // additional checks could come here

        gint valMin1 = 0;
        gint valMax1 = 0;
        if(!error) {
            arv_camera_get_height_bounds(cameras[1], &valMin1, &valMax1, &error);
        }

        // NOTE: the Aravis library provides the maximum with the offset already subtracted.
        //  But in camera settings GUI, etc we want to know the max possible value, and the
        //  offset is already taken care of separately. So just add that.
        val = std::min(valMax, valMax1) + getImageROIoffsetY();

        if(error) {
            qDebug() << "Could not get image acquisition ROI Height maximum.";
            wrappedErrorOccured(error);
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int StereoCamera::getImageROIheightInc() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        auto temp = arv_camera_get_height_increment(cameras[0], &error);
        gint temp1 = 0;
        if(!error) temp1 = arv_camera_get_height_increment(cameras[1], &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI height increment.";
            wrappedErrorOccured(error);
        } else {
            val = std::max(temp, temp1); // safer
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

QRectF StereoCamera::getImageROI(){
    GError *error = nullptr;
    QRectF val = {0,0,0,0};

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        gint valXoffset = 0;
        gint valYoffset = 0;
        gint valWidth = 0;
        gint valHeight = 0;
        //arv_camera_get_region(camera, &valXoffset, &valYoffset, &valWidth, &valHeight, &error);

        if(!error) valXoffset = arv_camera_get_integer(cameras[0], "OffsetX", &error);
        if(!error) valYoffset = arv_camera_get_integer(cameras[0], "OffsetY", &error);
        if(!error) valWidth = arv_camera_get_integer(cameras[0], "Width", &error);
        if(!error) valHeight = arv_camera_get_integer(cameras[0], "Height", &error);

        if(!error && arv_camera_get_integer(cameras[1], "OffsetX", &error) != valXoffset)
            setImageROIoffsetX(valXoffset);
        if(!error && arv_camera_get_integer(cameras[1], "OffsetY", &error) != valYoffset)
            setImageROIoffsetY(valYoffset);
        if(!error && arv_camera_get_integer(cameras[1], "Width", &error) != valWidth)
            setImageROIwidth(valWidth);
        if(!error && arv_camera_get_integer(cameras[1], "Height", &error) != valHeight)
            setImageROIheight(valHeight);

        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI.";
            wrappedErrorOccured(error);
        } else {
            val = QRectF(valXoffset, valYoffset, valWidth, valHeight);
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

bool StereoCamera::isBinningAvailable() {
    GError *error = nullptr;
    bool val = false;

    if(!ARV_IS_CAMERA(cameras[0]) || !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool tval = arv_camera_is_binning_available(cameras[0], &error);
        if(error) tval &= arv_camera_is_binning_available(cameras[1], &error);
        if(error) {
            qDebug() << "Could not get whether binning setting is available, assuming not.";
            wrappedErrorOccured(error);
        }

        // IMPORTANT: there are camera models (e.g. Basler puA1280-54um) where the API will tell that the
        //  Horizontal Binning levels are minimum 1 and maxumim 2, however the Vertical Binning level is only 1.
        //  This is not an error in the APi, it is because the camera only supports the following binning
        //  combinations (H x V): 1x1, 2x1, 2x2 and the vertical 2 option will only show if the Horizontal
        //  is set to 2 already, as the 1x1 is not supported. So the y axis binning does not have to be checked here.

        // This way we can be 100% sure if binning is not available. Might be important for certain cameras.
        gint bxmin = 1;
        gint bxmax = 1;
        error = nullptr;
        arv_camera_get_x_binning_bounds(cameras[0], &bxmin, &bxmax, &error);
        tval &= (bxmin != bxmax);

        if(error) {
            qDebug() << "Could not get whether binning setting is available, assuming not.";
            wrappedErrorOccured(error);
        } else {
            bxmin = 1;
            bxmax = 1;
            error = nullptr;
            arv_camera_get_x_binning_bounds(cameras[1], &bxmin, &bxmax, &error);
            tval &= (bxmin != bxmax);

            if(error) {
                qDebug() << "Could not get whether binning setting is available, assuming not.";
                wrappedErrorOccured(error);
            } else {
                val = tval;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int StereoCamera::getBinningVal() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(cameras[0]) && !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool canGet = arv_camera_is_binning_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_binning_available(cameras[1], &error);
        if(error) {
            qDebug() << "Could not get binning value.";
            wrappedErrorOccured(error);
        } else if(canGet) {
            gint valX = 1;
            gint valY = 1;
            error = nullptr;
            arv_camera_get_binning(cameras[0], &valX, &valY, &error);
            // additional checks could come here
            if(error) {
                wrappedErrorOccured(error);
            } else {
                // TODO reset to larget/smaller (?) value if not equal

                gint valX1 = 1;
                gint valY1 = 1;
                error = nullptr;
                arv_camera_get_binning(cameras[1], &valX1, &valY1, &error);
                if(valX != valX1 || valY != valY1)
                    setBinningVal(valX);
                // additional checks could come here
                if(error) {
                    wrappedErrorOccured(error);
                } else {
                    // TODO reset to larget/smaller (?) value if not equal
                    val = valX;
                }
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int StereoCamera::getBinningMax() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(cameras[0]) && !ARV_IS_CAMERA(cameras[1]))
        return val;

    try {
        bool canGet = arv_camera_is_binning_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_binning_available(cameras[1], &error);
        if(error) {
            qDebug() << "Could not get binning value.";
            wrappedErrorOccured(error);
        } else if(canGet) {
            gint minX = 1;
            gint maxX = 1;
            error = nullptr;
            arv_camera_get_x_binning_bounds(cameras[0], &minX, &maxX, &error);
            // additional checks could come here
            if(error) {
                wrappedErrorOccured(error);
            } else {
                gint minX1 = 1;
                gint maxX1 = 1;
                error = nullptr;
                arv_camera_get_x_binning_bounds(cameras[1], &minX1, &maxX1, &error);
                if(maxX != maxX1)
                    return val;
                // additional checks could come here
                if(error) {
                    wrappedErrorOccured(error);
                } else {
                    val = maxX;
                }
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

bool StereoCamera::isTemperatureReadingSupported() {

    GError *error = nullptr;
    bool isit = false;

    if(!ARV_IS_CAMERA(cameras[0]) && !ARV_IS_CAMERA(cameras[1]))
        return isit;

    try {
        //auto temp = arv_camera_get_float(camera, "DeviceTemperature", &error);
        //// fallbacks: set "DeviceTemperatureSelector" value to "Sensor" or "Mainboard"

        if( arv_device_is_feature_available(arv_camera_get_device(cameras[0]), "DeviceTemperature", NULL) &&
            arv_device_is_feature_available(arv_camera_get_device(cameras[1]), "DeviceTemperature", NULL)) {

            isit = true;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return isit;
}

std::vector<double> StereoCamera::getTemperatures() {
    std::vector<double> temperatures = {CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE, CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE};

    GError *error = nullptr;

    if(!ARV_IS_CAMERA(cameras[0]) && !ARV_IS_CAMERA(cameras[1]) || !isTemperatureReadingSupported())
        return temperatures;

    try {
        auto temp = arv_camera_get_float(cameras[0], "DeviceTemperature", &error);
        // fallbacks: set "DeviceTemperatureSelector" value to "Sensor" or "Mainboard"

        // additional checks could come here
        if(error) {
            qDebug() << "Could not get camera temperature reading.";
            wrappedErrorOccured(error);
        } else {
            temperatures[0] = temp;

            temp = arv_camera_get_float(cameras[1], "DeviceTemperature", &error);
            // fallbacks: set "DeviceTemperatureSelector" value to "Sensor" or "Mainboard"

            // additional checks could come here
            if(error) {
                qDebug() << "Could not get camera temperature reading.";
                wrappedErrorOccured(error);
            } else {
                temperatures[1] = temp;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }

    return temperatures;
}

bool StereoCamera::isGrabbing() {
    return isGrabbingV;
}

void StereoCamera::enableSensorLevelBinningIfPossible() {

    // Average BinningMode setting is only possible if the BinningSelector setting is not in Sensor mode
    //  and Sum BinningMode only really has a meaning if Sensor BinningSelector is the case, to compensate
    //  image brightness lost due to smaller area per "pixel", i.e. to retain same brightness even if switched
    //  to binning level 2 or 4 later.
    //  So we set Sum BinningMode only if Sensor BinningSelector is the case, and to
    //  Average BinningMode if BinningSelector is set otherwise.

    GError *error = nullptr;
    try {

        if(arv_device_is_feature_available(arv_camera_get_device(cameras[0]), "BinningSelector", NULL)) {

            if(arv_device_is_feature_available(arv_camera_get_device(cameras[0]), "BinningHorizontalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(cameras[0]), "BinningHorizontalMode", "Sum", &error);
            }
            if(!error && arv_device_is_feature_available(arv_camera_get_device(cameras[0]), "BinningVerticalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(cameras[0]), "BinningVerticalMode", "Sum", &error);
            }

            if(!error) arv_device_set_string_feature_value(arv_camera_get_device(cameras[0]), "BinningSelector", "Sensor", &error);

            if(error) {
                qDebug() << "Could not set sensor level binning.";
                wrappedErrorOccured(error);
            }
        } else {

            if(arv_device_is_feature_available(arv_camera_get_device(cameras[0]), "BinningHorizontalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(cameras[0]), "BinningHorizontalMode", "Average", &error);
            }
            if(!error && arv_device_is_feature_available(arv_camera_get_device(cameras[0]), "BinningVerticalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(cameras[0]), "BinningVerticalMode", "Average", &error);
            }
            if(error) {
                qDebug() << "Could not set Average as Binning Mode, although this should always be possible.";
                wrappedErrorOccured(error);
            }
        }

        //

        if(arv_device_is_feature_available(arv_camera_get_device(cameras[1]), "BinningSelector", NULL)) {

            if(arv_device_is_feature_available(arv_camera_get_device(cameras[1]), "BinningHorizontalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(cameras[1]), "BinningHorizontalMode", "Sum", &error);
            }
            if(!error && arv_device_is_feature_available(arv_camera_get_device(cameras[1]), "BinningVerticalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(cameras[1]), "BinningVerticalMode", "Sum", &error);
            }

            if(!error) arv_device_set_string_feature_value(arv_camera_get_device(cameras[1]), "BinningSelector", "Sensor", &error);

            if(error) {
                qDebug() << "Could not set sensor level binning.";
                wrappedErrorOccured(error);
            }
        } else {

            if(arv_device_is_feature_available(arv_camera_get_device(cameras[1]), "BinningHorizontalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(cameras[1]), "BinningHorizontalMode", "Average", &error);
            }
            if(!error && arv_device_is_feature_available(arv_camera_get_device(cameras[1]), "BinningVerticalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(cameras[1]), "BinningVerticalMode", "Average", &error);
            }
            if(error) {
                qDebug() << "Could not set Average as Binning Mode, although this should always be possible.";
                wrappedErrorOccured(error);
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
}

// NOTE: grabbing "pause" is necessary for setting binning
bool StereoCamera::setBinningVal(int value) {

    bool success = false;

    stopGrabbing();

    GError *error = nullptr;
    try {

        // TODO only do this when opening camera
        enableSensorLevelBinningIfPossible();

        error = nullptr;
        bool canGet = arv_camera_is_binning_available(cameras[0], &error);
        if(!error) canGet &= arv_camera_is_binning_available(cameras[1], &error);
        gint valXMin = 1;
        gint valXMax = 1;
        gint valYMin = 1;
        gint valYMax = 1;
        if(canGet) {
            error = nullptr;
            arv_camera_get_x_binning_bounds(cameras[0], &valXMin, &valXMax, &error);
            if(!error) arv_camera_get_y_binning_bounds(cameras[0], &valYMin, &valYMax, &error);
        } else {
            qDebug() << "Binning value is not avaliable.";
        }
        if(error) {
            qDebug() << "Could not get binning value.";
            wrappedErrorOccured(error);
        } else if( (value <= valXMax && value >= valXMin) && (value <= valYMax && value >= valYMin) ) {

            // TODO: better, find common number of available X and Y binning values (if they might differ)
            arv_camera_set_binning(cameras[0], value, value, &error);
            if(!error) arv_camera_set_binning(cameras[1], value, value, &error);

            resizeStreamBuffer();
            if(error) {
                qDebug() << "Could not set binning value.";
                wrappedErrorOccured(error);
            } else {
                success = true;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
        return false;
    }

    startGrabbing();

    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool StereoCamera::setImageROIwidth(int width) {
    //qDebug() << "Setting Image ROI width=" << std::to_string(width);
    bool success = false;

    stopGrabbing();

    int maxWidth = getImageROIwidthMax();
    int offsetX = getImageROIoffsetX();

    if(width < 16)
        width = 16;

    int modVal = width % getImageROIwidthInc();
    if(modVal != 0)
        width -= modVal;

    int bestWidth = (offsetX+width > maxWidth) ? maxWidth-offsetX-((maxWidth-offsetX) % getImageROIwidthInc()) : width;
//    if (offsetX >= maxWidth-16)
//        width = maxWidth-offsetX;

    //qDebug() << "width = " << width;
    //qDebug() << "maxWidth = " << maxWidth;
    //qDebug() << "offsetX = " << offsetX;
    //qDebug() << "getImageROIwidthMax() = " << getImageROIwidthMax();
    //qDebug() << "getImageROIwidthInc() = " << getImageROIwidthInc();
    //qDebug() << "modVal = " << modVal;
    //qDebug() << "bestWidth = " << bestWidth;

    GError *error = nullptr;
    try {
        auto currentROI = getImageROI();
        arv_camera_set_region(cameras[0], currentROI.x(), currentROI.y(), bestWidth, currentROI.height(), &error);
        if(!error) arv_camera_set_region(cameras[1], currentROI.x(), currentROI.y(), bestWidth, currentROI.height(), &error);

        resizeStreamBuffer();
        if(error) {
            qDebug() << "Could not set image acquisition ROI Width.";
            wrappedErrorOccured(error);
        } else {
            success = true;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
        return false;
    }

    startGrabbing();
    //camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool StereoCamera::setImageROIheight(int height) {
    //qDebug() << "Setting Image ROI height=" << std::to_string(height);
    bool success = false;

    stopGrabbing();

    int maxHeight = getImageROIheightMax();
    int offsetY = getImageROIoffsetY();

    if(height < 16)
        height=16;

    int modVal=height % getImageROIheightInc();
    if(modVal != 0)
        height -= modVal;

    int bestHeight = (offsetY+height > maxHeight) ? maxHeight-offsetY-((maxHeight-offsetY) % getImageROIheightInc()) : height;
//    if (offsetY >= maxHeight-16)
//        height = maxHeight-offsetY;

    GError *error = nullptr;
    try {
        auto currentROI = getImageROI();
        arv_camera_set_region(cameras[0], currentROI.x(), currentROI.y(), currentROI.width(), bestHeight, &error);
        if(!error) arv_camera_set_region(cameras[1], currentROI.x(), currentROI.y(), currentROI.width(), bestHeight, &error);

        resizeStreamBuffer();
        if(error) {
            qDebug() << "Could not set image acquisition ROI Height.";
            wrappedErrorOccured(error);
        } else {
            success = true;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
        return false;
    }

    startGrabbing();
    //camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool StereoCamera::setImageROIoffsetX(int offsetX) {
    //qDebug() << "Setting Image ROI offsetX=" << std::to_string(offsetX);
    bool success = false;

    // NOTE: LIKELY the offsetX and offsetY (unlike height and width) could even be set on most cameras during
    //  grabbing as well, but it is not guaranteed. So yet we simply use the safest solution, stop, then change, then
    //  (re)start grabbing. But if you really want to play around with this to achieve a sliding window ROI or whatever
    //  for highspeed eye detection (the way SMI likely does this anyway), feel free to try. Expo timing could fail btw
    stopGrabbing();

    int maxWidth = getImageROIwidthMax();
    int width = getImageROIwidth();

    if(maxWidth - offsetX < getImageROIoffsetXInc())
        offsetX = maxWidth - getImageROIoffsetXInc();
    //if(width + offsetX > maxWidth)
    //    return;

    int modVal=offsetX % getImageROIoffsetXInc();
    if(modVal != 0)
        offsetX -= modVal;

    if (width + offsetX > maxWidth) {
        return false;
    }

    GError *error = nullptr;
    try {
        auto currentROI = getImageROI();
        arv_camera_set_region(cameras[0], offsetX, currentROI.y(), currentROI.width(), currentROI.height(), &error);
        if(!error) arv_camera_set_region(cameras[1], offsetX, currentROI.y(), currentROI.width(), currentROI.height(), &error);

        resizeStreamBuffer();
        if(error) {
            qDebug() << "Could not set image acquisition ROI OffsetX.";
            wrappedErrorOccured(error);
        } else {
            success = true;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
        return false;
    }

    startGrabbing();
    //camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool StereoCamera::setImageROIoffsetY(int offsetY) {
    //qDebug() << "Setting Image ROI offsetY=" << std::to_string(offsetY);
    bool success = false;

    // NOTE: LIKELY the offsetX and offsetY (unlike height and width) could even be set on most cameras during
    //  grabbing as well, but it is not guaranteed. So yet we simply use the safest solution, stop, then change, then
    //  (re)start grabbing. But if you really want to play around with this to achieve a sliding window ROI or whatever
    //  for highspeed eye detection (the way SMI likely does this anyway), feel free to try. Expo timing could fail btw
    stopGrabbing();

    int maxHeight = getImageROIheightMax();
    int height = getImageROIheight();

    if(maxHeight - offsetY < getImageROIoffsetYInc())
        offsetY = maxHeight - getImageROIoffsetYInc();
    //if(height + offsetY > maxHeight)
    //    return;

    int modVal = offsetY % getImageROIoffsetYInc();
    if(modVal != 0)
        offsetY -= modVal;

    if (height + offsetY > maxHeight) {
        return false;
    }

    GError *error = nullptr;
    try {
        auto currentROI = getImageROI();
        arv_camera_set_region(cameras[0], currentROI.x(), offsetY, currentROI.width(), currentROI.height(), &error);
        if(!error) arv_camera_set_region(cameras[1], currentROI.x(), offsetY, currentROI.width(), currentROI.height(), &error);

        resizeStreamBuffer();
        if(error) {
            qDebug() << "Could not set image acquisition ROI OffsetY.";
            wrappedErrorOccured(error);
        } else {
            success = true;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
        return false;
    }

    startGrabbing();
    //camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
    return success;
}

//bool StereoCamera::setImageROIwidthEmu(int width) {
//    return true;
//    /*
//    if (cameras.GetSize() != 2) {
//        return false;
//    }
//
//    //qDebug() << "Setting both cameras Image ROI width=" << std::to_string(width);
//    bool success = false;
//
//    if(cameras.IsGrabbing())
//        stopGrabbing();
//
//    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
//    int maxWidth = getImageROIwidthMax();
//    int offsetX = getImageROIoffsetX();
//
//    if(width < 16)
//        width=16;
//    int modVal=width%16;
//    if(modVal != 0)
//        width -= modVal;
//    int bestWidth = (offsetX+width > maxWidth) ? maxWidth-offsetX-((maxWidth-offsetX)%16) : width;
////    int bestWidth = (offsetX >= maxWidth-16) ? 16 : width;
//
//    if (cameras[0].Width.IsWritable() && cameras[1].Width.IsWritable() ) {
//        success = cameras[0].Width.TrySetValue(bestWidth) &&
//                  cameras[1].Width.TrySetValue(bestWidth);
//    }
//    startGrabbing();
//    return success;
//    */
//}
//
//// NOTE: grabbing "pause" is necessary for setting image ROI
//bool StereoCamera::setImageROIheightEmu(int height) {
//    return true;
//    /*
//    if (cameras.GetSize() != 2) {
//        return false;
//    }
//
//    //qDebug() << "Setting both cameras Image ROI height=" << std::to_string(height);
//    bool success = false;
//
//    if(cameras.IsGrabbing())
//        stopGrabbing();
//
//    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
//    int maxHeight = getImageROIheightMax();
//    int offsetY = getImageROIoffsetY();
//
//    if(height < 16)
//        height=16;
//    int modVal=height%16;
//    if(modVal != 0)
//        height -= modVal;
//    int bestHeight = (offsetY+height > maxHeight) ? maxHeight-offsetY-((maxHeight-offsetY)%16) : height;
////    int bestHeight = (offsetY >= maxHeight-16) ? 16 : height;
//
//    if (cameras[0].Height.IsWritable() && cameras[1].Height.IsWritable() ) {
//        success = cameras[0].Height.TrySetValue(bestHeight) &&
//                  cameras[1].Height.TrySetValue(bestHeight);
//    }
//    startGrabbing();
//    return success;
//    */
//}
//
//// NOTE: grabbing "pause" is necessary for setting image ROI
//bool StereoCamera::setImageROIoffsetXEmu(int offsetX) {
//    return true;
//    /*
//    if (cameras.GetSize() != 2) {
//        return false;
//    }
//
//    //qDebug() << "Setting Image ROI offsetX=" << std::to_string(offsetX);
//    bool success = false;
//
//    if(cameras.IsGrabbing())
//        stopGrabbing();
//
//    int maxWidth = getImageROIwidthMax();;
//    int width = getImageROIwidth();;
//
//    if(maxWidth - offsetX < 16)
//        offsetX = maxWidth - 16;
//    int modVal=offsetX%16;
//    if(modVal != 0)
//        offsetX -= modVal;
//
//    if (width + offsetX <= maxWidth && cameras[0].OffsetX.IsWritable() && cameras[1].OffsetX.IsWritable() ) {
//        success = cameras[0].OffsetX.TrySetValue(offsetX) &&
//                  cameras[1].OffsetX.TrySetValue(offsetX);
//    }
//    startGrabbing();
//    return success;
//    */
//}
//
//// NOTE: grabbing "pause" is necessary for setting image ROI
//bool StereoCamera::setImageROIoffsetYEmu(int offsetY) {
//    return true;
//    /*
//    if (cameras.GetSize() != 2) {
//        return false;
//    }
//
//    //qDebug() << "Setting Image ROI offsetY=" << std::to_string(offsetY);
//    bool success = false;
//
//    if(cameras.IsGrabbing())
//        stopGrabbing();
//
//    int maxHeight = getImageROIheightMax();
//    int height = getImageROIheight();
//
//    if(maxHeight - offsetY < 16)
//        offsetY = maxHeight - 16;
//    int modVal=offsetY%16;
//    if(modVal != 0)
//        offsetY -= modVal;
//
//    if (height + offsetY <= maxHeight && cameras[0].OffsetY.IsWritable() && cameras[1].OffsetY.IsWritable() ) {
//        success = cameras[0].OffsetY.TrySetValue(offsetY) &&
//                  cameras[1].OffsetY.TrySetValue(offsetY);
//    }
//    startGrabbing();
//    return success;
//    */
//}

/*
void StereoCamera::safelyCloseCameras() {
    if(!cameras.IsOpen())
        return;

    if(cameras.IsGrabbing())
        cameras.StopGrabbing();
    cameras.Close();

    if(cameraImageEventHandler) {
        disconnect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), this,
                   SIGNAL(onNewGrabResult(CameraImage)));
        disconnect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), frameCounter,
                   SLOT(count(CameraImage)));
        //disconnect(cameraImageEventHandler, SIGNAL(needsTimeSynchronization()), this, SLOT(resynchronizeTime()));
        disconnect(cameraImageEventHandler, SIGNAL(imagesSkipped()), this, SIGNAL(imagesSkipped()));
        cameras[0].DeregisterImageEventHandler(cameraImageEventHandler);
        cameras[1].DeregisterImageEventHandler(cameraImageEventHandler);
//        cameraImageEventHandler->DestroyImageEventHandler(); //
        cameraImageEventHandler = nullptr;
    }

    if(cameraConfigurationEventHandler0) {
        disconnect(cameraConfigurationEventHandler0, SIGNAL(cameraDeviceRemoved()), this,
                   SIGNAL(cameraDeviceRemoved()));
        cameras[0].DeregisterConfiguration(hardwareTriggerConfiguration0);
        cameras[0].DeregisterConfiguration(cameraConfigurationEventHandler0);
        hardwareTriggerConfiguration0 = nullptr;
        cameraConfigurationEventHandler0 = nullptr;
    }

    if(cameraConfigurationEventHandler1) {
        disconnect(cameraConfigurationEventHandler1, SIGNAL(cameraDeviceRemoved()), this,
                   SIGNAL(cameraDeviceRemoved()));
        cameras[1].DeregisterConfiguration(hardwareTriggerConfiguration1);
        cameras[1].DeregisterConfiguration(cameraConfigurationEventHandler1);
        hardwareTriggerConfiguration1 = nullptr;
        cameraConfigurationEventHandler1 = nullptr;
    }
}
*/

void StereoCamera::wrappedErrorOccured(GError *error) {

    qDebug() << "Error during aravis API call. Message: " << error->message;

    GValue v = G_VALUE_INIT;
    auto device_type = arv_device_get_type();
    if(device_type == ARV_TYPE_UV_DEVICE) {
        // as far as we know, USB3 devices cannot get in a stuck-with-error state, so (almost) nothing to do here

        // if the device is unplugged this if branch just cannot get control...
        //  whatever. I put the device umplug emit outside then.

    } else if(QString(error->message).contains("access-denied")) {
        // NOTE: if(device_type == ARV_TYPE_GV_DEVICE) does not always work. I dont know why.
        //  We assume it is always GigE if not USB3
        // On GigEVision cameras, any error on the API call can result in control lost over the camera
        //  These special cases need the acquisition be stopped and restarted

        qDebug() << "This is a GigE device, thus needs to be reset now.";
        GError *error2 = nullptr;
        bool isit = arv_device_is_feature_available(arv_camera_get_device(cameras[0]), "DeviceReset", &error2);
        bool device0wasReset = false;
        if(isit) {
            arv_device_execute_command(arv_camera_get_device(cameras[0]), "DeviceReset", &error2);

            device0wasReset = !error2;
            //if(error2) {
            //    emit manualDeviceResetNecessary();
            //} else {
            //    emit deviceWasReset();
            //}
        } else {
            emit manualDeviceResetNecessary();
        }

        isit = arv_device_is_feature_available(arv_camera_get_device(cameras[1]), "DeviceReset", &error2);
        if(isit) {
            arv_device_execute_command(arv_camera_get_device(cameras[1]), "DeviceReset", &error2);

            if(!device0wasReset || error2) {
                emit manualDeviceResetNecessary();
            } else {
                emit deviceWasReset();
            }
        } else {
            emit manualDeviceResetNecessary();
        }
    }

    if(QString(error->message).endsWith("timeout")) {
        // camera is likely unplugged.
        // TODO emit, also close and cleanup (likely top-level initiated already after signal arrived in slot)
        // TODO: check if the device exists if we re-enumerate devices list?

        cameraDeviceRemoved();
    }
}

#endif