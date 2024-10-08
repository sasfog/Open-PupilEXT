
#include "stereoCamera.h"
#include <pylon/TlFactory.h>
#include <QThread>

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

// Creates a stereo camera and attaches the two given Pylon camera device information
StereoCamera::StereoCamera(const CDeviceInfo &diMain, const CDeviceInfo &diSecondary, QObject* parent)
        : StereoCamera(parent) {

    cameras[0].Attach(CTlFactory::GetInstance().CreateDevice(diMain));
    cameras[1].Attach(CTlFactory::GetInstance().CreateDevice(diSecondary));
}

// Creates a stereo camera and attaches the two given Pylon device names (fullnames)
StereoCamera::StereoCamera(const String_t &fullnameMain, const String_t &fullnameSecondary, QObject* parent)
        : StereoCamera(CDeviceInfo().SetFullName(fullnameMain), CDeviceInfo().SetFullName(fullnameSecondary), parent) {

}

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

    cameras[0].Attach(CTlFactory::GetInstance().CreateDevice(diMain));
    cameras[1].Attach(CTlFactory::GetInstance().CreateDevice(diSecondary));

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

        if (cameras[0].CanWaitForFrameTriggerReady() && cameras[1].CanWaitForFrameTriggerReady()) {

            // Start the grabbing using the grab loop thread, by setting the grabLoopType parameter
            // to GrabLoop_ProvidedByInstantCamera. The grab results are delivered to the image event handlers.
            // The GrabStrategy_OneByOne default grab strategy is used.
            startGrabbing();
        } else {
            // See the documentation of CInstantCamera::CanWaitForFrameTriggerReady() for more information.
            std::cout << std::endl;
            std::cout << "Error: This sample can only be used with cameras that can be queried whether they are ready to accept the next frame trigger.";
            std::cout << std::endl;
            std::cout << std::endl;
        }

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

    std::cout << "Camera Synchronize Time" << std::endl << "=========================" << std::endl;
    std::cout << "Timestamp Camera Main: " << cameraMainTime << std::endl;
    std::cout << "Timestamp Camera Secondary: " << cameraSecondaryTime << std::endl;
    std::cout << "Timestamp System: " << systemTime << std::endl;
    std::cout << "System Epoch: " << std::ctime(&epochTime) << std::endl;
    std::cout << "System Time: " << std::ctime(&startTime) << std::endl;
    std::cout << "Time from Epoch (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count() << std::endl;
    std::cout << "Time from Epoch (us): " << std::chrono::duration_cast<std::chrono::microseconds>(start.time_since_epoch()).count() << std::endl;
    std::cout << "Time from Epoch (ns): " << std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count() << std::endl;
    std::cout << "=========================" << std::endl;
}


bool StereoCamera::isOpen() {
    return cameras.IsOpen();
}

// Close the stereo camera and release all Pylon resources
void StereoCamera::close() {

    std::cout << "StereoCamera: Releasing pylon resources.";
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
        }
        if (cameras.GetSize() > 0 && cameras[0].ExposureTimeAbs.IsReadable()) {
            return cameras[0].ExposureTimeAbs.GetValue();
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
        }
        if (cameras.GetSize() > 0 && cameras[0].ExposureTimeAbs.IsReadable()) {
            return cameras[0].ExposureTimeAbs.GetMin();
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
        }
        if (cameras.GetSize() > 0 && cameras[0].ExposureTimeAbs.IsReadable()) {
            return cameras[0].ExposureTimeAbs.GetMax();
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
        }
        if (cameras.GetSize() > 0 && cameras[0].GainRaw.IsReadable()) {
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
        }
        if (cameras.GetSize() > 0 && cameras[0].GainRaw.IsReadable()) {
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
        }
        if (cameras.GetSize() > 0 && cameras[0].GainRaw.IsReadable()) {
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
        if (isEmulated()) {
            if (cameras.GetSize() == 2 && cameras[0].GainRaw.IsWritable() && cameras[1].GainRaw.IsWritable() &&
                value >= getGainMin() && value <= getGainMax()) {
                qDebug() << cameras[0].GainRaw.GetMin();
                qDebug() << cameras[1].GainRaw.GetMin();
                qDebug() << cameras[0].GainRaw.GetMax();
                qDebug() << cameras[1].GainRaw.GetMax();
                int intValue = static_cast<int>(value);
                cameras[0].GainRaw.TrySetValue(intValue);
                cameras[1].GainRaw.TrySetValue(intValue);
            }
        } else {
            if (cameras[0].Gain.IsReadable() && cameras[1].Gain.IsReadable()) {
                if (getGainMax() < value)
                    value = getGainMax();
                else if (getGainMin() > value)
                    value = getGainMin();
            }
            if (cameras.GetSize() == 2 && cameras[0].Gain.IsWritable() && cameras[1].Gain.IsWritable()) {
                cameras[0].Gain.TrySetValue(value);
                cameras[1].Gain.TrySetValue(value);
            }
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }

}

// Sets the exposure time value of the main and secondary camera
void StereoCamera::setExposureTimeValue(int value) {
    try {
        if (isEmulated()) {
            if (cameras.GetSize() == 2 && cameras[0].ExposureTimeAbs.IsWritable() &&
                cameras[1].ExposureTimeAbs.IsWritable() && value != 0) {
                std::cout << "Writing exposure value: " << value << std::endl;
                cameras[0].ExposureTimeAbs.TrySetValue(value);
                cameras[1].ExposureTimeAbs.TrySetValue(value);
            }
        } else {
            if (cameras[0].ExposureTime.IsReadable() && cameras[1].ExposureTime.IsReadable()) {
                if (getExposureTimeMax() < value)
                    value = getExposureTimeMax();
                else if (getExposureTimeMin() > value)
                    value = getExposureTimeMin();
            }
            if (cameras.GetSize() == 2 && cameras[0].ExposureTime.IsWritable() &&
                cameras[1].ExposureTime.IsWritable()) {
                std::cout << "Writing exposure value: " << value << std::endl;
                cameras[0].ExposureTime.TrySetValue(value);
                cameras[1].ExposureTime.TrySetValue(value);
            }
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

// Loads the camera settings of the main camera and sets its values to both the main and secondary camera
// Reads the Basler specific camera setting file format
// Camera settings are automatically saved in the applications settings directory (Path visible in the about window)
void StereoCamera::loadMainFromFile(const String_t &filename) {

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
}

// Saves the main camera settings to file
// Uses the Basler specific file format
void StereoCamera::saveMainToFile(const String_t &filename) {
    try {
        CFeaturePersistence::Save(filename, &cameras[0].GetNodeMap());
    } catch (const GenericException &e) {
        // Error handling.
        std::cerr << "An exception occurred: " << e.GetDescription() << std::endl;
    }
}

// Reads if a image acquisition frame rate is enabled
// The value of the image acquisition may overwrite hardware trigger framerates
// Assumes that main and secondary camera have the same settings
bool StereoCamera::isEnabledAcquisitionFrameRate() {
    if (cameras.GetSize() > 0 && cameras[0].AcquisitionFrameRateEnable.IsReadable()) {
        return cameras[0].AcquisitionFrameRateEnable.GetValue();
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
        if (cameras.GetSize() == 2 && cameras[0].AcquisitionFrameRateEnable.IsWritable() &&
            cameras[1].AcquisitionFrameRateEnable.IsWritable()) {
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
        if (isEmulated()) {
            if (cameras.GetSize() == 2 && cameras[0].AcquisitionFrameRateAbs.IsWritable() &&
                cameras[1].AcquisitionFrameRateAbs.IsWritable()) {
                if (value <= 0)
                    value = 10;
                cameras[0].AcquisitionFrameRateAbs.TrySetValue(value);
                cameras[1].AcquisitionFrameRateAbs.TrySetValue(value);
            }
        } else {
            if (cameras.GetSize() == 2 && cameras[0].AcquisitionFrameRate.IsWritable() &&
                cameras[1].AcquisitionFrameRate.IsWritable()) {
                cameras[0].AcquisitionFrameRate.TrySetValue(value);
                cameras[1].AcquisitionFrameRate.TrySetValue(value);
            }
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
        }
        if (cameras.GetSize() > 0 && cameras[0].AcquisitionFrameRateAbs.IsReadable()) {
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
        }
        if (cameras.GetSize() > 0 && cameras[0].AcquisitionFrameRateAbs.IsReadable()) {
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
        }
        if (cameras.GetSize() > 0 && cameras[0].ResultingFrameRateAbs.IsReadable()) {
            return cameras[0].ResultingFrameRateAbs.GetValue();
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

                std::cout << "Starting AutoGain..." << std::endl;

                //camera.Gain.SetToMaximum();
                cameras[0].Gain.TrySetToMaximum();


                if (!cameras[0].GainAuto.IsWritable()) {
                    std::cout << "The camera does not support Gain Auto." << std::endl;
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

                    cameras[0].AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI1);
                    cameras[0].AutoFunctionROIUseBrightness.TrySetValue(true);   // ROI 1 is used for brightness control
                    cameras[0].AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI2);
                    cameras[0].AutoFunctionROIUseBrightness.TrySetValue(false);   // ROI 2 is not used for brightness control

                    // Set the ROI (in this example the complete sensor is used)
                    cameras[0].AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI1);  // configure ROI 1
                    cameras[0].AutoFunctionROIOffsetX.SetValue(cameras[0].OffsetX.GetMin());
                    cameras[0].AutoFunctionROIOffsetY.SetValue(cameras[0].OffsetY.GetMin());
                    cameras[0].AutoFunctionROIWidth.SetValue(cameras[0].Width.GetMax());
                    cameras[0].AutoFunctionROIHeight.SetValue(cameras[0].Height.GetMax());
                }

                if (cameras[0].GetSfncVersion() >= Sfnc_2_0_0) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    // Set the target value for luminance control.
                    // A value of 0.3 means that the target brightness is 30 % of the maximum brightness of the raw pixel value read out from the sensor.
                    // A value of 0.4 means 40 % and so forth.
                    cameras[0].AutoTargetBrightness.SetValue(0.3);

                    // We are going to try GainAuto = Once.

                    std::cout << "Trying 'GainAuto = Once'." << std::endl;
                    std::cout << "Initial Gain = " << cameras[0].Gain.GetValue() << std::endl;

                    // Set the gain ranges for luminance control.
                    cameras[0].AutoGainLowerLimit.SetValue(cameras[0].Gain.GetMin());
                    cameras[0].AutoGainUpperLimit.SetValue(cameras[0].Gain.GetMax());
                }

                cameras[0].GainAuto.SetValue(GainAuto_Once);

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

                std::cout << "GainAuto went back to 'Off' after " << n << " frames." << std::endl;
                if(cameras[0].Gain.IsReadable()) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    std::cout << "Final Gain = " << cameras[0].Gain.GetValue() << std::endl;
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
                cameras[0].ExposureTime.SetToMinimum();
                // Carry out luminance control by using the "once" exposure auto function.


                if (!cameras[0].ExposureAuto.IsWritable())
                {
                    std::cout << "The camera does not support Exposure Auto." << std::endl;
                    return;
                }

                // Maximize the grabbed area of interest (Image AOI).
                cameras[0].OffsetX.TrySetToMinimum();
                cameras[0].OffsetY.TrySetToMinimum();
                cameras[0].Width.SetToMaximum();
                cameras[0].Height.SetToMaximum();

                if (cameras[0].AutoFunctionROISelector.IsWritable())
                {
                    // Set the Auto Function ROI for luminance statistics.
                    // We want to use ROI1 for gathering the statistics
                    cameras[0].AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI1);
                    cameras[0].AutoFunctionROIUseBrightness.TrySetValue(true);   // ROI 1 is used for brightness control
                    cameras[0].AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI2);
                    cameras[0].AutoFunctionROIUseBrightness.TrySetValue(false);   // ROI 2 is not used for brightness control

                    // Set the ROI (in this example the complete sensor is used)
                    cameras[0].AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI1);  // configure ROI 1
                    cameras[0].AutoFunctionROIOffsetX.SetValue(cameras[0].OffsetX.GetMin());
                    cameras[0].AutoFunctionROIOffsetY.SetValue(cameras[0].OffsetY.GetMin());
                    cameras[0].AutoFunctionROIWidth.SetValue(cameras[0].Width.GetMax());
                    cameras[0].AutoFunctionROIHeight.SetValue(cameras[0].Height.GetMax());
                }

                if (cameras[0].GetSfncVersion() >= Sfnc_2_0_0) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    // Set the target value for luminance control.
                    // A value of 0.3 means that the target brightness is 30 % of the maximum brightness of the raw pixel value read out from the sensor.
                    // A value of 0.4 means 40 % and so forth.
                    cameras[0].AutoTargetBrightness.SetValue(0.3);

                    // Try ExposureAuto = Once.
                    std::cout << "Trying 'ExposureAuto = Once'." << std::endl;
                    std::cout << "Initial exposure time = ";
                    std::cout << cameras[0].ExposureTime.GetValue() << " us" << std::endl;

                    // Set the exposure time ranges for luminance control.
                    cameras[0].AutoExposureTimeLowerLimit.SetValue(cameras[0].AutoExposureTimeLowerLimit.GetMin());
                    cameras[0].AutoExposureTimeUpperLimit.SetValue(cameras[0].AutoExposureTimeLowerLimit.GetMax());

                    cameras[0].ExposureAuto.SetValue(ExposureAuto_Once);
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

                std::cout << "ExposureAuto went back to 'Off' after " << n << " frames." << std::endl;
                std::cout << "Final exposure time = ";

                if (cameras[0].ExposureTime.IsReadable()) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    std::cout << cameras[0].ExposureTime.GetValue() << " us" << std::endl;
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
String_t StereoCamera::getLineSource() {
    return lineSource;
}

// Sets the linesource used as the hardware trigger source
// The same linesource is used for both cameras
// When the linesource changes, the camera must be closed and opened again
void StereoCamera::setLineSource(String_t value) {
    lineSource = value;
}

CameraImageType StereoCamera::getType() {
    return CameraImageType::LIVE_STEREO_CAMERA;
}

void StereoCamera::startGrabbing()
{
    if (cameras.IsOpen() && !cameras.IsGrabbing()) {
        cameras.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
    }
}

void StereoCamera::stopGrabbing()
{
    if (cameras.IsOpen() && cameras.IsGrabbing())
        cameras.StopGrabbing();
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
        std::cout << "Found calibration file in settings directory. Loading: " << configFile.toStdString()
                  << std::endl;
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
            std::cout << "Image acquisition ROI height of the two cameras are not the same. Now resetting both to the lower value." << std::endl;
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
            std::cout << "Image acquisition ROI offsetX of the two cameras are not the same. Now resetting both to the lower value." << std::endl;
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

int StereoCamera::getImageROIoffsetY() {
    try {
        if (cameras.GetSize() != 2 || !cameras[0].OffsetY.IsReadable() || !cameras[1].OffsetY.IsReadable()) {
            return 0;
        }

        int val0 = (int) cameras[0].OffsetY.GetValue();
        int val1 = (int) cameras[1].OffsetY.GetValue();
        if (val0 != val1) {
            std::cout << "Image acquisition ROI offsetY of the two cameras are not the same. Now resetting both to the lower value." << std::endl;
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
            std::cout << "Image acquisition ROI max width of the two cameras are not the same. Now using the lower (safer) value." << std::endl;
            if (val0 > val1)
                val0 = val1;
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
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
            std::cout << "Image acquisition ROI max height of the two cameras are not the same. Now using the lower (safer) value." << std::endl;
            if (val0 > val1)
                val0 = val1;
        }
        return val0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

QRectF StereoCamera::getImageROI(){
    return QRectF(getImageROIoffsetX(),getImageROIoffsetY(),getImageROIwidth(), getImageROIheight());
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
            std::cout << "Image acquisition horizontal binning of the two cameras are not the same. Now resetting both to the lower value." << std::endl;
            setBinningVal(b0);
        }
        return b0;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 1;
}

std::vector<double> StereoCamera::getTemperatures() {
    std::vector<double> temperatures = {0.0, 0.0};
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
        cameras[0].DeviceTemperatureSelector.SetValue(
                Basler_UniversalCameraParams::DeviceTemperatureSelectorEnums::DeviceTemperatureSelector_Coreboard);
        cameras[1].DeviceTemperatureSelector.SetValue(
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

// NOTE: grabbing "pause" is necessary for setting binning
bool StereoCamera::setBinningVal(int value) {
    if (cameras.GetSize() != 2 || !cameras[0].BinningHorizontal.IsReadable() || !cameras[1].BinningHorizontal.IsReadable()) {
        return false;
    }
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    // in case of our Basler cameras here, only mode=1,2,4 are only valid values
    if (cameras[0].BinningHorizontal.IsWritable() && cameras[0].BinningVertical.IsWritable() &&
        cameras[1].BinningHorizontal.IsWritable() && cameras[1].BinningVertical.IsWritable()) {
        // "Enable sensor binning"
        // "Note: Available on selected camera models only"
       // camera.BinningSelector.SetValue(BinningSelector_Sensor); // NOTE: found in Basler docs, but no trace of it in Pylon::CBaslerUniversalInstantCamera:: when code tries to compile. What is this?

        // Set "binning mode" of camera
        cameras[0].BinningHorizontalMode.TrySetValue(BinningHorizontalMode_Average);
        //cameras[0].BinningHorizontalMode.SetValue(BinningHorizontalMode_Sum);
        cameras[0].BinningVerticalMode.TrySetValue(BinningVerticalMode_Average);
        //cameras[0].BinningVerticalMode.SetValue(BinningHorizontalMode_Sum);
        //
        cameras[1].BinningHorizontalMode.TrySetValue(BinningHorizontalMode_Average);
        //cameras[1].BinningHorizontalMode.SetValue(BinningHorizontalMode_Sum);
        cameras[1].BinningVerticalMode.TrySetValue(BinningVerticalMode_Average);
        //cameras[1].BinningVerticalMode.SetValue(BinningHorizontalMode_Sum);

        if(value==2 || value==3) {
            success = cameras[0].BinningHorizontal.TrySetValue(2) &&
                cameras[0].BinningVertical.TrySetValue(2) &&
                cameras[1].BinningHorizontal.TrySetValue(2) &&
                cameras[1].BinningVertical.TrySetValue(2);
            std::cout << "Setting binning to 2 on both axes"<< std::endl;
        } else if(value==4) {
            success = cameras[0].BinningHorizontal.TrySetValue(4) &&
                cameras[0].BinningVertical.TrySetValue(4) &&
                cameras[1].BinningHorizontal.TrySetValue(4) &&
                cameras[1].BinningVertical.TrySetValue(4);
            std::cout << "Setting binning to 4 on both axes"<< std::endl;
        } else { //if(value==1) {
            success = cameras[0].BinningHorizontal.TrySetValue(1) &&
                cameras[0].BinningVertical.TrySetValue(1) &&
                cameras[1].BinningHorizontal.TrySetValue(1) &&
                cameras[1].BinningVertical.TrySetValue(1);
            std::cout << "Setting binning to 1 (no binning) on both axes"<< std::endl;
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

    //std::cout << "Setting both cameras Image ROI width=" << std::to_string(width) << std::endl;
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
    int maxWidth = getImageROIwidthMax();
    int offsetX = getImageROIoffsetX();

    if(width < 16)
        width=16;
    int modVal=width%16;
    if(modVal != 0)
        width -= modVal;
    int bestWidth = (offsetX+width > maxWidth) ? maxWidth-offsetX-((maxWidth-offsetX)%16) : width;
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

    //std::cout << "Setting both cameras Image ROI height=" << std::to_string(height) << std::endl;
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
    int maxHeight = getImageROIheightMax();
    int offsetY = getImageROIoffsetY();

    if(height < 16)
        height=16;
    int modVal=height%16;
    if(modVal != 0)
        height -= modVal;
    int bestHeight = (offsetY+height > maxHeight) ? maxHeight-offsetY-((maxHeight-offsetY)%16) : height;
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

    //std::cout << "Setting Image ROI offsetX=" << std::to_string(offsetX) << std::endl;
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
    int maxWidth = getImageROIwidthMax();
    int width = getImageROIwidth();

    if(maxWidth - offsetX < 16)
        offsetX = maxWidth - 16;
    int modVal=offsetX%16;
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
    
    //std::cout << "Setting Image ROI offsetY=" << std::to_string(offsetY) << std::endl;
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
    int maxHeight = getImageROIheightMax();;
    int height = getImageROIheight();;

    if(maxHeight - offsetY < 16)
        offsetY = maxHeight - 16;
    int modVal=offsetY%16;
    if(modVal != 0)
        offsetY -= modVal;

    if (height + offsetY <= maxHeight && cameras[0].OffsetY.IsWritable() && cameras[1].OffsetY.IsWritable() ) {
        success = cameras[0].OffsetY.TrySetValue(offsetY) &&
            cameras[1].OffsetY.TrySetValue(offsetY);
    }
    startGrabbing();
    return success;
}

bool StereoCamera::setImageROIwidthEmu(int width) {
    if (cameras.GetSize() != 2) {
        return false;
    }

    //std::cout << "Setting both cameras Image ROI width=" << std::to_string(width) << std::endl;
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
    int maxWidth = getImageROIwidthMax();
    int offsetX = getImageROIoffsetX();

    if(width < 16)
        width=16;
    int modVal=width%16;
    if(modVal != 0)
        width -= modVal;
    int bestWidth = (offsetX+width > maxWidth) ? maxWidth-offsetX-((maxWidth-offsetX)%16) : width;
//    int bestWidth = (offsetX >= maxWidth-16) ? 16 : width;

    if (cameras[0].Width.IsWritable() && cameras[1].Width.IsWritable() ) {
        success = cameras[0].Width.TrySetValue(bestWidth) &&
            cameras[1].Width.TrySetValue(bestWidth);
    }
    startGrabbing();
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool StereoCamera::setImageROIheightEmu(int height) {
    if (cameras.GetSize() != 2) {
        return false;
    }

    //std::cout << "Setting both cameras Image ROI height=" << std::to_string(height) << std::endl;
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    getBinningVal(); // Call just to reset binning if they do not match for the 2 cameras
    int maxHeight = getImageROIheightMax();
    int offsetY = getImageROIoffsetY();

    if(height < 16)
        height=16;
    int modVal=height%16;
    if(modVal != 0)
        height -= modVal;
    int bestHeight = (offsetY+height > maxHeight) ? maxHeight-offsetY-((maxHeight-offsetY)%16) : height;
//    int bestHeight = (offsetY >= maxHeight-16) ? 16 : height;

    if (cameras[0].Height.IsWritable() && cameras[1].Height.IsWritable() ) {
        success = cameras[0].Height.TrySetValue(bestHeight) &&
            cameras[1].Height.TrySetValue(bestHeight);
    }
    startGrabbing();
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool StereoCamera::setImageROIoffsetXEmu(int offsetX) {
    if (cameras.GetSize() != 2) {
        return false;
    }

    //std::cout << "Setting Image ROI offsetX=" << std::to_string(offsetX) << std::endl;
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    int maxWidth = getImageROIwidthMax();;
    int width = getImageROIwidth();;

    if(maxWidth - offsetX < 16)
        offsetX = maxWidth - 16;
    int modVal=offsetX%16;
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
bool StereoCamera::setImageROIoffsetYEmu(int offsetY) {
    if (cameras.GetSize() != 2) {
        return false;
    }
    
    //std::cout << "Setting Image ROI offsetY=" << std::to_string(offsetY) << std::endl;
    bool success = false;

    if(cameras.IsGrabbing())
        stopGrabbing();

    int maxHeight = getImageROIheightMax();
    int height = getImageROIheight();

    if(maxHeight - offsetY < 16)
        offsetY = maxHeight - 16;
    int modVal=offsetY%16;
    if(modVal != 0)
        offsetY -= modVal;

    if (height + offsetY <= maxHeight && cameras[0].OffsetY.IsWritable() && cameras[1].OffsetY.IsWritable() ) {
        success = cameras[0].OffsetY.TrySetValue(offsetY) &&
            cameras[1].OffsetY.TrySetValue(offsetY);
    }
    startGrabbing();
    return success;
}

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
