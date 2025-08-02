
#include "singleCamera.h"
#include <QThread>

#ifdef USE_PYLON

#include <pylon/TlFactory.h>

SingleCamera::SingleCamera(const QString &friendlyName, QObject* parent)
        : Camera(parent),
        frameCounter(new CameraFrameRateCounter(parent)),
        cameraCalibration(new CameraCalibration()),
        calibrationThread(new QThread()),
        hardwareTriggerEnabled(false),
        lineSource("Line1") {

    // TODO: LOOKUP CAMERA

    CTlFactory& TlFactory = CTlFactory::GetInstance();
    IGigETransportLayer* pTl = dynamic_cast<IGigETransportLayer*>(TlFactory.CreateTl( Pylon::BaslerGigEDeviceClass ));

    Pylon::DeviceInfoList_t allDevices;
    Pylon::DeviceInfoList_t lstDevices;
    TlFactory.EnumerateDevices(lstDevices);

    if (pTl == NULL) {
        qDebug() << "Error: No GigE transport layer installed.";
        qDebug() << "       Please install GigE support as it is required for this sample.";
        //return {};
        allDevices = lstDevices;
    } else {
        Pylon::DeviceInfoList_t lstDevicesGigE;
        pTl->EnumerateAllDevices(lstDevicesGigE);
        std::merge(lstDevices.begin(), lstDevices.end(), lstDevicesGigE.begin(), lstDevicesGigE.end(), std::back_inserter(allDevices));
    }

    //Pylon::DeviceInfoList_t lstDevices = enumerateCameraDevices();
    if(allDevices.empty()) {
        throw new std::exception("Camera connection problem.");
        //return;
    }
    bool cfound = false;
    CDeviceInfo di;
    Pylon::DeviceInfoList_t::const_iterator deviceIt;
    for (deviceIt = allDevices.begin(); deviceIt != allDevices.end(); ++deviceIt) {
        qDebug() << "deviceIt->GetFriendlyName().c_str() " << deviceIt->GetFriendlyName().c_str();
        if(deviceIt->GetFriendlyName().c_str() == friendlyName) {
            //qDebug() << "FOUND";
            cfound = true;
            di = *deviceIt;
            break;
        }
    }
    if(!cfound) {
        throw new std::exception("The specified camera was not found among the ones currently detected.");
    }

    //auto di = CDeviceInfo().SetFriendlyName(friendlyName.toStdString().c_str());

    camera.Attach(TlFactory.CreateDevice(di));
    //camera = TlFactory.CreateDevice(di);

    settingsDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    if(!settingsDirectory.exists()) {
// mkdir(".") DOES NOT WORK ON MACOS, ONLY WINDOWS. (Reported on MacOS 12.7.6 and Windows 10)
//        settingsDirectory.mkdir(".");
        QDir().mkpath(settingsDirectory.absolutePath());
    }

    // calibration worker thread
    cameraCalibration->moveToThread(calibrationThread);
    calibrationThread->start();
    calibrationThread->setPriority(QThread::HighPriority);

    connect(frameCounter, SIGNAL(fps(double)), this, SIGNAL(fps(double)));
    connect(frameCounter, SIGNAL(framecount(int)), this, SIGNAL(framecount(int)));

    if(camera.IsOpen()) {
        camera.Close();
    }

    try {
        cameraConfigurationEventHandler = new CameraConfigurationEventHandler;
        connect(cameraConfigurationEventHandler, SIGNAL(cameraDeviceRemoved()), this, SIGNAL(cameraDeviceRemoved()));
        camera.RegisterConfiguration(cameraConfigurationEventHandler, RegistrationMode_ReplaceAll, Cleanup_Delete);

        softwareTriggerConfiguration = new CAcquireContinuousConfiguration;
        camera.RegisterConfiguration(softwareTriggerConfiguration, RegistrationMode_Append, Cleanup_Delete);

        cameraImageEventHandler = new SingleCameraImageEventHandler(parent);
        connect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), this, SIGNAL(onNewGrabResult(CameraImage)));
        connect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), frameCounter, SLOT(count(CameraImage)));
        //
        connect(cameraImageEventHandler, SIGNAL(imagesSkipped()), this, SIGNAL(imagesSkipped()));

        camera.RegisterImageEventHandler(cameraImageEventHandler, RegistrationMode_Append, Cleanup_Delete);

        camera.Open();
        assert(camera.IsOpen());

        synchronizeTime();
        cameraImageEventHandler->setTimeSynchronization(cameraTime, systemTime);

        camera.PixelFormat.SetValue(PixelFormat_Mono8);

        // load calibration if existing
        if(!cameraCalibration->isCalibrated()) {
            // If we already used this camera before, a config file may exists
            loadCalibrationFile();
        }

        CIntegerParameter heartbeat( camera.GetTLNodeMap(), "HeartbeatTimeout" );
        heartbeat.TrySetValue( 1000, IntegerValueCorrection_Nearest );

        if(camera.CanWaitForFrameTriggerReady()) {

            // Start the grabbing using the grab loop thread, by setting the grabLoopType parameter
            // to GrabLoop_ProvidedByInstantCamera. The grab results are delivered to the image event handlers.
            // The GrabStrategy_OneByOne default grab strategy is used.
            camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
        } else {
            // See the documentation of CInstantCamera::CanWaitForFrameTriggerReady() for more information.
            std::cout << std::endl;
            std::cout << "Error: This sample can only be used with cameras that can be queried whether they are ready to accept the next frame trigger.";
            std::cout << std::endl;
            std::cout << std::endl;
        }
    }
    catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

SingleCamera::~SingleCamera() {
    if (cameraCalibration != nullptr)
        cameraCalibration->deleteLater();
    if (calibrationThread != nullptr) {
        calibrationThread->quit();
        calibrationThread->deleteLater();
    }
}

void SingleCamera::genericExceptionOccured(const GenericException &e) {
    //QThread::msleep(1000);
    std::cerr << "A Pylon exception occurred." << std::endl<< e.GetDescription() << std::endl;
    if (camera.IsCameraDeviceRemoved()) {
        emit cameraDeviceRemoved();
        camera.Close();
        camera.DetachDevice();
        camera.DestroyDevice();
    }
}

bool SingleCamera::isOpen() {
    return camera.IsOpen();
}

void SingleCamera::close() {
    std::cout << "SingleCamera: Releasing pylon resources.";
    camera.StopGrabbing();
    camera.Close();
    camera.DeregisterImageEventHandler(cameraImageEventHandler);
    camera.DeregisterConfiguration(cameraConfigurationEventHandler);
    if(hardwareTriggerConfiguration) {
        camera.DeregisterConfiguration(hardwareTriggerConfiguration);
    }
    if(softwareTriggerConfiguration) {
        camera.DeregisterConfiguration(softwareTriggerConfiguration);
    }
}

void SingleCamera::enableHardwareTrigger(bool state) {
    std::cout<< "SingleCamera: Enabling Hardware trigger to line source: " + lineSource << " to state: " << state << std::endl;

    frameCounter->reset();

    try {

        if(camera.IsOpen()) {
            camera.StopGrabbing();
            camera.Close();
        }


        if(hardwareTriggerConfiguration) {
            camera.DeregisterConfiguration(hardwareTriggerConfiguration);
            hardwareTriggerConfiguration = nullptr;
        }
        if(softwareTriggerConfiguration) {
            camera.DeregisterConfiguration(softwareTriggerConfiguration);
            softwareTriggerConfiguration = nullptr;
        }

        if(state) {
            hardwareTriggerConfiguration = new HardwareTriggerConfiguration(lineSource);
            camera.RegisterConfiguration(hardwareTriggerConfiguration, RegistrationMode_Append, Cleanup_Delete);
            hardwareTriggerEnabled = true;
        } else {
            softwareTriggerConfiguration = new CAcquireContinuousConfiguration;
            camera.RegisterConfiguration(softwareTriggerConfiguration, RegistrationMode_Append, Cleanup_Delete);
            hardwareTriggerEnabled = false;
        }

        camera.Open();

        // DEV
        // TODO: This guy is causing a lot of trouble.
        try {
            synchronizeTime();
            cameraImageEventHandler->setTimeSynchronization(cameraTime, systemTime);
        } catch (const GenericException &e) {
            qDebug() << e.GetDescription();
            //genericExceptionOccured(e);
        }

        if (camera.CanWaitForFrameTriggerReady()) {

            // Start the grabbing using the grab loop thread, by setting the grabLoopType parameter
            // to GrabLoop_ProvidedByInstantCamera. The grab results are delivered to the image event handlers.
            // The GrabStrategy_OneByOne default grab strategy is used.
            camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
        } else {
            // See the documentation of CInstantCamera::CanWaitForFrameTriggerReady() for more information.
            std::cout << std::endl;
            std::cout << "Error: This sample can only be used with cameras that can be queried whether they are ready to accept the next frame trigger.";
            std::cout << std::endl;
            std::cout << std::endl;
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

/*
 * Taken from the Basler C++ Documentation Examples
 */
void SingleCamera::autoGainOnce() {
    try {

        if(!camera.IsOpen()) {
            camera.Open();
        }

        camera.StopGrabbing();

        // Turn test image off.
        camera.TestImageSelector.TrySetValue(TestImageSelector_Off);
        camera.TestPattern.TrySetValue(TestPattern_Off);

        // Only area scan cameras support auto functions.
        if (camera.DeviceScanType.GetValue() == DeviceScanType_Areascan) {
            // Cameras based on SFNC 2.0 or later, e.g., USB cameras
            if (camera.GetSfncVersion() >= Sfnc_2_0_0) {
                // All area scan cameras support luminance control.
                // Carry out luminance control by using the "once" gain auto function.
                // For demonstration purposes only, set the gain to an initial value.

                std::cout << "Starting AutoGain..." << std::endl;

                //camera.Gain.SetToMaximum();
                //camera.Gain.TrySetToMaximum();

                if (!camera.GainAuto.IsWritable()) {
                    std::cout << "The camera does not support Gain Auto." << std::endl;
                    return;
                }

                // Maximize the grabbed image area of interest (Image AOI).
                camera.OffsetX.TrySetToMinimum();
                camera.OffsetY.TrySetToMinimum();
                camera.Width.TrySetToMaximum();
                camera.Height.TrySetToMaximum();

                if (camera.AutoFunctionROISelector.IsWritable()) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    // Set the Auto Function ROI for luminance statistics.
                    // We want to use ROI1 for gathering the statistics

                    camera.AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI1);
                    camera.AutoFunctionROIUseBrightness.TrySetValue(true);   // ROI 1 is used for brightness control
                    camera.AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI2);
                    camera.AutoFunctionROIUseBrightness.TrySetValue(false);   // ROI 2 is not used for brightness control

                    // Set the ROI (in this example the complete sensor is used)
                    camera.AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI1);  // configure ROI 1
                    camera.AutoFunctionROIOffsetX.SetValue(camera.OffsetX.GetMin());
                    camera.AutoFunctionROIOffsetY.SetValue(camera.OffsetY.GetMin());
                    camera.AutoFunctionROIWidth.SetValue(camera.Width.GetMax());
                    camera.AutoFunctionROIHeight.SetValue(camera.Height.GetMax());
                }

                if (camera.GetSfncVersion() >= Sfnc_2_0_0) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    // Set the target value for luminance control.
                    // A value of 0.3 means that the target brightness is 30 % of the maximum brightness of the raw pixel value read out from the sensor.
                    // A value of 0.4 means 40 % and so forth.
                    camera.AutoTargetBrightness.SetValue(0.3);

                    // We are going to try GainAuto = Once.

                    std::cout << "Trying 'GainAuto = Once'." << std::endl;
                    std::cout << "Initial Gain = " << camera.Gain.GetValue() << std::endl;

                    // Set the gain ranges for luminance control.
                    camera.AutoGainLowerLimit.SetValue(camera.Gain.GetMin());
                    camera.AutoGainUpperLimit.SetValue(camera.Gain.GetMax());
                }

                camera.GainAuto.SetValue(GainAuto_Once);

                // When the "once" mode of operation is selected,
                // the parameter values are automatically adjusted until the related image property
                // reaches the target value. After the automatic parameter value adjustment is complete, the auto
                // function will automatically be set to "off" and the new parameter value will be applied to the
                // subsequently grabbed images.

                int n = 0;
                while (camera.GainAuto.GetValue() != GainAuto_Off) {
                    CBaslerUniversalGrabResultPtr ptrGrabResult;
                    camera.GrabOne( 5000, ptrGrabResult);
                    ++n;
                    //Make sure the loop is exited.
                    if (n > 100) {
                        throw TIMEOUT_EXCEPTION( "The adjustment of auto gain did not finish.");
                    }
                }

                std::cout << "GainAuto went back to 'Off' after " << n << " frames." << std::endl;
                if(camera.Gain.IsReadable()) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    std::cout << "Final Gain = " << camera.Gain.GetValue() << std::endl;
                }

                camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
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

/*
 * Taken from the Basler C++ Documentation Examples
 */
void SingleCamera::autoExposureOnce() {
    try {

        if(!camera.IsOpen()) {
            camera.Open();
        }

        camera.StopGrabbing();

        // Turn test image off.
        camera.TestImageSelector.TrySetValue(TestImageSelector_Off);
        camera.TestPattern.TrySetValue(TestPattern_Off);

        // Only area scan cameras support auto functions.
        if (camera.DeviceScanType.GetValue() == DeviceScanType_Areascan) {
            // Cameras based on SFNC 2.0 or later, e.g., USB cameras
            if (camera.GetSfncVersion() >= Sfnc_2_0_0) {
                // For demonstration purposes only, set the exposure time to an initial value.
                //camera.ExposureTime.SetToMinimum();
                // Carry out luminance control by using the "once" exposure auto function.


                if (!camera.ExposureAuto.IsWritable())
                {
                    std::cout << "The camera does not support Exposure Auto." << std::endl;
                    return;
                }

                // Maximize the grabbed area of interest (Image AOI).
                camera.OffsetX.TrySetToMinimum();
                camera.OffsetY.TrySetToMinimum();
                camera.Width.SetToMaximum();
                camera.Height.SetToMaximum();

                if (camera.AutoFunctionROISelector.IsWritable())
                {
                    // Set the Auto Function ROI for luminance statistics.
                    // We want to use ROI1 for gathering the statistics
                    camera.AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI1);
                    camera.AutoFunctionROIUseBrightness.TrySetValue(true);   // ROI 1 is used for brightness control
                    camera.AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI2);
                    camera.AutoFunctionROIUseBrightness.TrySetValue(false);   // ROI 2 is not used for brightness control

                    // Set the ROI (in this example the complete sensor is used)
                    camera.AutoFunctionROISelector.SetValue(AutoFunctionROISelector_ROI1);  // configure ROI 1
                    camera.AutoFunctionROIOffsetX.SetValue(camera.OffsetX.GetMin());
                    camera.AutoFunctionROIOffsetY.SetValue(camera.OffsetY.GetMin());
                    camera.AutoFunctionROIWidth.SetValue(camera.Width.GetMax());
                    camera.AutoFunctionROIHeight.SetValue(camera.Height.GetMax());
                }

                if (camera.GetSfncVersion() >= Sfnc_2_0_0) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    // Set the target value for luminance control.
                    // A value of 0.3 means that the target brightness is 30 % of the maximum brightness of the raw pixel value read out from the sensor.
                    // A value of 0.4 means 40 % and so forth.
                    camera.AutoTargetBrightness.SetValue(0.3);

                    // Try ExposureAuto = Once.
                    std::cout << "Trying 'ExposureAuto = Once'." << std::endl;
                    std::cout << "Initial exposure time = ";
                    std::cout << camera.ExposureTime.GetValue() << " us" << std::endl;

                    // Set the exposure time ranges for luminance control.
                    camera.AutoExposureTimeLowerLimit.SetValue(camera.AutoExposureTimeLowerLimit.GetMin());
                    camera.AutoExposureTimeUpperLimit.SetValue(camera.AutoExposureTimeLowerLimit.GetMax());

                    camera.ExposureAuto.SetValue(ExposureAuto_Once);
                }

                // When the "once" mode of operation is selected,
                // the parameter values are automatically adjusted until the related image property
                // reaches the target value. After the automatic parameter value adjustment is complete, the auto
                // function will automatically be set to "off", and the new parameter value will be applied to the
                // subsequently grabbed images.
                int n = 0;
                while (camera.ExposureAuto.GetValue() != ExposureAuto_Off)
                {
                    CBaslerUniversalGrabResultPtr ptrGrabResult;
                    camera.GrabOne(5000, ptrGrabResult);
                    ++n;

                    //Make sure the loop is exited.
                    if (n > 100) {
                        throw TIMEOUT_EXCEPTION( "The adjustment of auto exposure did not finish.");
                    }
                }

                std::cout << "ExposureAuto went back to 'Off' after " << n << " frames." << std::endl;
                std::cout << "Final exposure time = ";

                if (camera.ExposureTime.IsReadable()) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    std::cout << camera.ExposureTime.GetValue() << " us" << std::endl;
                }

                camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
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

QString SingleCamera::getFriendlyName() {
    try {
        return QString(camera.GetDeviceInfo().GetFriendlyName().c_str());
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return "";
}

QString SingleCamera::getFullName() {
    try {
        return QString(camera.GetDeviceInfo().GetFullName().c_str());
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return "";
}

QString SingleCamera::getDeviceID() {
    try {
        return QString(camera.GetDeviceInfo().GetDeviceID().c_str());
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return "";
}

int SingleCamera::getExposureTimeValue() {
    try {
        if (camera.ExposureTime.IsReadable()) {
            return static_cast<int>(camera.ExposureTime.GetValue());
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleCamera::getExposureTimeMin() {
    try {
        if (camera.ExposureTime.IsReadable()) {
            return static_cast<int>(camera.ExposureTime.GetMin());
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleCamera::getExposureTimeMax() {
    try {
        if (camera.ExposureTime.IsReadable()) {
            return static_cast<int>(camera.ExposureTime.GetMax());
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

double SingleCamera::getGainValue() {
    try {
        if (camera.Gain.IsReadable()) {
            return camera.Gain.GetValue();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

double SingleCamera::getGainMin() {
    try {
        if (camera.Gain.IsReadable()) {
            return camera.Gain.GetMin();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

double SingleCamera::getGainMax() {
    try {
        if (camera.Gain.IsReadable()) {
            return camera.Gain.GetMax();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

void SingleCamera::setGainValue(double value) {
    try {
        if (camera.Gain.IsReadable()) {
            if (getGainMax() < value)
                value = getGainMax();
            else if (getGainMin() > value)
                value = getGainMin();
        }
        if (camera.Gain.IsWritable()) {
            camera.Gain.TrySetValue(value);
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

void SingleCamera::setExposureTimeValue(int value) {
    try {
        if (camera.ExposureTime.IsReadable()) {
            if (getExposureTimeMax() < value)
                value = getExposureTimeMax();
            else if (getExposureTimeMin() > value)
                value = getExposureTimeMin();
        }
        if (camera.ExposureTime.IsWritable()) {
            camera.ExposureTime.TrySetValue(value);
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

void SingleCamera::loadFromFile(const String_t &filename) {
    try {
        CFeaturePersistence::Load( filename, &camera.GetNodeMap(), true );
    } catch (const GenericException &e) {
        // Error handling.
        std::cerr << "An exception occurred: " << e.GetDescription() << std::endl;
    }

    if(camera.TriggerMode.GetValueOrDefault("Off") == "On") {
        lineSource = camera.TriggerSource.GetValueOrDefault("Line1");
        enableHardwareTrigger(true);
    }
}

void SingleCamera::saveToFile(const String_t &filename) {
    try {
        CFeaturePersistence::Save(filename, &camera.GetNodeMap() );
    } catch (const GenericException &e) {
        // Error handling.
        std::cerr << "An exception occurred: " << e.GetDescription() << std::endl;
    }
}

bool SingleCamera::isEnabledAcquisitionFrameRate() {
    try {
        if (camera.AcquisitionFrameRateEnable.IsReadable()) {
            return camera.AcquisitionFrameRateEnable.GetValue();
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return false;
}

bool SingleCamera::isEmulated()
{
    QString device_name = QString(camera.GetDeviceInfo().GetModelName().c_str());
    return (device_name.toLower().contains("emu"));
}

void SingleCamera::enableAcquisitionFrameRate(bool enabled) {
    try {
        if (camera.AcquisitionFrameRateEnable.IsWritable()) {
            camera.AcquisitionFrameRateEnable.TrySetValue(enabled);
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
}

void SingleCamera::setAcquisitionFPSValue(int value) {
    try {
        if (camera.AcquisitionFrameRate.IsWritable()) {
            camera.AcquisitionFrameRate.TrySetValue(value);
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
}

int SingleCamera::getAcquisitionFPSValue() {
    try {
        if (camera.AcquisitionFrameRate.IsReadable()) {
            return static_cast<int>(camera.AcquisitionFrameRate.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleCamera::getAcquisitionFPSMin() {
    try {
        if (camera.AcquisitionFrameRate.IsReadable()) {
            return static_cast<int>(camera.AcquisitionFrameRate.GetMin());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleCamera::getAcquisitionFPSMax() {
    try {
        if (camera.AcquisitionFrameRate.IsReadable()) {
            return static_cast<int>(camera.AcquisitionFrameRate.GetMax());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

double SingleCamera::getResultingFrameRateValue() {
    try {
        //qDebug() << "--------------------------------" << QString(camera.GetDeviceInfo().GetDeviceClass());
        if(QString(camera.GetDeviceInfo().GetDeviceClass()) == "BaslerGigE") {
            GenApi::INodeMap& nodemap = camera.GetNodeMap();
            // Get the resulting acquisition frame rate
            return CFloatParameter(nodemap, "ResultingFrameRateAbs").GetValue();
        } else if (camera.ResultingFrameRate.IsReadable()) {
            return camera.ResultingFrameRate.GetValue();
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

CameraCalibration *SingleCamera::getCameraCalibration() {
    return cameraCalibration;
}

bool SingleCamera::isHardwareTriggerEnabled() {
    return hardwareTriggerEnabled;
}

/*
 * Synchronizes system and camera time and sets corresponding variables, should be executed before grabbing starts
 */
void SingleCamera::synchronizeTime() {

    //GenApi::INodeMap& nodemap = camera.GetNodeMap();

    // Take a "snapshot" of the camera's current timestamp value
    //CCommandParameter(nodemap, "GevTimestampControlLatch").Execute();
    camera.TimestampLatch.Execute();
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> epoche = std::chrono::time_point<std::chrono::system_clock>{};

    // Get the timestamp value
    //cameraTime = CIntegerParameter(nodemap, "GevTimestampValue").GetValue();
    cameraTime = static_cast<uint64>(camera.TimestampLatchValue.GetValue());
    systemTime  = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    std::time_t startTime = std::chrono::system_clock::to_time_t(start);
    std::time_t epochTime = std::chrono::system_clock::to_time_t(epoche);

    std::cout << "Camera Synchronize Time" << std::endl << "=========================" << std::endl;
    std::cout << "Timestamp Camera: " << cameraTime << std::endl;
    std::cout << "Timestamp System: " << systemTime << std::endl;
    std::cout << "System Epoch: " << std::ctime(&epochTime) << std::endl;
    std::cout << "System Time: " << std::ctime(&startTime) << std::endl;
    std::cout << "Time from Epoch (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count() << std::endl;
    std::cout << "Time from Epoch (us): " << std::chrono::duration_cast<std::chrono::microseconds>(start.time_since_epoch()).count() << std::endl;
    std::cout << "=========================" << std::endl;
}

String_t SingleCamera::getLineSource() {
    return lineSource;
}

void SingleCamera::setLineSource(String_t value) {
    lineSource = value;
}

CameraImageType SingleCamera::getType() {
    return CameraImageType::LIVE_SINGLE_CAMERA;
}

void SingleCamera::startGrabbing()
{
    if (camera.IsOpen() && !camera.IsGrabbing())
        camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
}

void SingleCamera::stopGrabbing()
{
    if (camera.IsOpen() && camera.IsGrabbing())
        camera.StopGrabbing();
}

QString SingleCamera::getCalibrationFilename() {

    return settingsDirectory.filePath(getFriendlyName() + "_calibration_" +
            QString::number(cameraCalibration->getSquareSize()) + "_" +
            QString::number(cameraCalibration->getBoardSize().width+1) + "x" +
            QString::number(cameraCalibration->getBoardSize().height+1) + ".xml");
}

void SingleCamera::loadCalibrationFile() {
    QString configFile = getCalibrationFilename();
    configFile.replace(" ", "");

    if (QFile::exists(configFile)) {
        std::cout << "Found calibration file in settings directory. Loading: " << configFile.toStdString() << std::endl;
        cameraCalibration->loadFromFile(configFile.toStdString().c_str());
    }
}


int SingleCamera::getImageROIwidth() {
    try {
        if(camera.Width.IsReadable()) {
            return static_cast<int>(camera.Width.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleCamera::getImageROIheight() {
    try {
        if (camera.Height.IsReadable()) {
            return static_cast<int>(camera.Height.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleCamera::getImageROIoffsetX() {
    try {
        if(camera.OffsetX.IsReadable()) {
            return static_cast<int>(camera.OffsetX.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleCamera::getImageROIoffsetY() {
    try {
        if (camera.OffsetY.IsReadable()) {
            return static_cast<int>(camera.OffsetY.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// NOTE: Binning affects this
int SingleCamera::getImageROIwidthMax() {
    // Classic/U/L GigE cameras
  //  return (int)camera.Width.GetMax();
    // other cameras
    try {
        if(camera.WidthMax.IsReadable()) {
            return static_cast<int>(camera.WidthMax.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// NOTE: Binning affects this
int SingleCamera::getImageROIheightMax() {
    // Classic/U/L GigE cameras
  //  return (int)camera.Height.GetMax();
    // other cameras
    try {
        if (camera.WidthMax.IsReadable()) {
            return static_cast<int>(camera.HeightMax.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

QRectF SingleCamera::getImageROI(){
    return QRectF(getImageROIoffsetX(),getImageROIoffsetY(),getImageROIwidth(), getImageROIheight());
}

int SingleCamera::getBinningVal() {
    //if(camera.BinningHorizontal.GetValue()!=camera.BinningVertical.GetValue())
    //    return 0;
    try {
        return camera.BinningHorizontal.GetValue();
    }
    catch (const GenericException &e) {
        return 1;
    }
}

double SingleCamera::getTemperature() {
    double d = 0.0;
    try {
        if(!camera.DeviceTemperature.IsReadable())
            return d;

        // DEV
        //qDebug() << camera.GetValue(Basler_UniversalCameraParams::PLCamera::DeviceModelName);

        // NOTE: this line is only needed in ace 2, boost, and dart IMX Cameras
        // NOTE: sensor temp (and maybe others too) cannot be measured while grabbing, but coreboard anytime
        camera.DeviceTemperatureSelector.TrySetValue(Basler_UniversalCameraParams::DeviceTemperatureSelectorEnums::DeviceTemperatureSelector_Coreboard);

        d = camera.DeviceTemperature.GetValue();
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }

    return d;
}

bool SingleCamera::isGrabbing()
{
    return camera.IsGrabbing();
}

// NOTE: grabbing "pause" is necessary for setting binning
bool SingleCamera::setBinningVal(int value) {

    bool success = false;

    if(camera.IsGrabbing())
        camera.StopGrabbing();

    // in case of our Basler cameras here, only mode=1,2,4 are only valid values
    if (camera.BinningVertical.IsWritable()) {
        // "Enable sensor binning"
        // "Note: Available on selected camera models only"
       // camera.BinningSelector.SetValue(BinningSelector_Sensor); // NOTE: found in Basler docs, but no trace of it in Pylon::CBaslerUniversalInstantCamera:: when code tries to compile. What is this?

        // Set "binning mode" of camera
        camera.BinningHorizontalMode.TrySetValue(BinningHorizontalMode_Average);
        //camera.BinningHorizontalMode.SetValue(BinningHorizontalMode_Sum);
        camera.BinningVerticalMode.TrySetValue(BinningVerticalMode_Average);
        //camera.BinningVerticalMode.SetValue(BinningHorizontalMode_Sum);

        if(value==2 || value==3) {
            success = camera.BinningHorizontal.TrySetValue(2) &&
                    camera.BinningVertical.TrySetValue(2);
            std::cout << "Setting binning to 2 on both axes"<< std::endl;
        } else if(value==4) {
            success = camera.BinningHorizontal.TrySetValue(4) &&
                    camera.BinningVertical.TrySetValue(4);
            std::cout << "Setting binning to 4 on both axes"<< std::endl;
        } else { //if(value==1) {
            success = camera.BinningHorizontal.TrySetValue(1) &&
                    camera.BinningVertical.TrySetValue(1);
            std::cout << "Setting binning to 1 (no binning) on both axes"<< std::endl;
        }
    }
    camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool SingleCamera::setImageROIwidth(int width) {
    //std::cout << "Setting Image ROI width=" << std::to_string(width) << std::endl;
    bool success = false;

    if(camera.IsGrabbing())
        camera.StopGrabbing();

    // ace Classic/U/L GigE Cameras
   // int maxWidth = camera.Width.GetMax();
   // int maxHeight = camera.Height.GetMax();
    // other cameras
    int maxWidth = getImageROIwidthMax();
    int offsetX = getImageROIoffsetX();

    if(width < 16)
        width=16;

    int modVal=width%16;
    if(modVal != 0)
        width -= modVal;

    int bestWidth = (offsetX+width > maxWidth) ? maxWidth-offsetX-((maxWidth-offsetX)%16) : width;
//    if (offsetX >= maxWidth-16)
//        width = maxWidth-offsetX;

    if (camera.Width.IsWritable() ) {
        success = camera.Width.TrySetValue(bestWidth);
    }
    camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool SingleCamera::setImageROIheight(int height) {
    //std::cout << "Setting Image ROI height=" << std::to_string(height) << std::endl;
    bool success = false;

    if(camera.IsGrabbing())
        camera.StopGrabbing();

    // ace Classic/U/L GigE Cameras
   // int maxWidth = camera.Width.GetMax();
   // int maxHeight = camera.Height.GetMax();
    // other cameras
    int maxHeight = getImageROIheightMax();
    int offsetY = getImageROIoffsetY();

    if(height < 16)
        height=16;

    int modVal=height%16;
    if(modVal != 0)
        height -= modVal;

    int bestHeight = (offsetY+height > maxHeight) ? maxHeight-offsetY-((maxHeight-offsetY)%16) : height;
//    if (offsetY >= maxHeight-16)
//        height = maxHeight-offsetY;

    if (camera.Height.IsWritable() ) {
        success = camera.Height.TrySetValue(bestHeight);
    } 
    camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool SingleCamera::setImageROIoffsetX(int offsetX) {
    //std::cout << "Setting Image ROI offsetX=" << std::to_string(offsetX) << std::endl;
    bool success = false;

    if(camera.IsGrabbing())
        camera.StopGrabbing();

    // ace Classic/U/L GigE Cameras
   // int maxWidth = camera.Width.GetMax();
   // int maxHeight = camera.Height.GetMax();
    // other cameras
    int maxWidth = getImageROIwidthMax();
    int width = getImageROIwidth();

    if(maxWidth - offsetX < 16)
        offsetX = maxWidth - 16;
    //if(width + offsetX > maxWidth)
    //    return;
    
    int modVal=offsetX%16;
    if(modVal != 0)
        offsetX -= modVal;

    if (width + offsetX <= maxWidth && camera.OffsetX.IsWritable() ) {
        success = camera.OffsetX.TrySetValue(offsetX);
    }
    camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool SingleCamera::setImageROIoffsetY(int offsetY) {
    //std::cout << "Setting Image ROI offsetY=" << std::to_string(offsetY) << std::endl;
    bool success = false;

    if(camera.IsGrabbing())
        camera.StopGrabbing();

    // ace Classic/U/L GigE Cameras
   // int maxWidth = camera.Width.GetMax();
   // int maxHeight = camera.Height.GetMax();
    // other cameras
    int maxHeight = getImageROIheightMax();
    int height = getImageROIheight();

    if(maxHeight - offsetY < 16)
        offsetY = maxHeight - 16;
    //if(height + offsetY > maxHeight)
    //    return;

    int modVal=offsetY%16;
    if(modVal != 0)
        offsetY -= modVal;

    if (height + offsetY <= maxHeight && camera.OffsetY.IsWritable() ) {
        success = camera.OffsetY.TrySetValue(offsetY);
    } 
    camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
    return success;
}

#else

#include <iostream>

SingleCamera::SingleCamera(const QString &friendlyName, QObject* parent)
        : Camera(parent),
        frameCounter(new CameraFrameRateCounter(parent)),
        cameraCalibration(new CameraCalibration()),
        calibrationThread(new QThread()),
        hardwareTriggerEnabled(false),
        lineSource("Line1") {

    uint n = arv_get_n_devices();

    // TODO
    if(n < 1) {
        throw new std::exception("Camera connection problem.");
        //return;
    }

    int i = 0;
    while(i < n) {

        // TODO: regexp?
        QString matchableFoundDeviceID = arv_get_device_id(i);
        matchableFoundDeviceID.replace(" ", "");
        matchableFoundDeviceID.replace("-", "");
        matchableFoundDeviceID.replace("(", "");
        matchableFoundDeviceID.replace(")", "");

        QString matchableTargetDeviceID = friendlyName;
        matchableTargetDeviceID.replace(" ", "");
        matchableTargetDeviceID.replace("-", "");
        matchableTargetDeviceID.replace("(", "");
        matchableTargetDeviceID.replace(")", "");

        if(matchableFoundDeviceID == matchableTargetDeviceID)
            break;
        i++;
    }

    if(i >= n) {
        throw new std::exception("The specified camera was not found among the ones currently detected.");
        //return;
    }

    GError *error = NULL;
    camera = arv_camera_new(arv_get_device_id(i), &error);

    // TODO
    if(!ARV_IS_CAMERA(camera)) {
        g_clear_object (&camera);
        camera = nullptr;

        throw new std::exception("Could not initialize Aravis camera.");
        //return;
    }

    // TODO
    if(error != NULL) {
        /* En error happened, display the correspdonding message */
        printf ("Error: %s\n", error->message);
//        return EXIT_FAILURE;
    }

    qDebug() << "To our best knowledge, the camera was successfully opened: " << arv_get_device_id(i);
    size_t xmls;
    qDebug() << "GenICam XML:\n" << arv_device_get_genicam_xml(arv_camera_get_device(camera), &xmls);

    connect(frameCounter, SIGNAL(fps(double)), this, SIGNAL(fps(double)));
    connect(frameCounter, SIGNAL(framecount(int)), this, SIGNAL(framecount(int)));

    cameraImageEventHandler = new SingleCameraImageEventHandler(parent);
    connect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), this, SIGNAL(onNewGrabResult(CameraImage)));
    connect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), frameCounter, SLOT(count(CameraImage)));
    //
    connect(cameraImageEventHandler, SIGNAL(imagesSkipped()), this, SIGNAL(imagesSkipped()));

    ////// camera.RegisterImageEventHandler(cameraImageEventHandler, RegistrationMode_Append, Cleanup_Delete);
    //arv_camera_set_acquisition_mode (camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error);

    callbackData.counter = 0;
    callbackData.done = FALSE;
    callbackData.stream = NULL;
    callbackData.emitter = cameraImageEventHandler; //nullptr;
    callbackData.aboutToStopGrabbing = false;

    resizeStreamBuffer();

    synchronizeTime();
    cameraImageEventHandler->setTimeSynchronization(cameraTime, systemTime);

    //camera.PixelFormat.SetValue(PixelFormat_Mono8);

    // load calibration if existing
    if(!cameraCalibration->isCalibrated()) {
        // If we already used this camera before, a config file may exists
        loadCalibrationFile();
    }

    //CIntegerParameter heartbeat( camera.GetTLNodeMap(), "HeartbeatTimeout" );
    //heartbeat.TrySetValue( 1000, IntegerValueCorrection_Nearest );

    startGrabbing();

    // TODO
    if(error != NULL) {
        /* En error happened, display the correspdonding message */
        printf ("Error: %s\n", error->message);
//        return EXIT_FAILURE;
    }

    /*
    // TODO: LOOKUP CAMERA

    Pylon::DeviceInfoList_t allDevices;
    Pylon::DeviceInfoList_t lstDevices;
    TlFactory.EnumerateDevices(lstDevices);

    if (pTl == NULL) {
        qDebug() << "Error: No GigE transport layer installed.";
        qDebug() << "       Please install GigE support as it is required for this sample.";
        //return {};
        allDevices = lstDevices;
    } else {
        Pylon::DeviceInfoList_t lstDevicesGigE;
        pTl->EnumerateAllDevices(lstDevicesGigE);
        std::merge(lstDevices.begin(), lstDevices.end(), lstDevicesGigE.begin(), lstDevicesGigE.end(), std::back_inserter(allDevices));
    }

    //Pylon::DeviceInfoList_t lstDevices = enumerateCameraDevices();
    if(allDevices.empty()) {
        // TODO
        return;
    }
    CDeviceInfo di;
    Pylon::DeviceInfoList_t::const_iterator deviceIt;
    for (deviceIt = allDevices.begin(); deviceIt != allDevices.end(); ++deviceIt) {
        qDebug() << "deviceIt->GetFriendlyName().c_str() " << deviceIt->GetFriendlyName().c_str();
        if(deviceIt->GetFriendlyName().c_str() == friendlyName) {
            qDebug() << "FOUND";
            di = *deviceIt;
            break;
        }
    }

    //auto di = CDeviceInfo().SetFriendlyName(friendlyName.toStdString().c_str());

    camera.Attach(TlFactory.CreateDevice(di));
    //camera = TlFactory.CreateDevice(di);

    settingsDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    if(!settingsDirectory.exists()) {
// mkdir(".") DOES NOT WORK ON MACOS, ONLY WINDOWS. (Reported on MacOS 12.7.6 and Windows 10)
//        settingsDirectory.mkdir(".");
        QDir().mkpath(settingsDirectory.absolutePath());
    }

    // calibration worker thread
    cameraCalibration->moveToThread(calibrationThread);
    calibrationThread->start();
    calibrationThread->setPriority(QThread::HighPriority);

    connect(frameCounter, SIGNAL(fps(double)), this, SIGNAL(fps(double)));
    connect(frameCounter, SIGNAL(framecount(int)), this, SIGNAL(framecount(int)));

    if(camera.IsOpen()) {
        camera.Close();
    }

    try {
        cameraConfigurationEventHandler = new CameraConfigurationEventHandler;
        connect(cameraConfigurationEventHandler, SIGNAL(cameraDeviceRemoved()), this, SIGNAL(cameraDeviceRemoved()));
        camera.RegisterConfiguration(cameraConfigurationEventHandler, RegistrationMode_ReplaceAll, Cleanup_Delete);

        softwareTriggerConfiguration = new CAcquireContinuousConfiguration;
        camera.RegisterConfiguration(softwareTriggerConfiguration, RegistrationMode_Append, Cleanup_Delete);

        cameraImageEventHandler = new SingleCameraImageEventHandler(parent);
        connect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), this, SIGNAL(onNewGrabResult(CameraImage)));
        connect(cameraImageEventHandler, SIGNAL(onNewGrabResult(CameraImage)), frameCounter, SLOT(count(CameraImage)));
        //
        connect(cameraImageEventHandler, SIGNAL(imagesSkipped()), this, SIGNAL(imagesSkipped()));

        camera.RegisterImageEventHandler(cameraImageEventHandler, RegistrationMode_Append, Cleanup_Delete);

        camera.Open();
        assert(camera.IsOpen());

        synchronizeTime();
        cameraImageEventHandler->setTimeSynchronization(cameraTime, systemTime);

        camera.PixelFormat.SetValue(PixelFormat_Mono8);

        // load calibration if existing
        if(!cameraCalibration->isCalibrated()) {
            // If we already used this camera before, a config file may exists
            loadCalibrationFile();
        }

        CIntegerParameter heartbeat( camera.GetTLNodeMap(), "HeartbeatTimeout" );
        heartbeat.TrySetValue( 1000, IntegerValueCorrection_Nearest );

        if(camera.CanWaitForFrameTriggerReady()) {

            // Start the grabbing using the grab loop thread, by setting the grabLoopType parameter
            // to GrabLoop_ProvidedByInstantCamera. The grab results are delivered to the image event handlers.
            // The GrabStrategy_OneByOne default grab strategy is used.
            camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
        } else {
            // See the documentation of CInstantCamera::CanWaitForFrameTriggerReady() for more information.
            std::cout << std::endl;
            std::cout << "Error: This sample can only be used with cameras that can be queried whether they are ready to accept the next frame trigger.";
            std::cout << std::endl;
            std::cout << std::endl;
        }
    }
    catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
     */
}

SingleCamera::~SingleCamera() {
    if (cameraCalibration != nullptr)
        cameraCalibration->deleteLater();
    if (calibrationThread != nullptr) {
        calibrationThread->quit();
        calibrationThread->deleteLater();
    }
}

void SingleCamera::resizeStreamBuffer() {

    // just to be sure
    stopGrabbing();

    GError *error = NULL;

    // NOTE. already done in stopGrabbing, although not NULLed
    if(callbackData.stream) {
        arv_stream_stop_thread(callbackData.stream, true);
        g_clear_object (&callbackData.stream);
    }

    // Create the stream object with callback
    callbackData.stream = arv_camera_create_stream (camera, cameraImageEventHandler->stream_callback, &callbackData, &error);

    if (ARV_IS_STREAM (callbackData.stream)) {
        int i;
        size_t payload;

        // Retrieve the payload size for buffer creation
        payload = arv_camera_get_payload(camera, &error);
        if (error == NULL) {
            // Insert some buffers in the stream buffer pool
            for (i = 0; i < 2; i++)
                arv_stream_push_buffer(callbackData.stream, arv_buffer_new(payload, NULL));
        }
    }

    // TODO
    if(error != NULL) {
        // En error happened, display the correspdonding message
        printf ("Error: %s\n", error->message);
//        return EXIT_FAILURE;
    }
}

void SingleCamera::getTEST() {
    ArvCamera *camera;
    GError *error = NULL;

    /* Connect to the first available camera */
    camera = arv_camera_new(NULL, &error);

    if(ARV_IS_CAMERA(camera)) {
        int width;
        int height;
        const char *pixel_format;

        printf("Found camera '%s'\n", arv_camera_get_model_name(camera, NULL));

        if (!error) arv_camera_get_region(camera, NULL, NULL, &width, &height, &error);
        if (!error) pixel_format = arv_camera_get_pixel_format_as_string(camera, &error);

        if (error == NULL) {
            printf ("Width = %d\n", width);
            printf ("Height = %d\n", height);
            printf ("Pixel format = %s\n", pixel_format);
        }

        g_clear_object (&camera);
    }

    if (error != NULL) {
        /* En error happened, display the correspdonding message */
        printf ("Error: %s\n", error->message);
//        return EXIT_FAILURE;
    }
}

void SingleCamera::genericExceptionOccured(const std::exception &e, const GError &lastAravisError) {
    //QThread::msleep(1000);
    std::cerr << "An Aravis exception occurred." << std::endl<< e.what() << std::endl;

    // TODO: sketchy check if the device was removed or not
    bool deviceRemoved = false;
    if(QString::fromStdString(lastAravisError.message).toLower().contains("remov")) {
        deviceRemoved = true;
    }

    genericExceptionOccured(e, deviceRemoved);
}

void SingleCamera::genericExceptionOccured(const std::exception &e, bool deviceRemoved) {
    //QThread::msleep(1000);
    std::cerr << "An Aravis exception occurred." << std::endl<< e.what() << std::endl;

    // logic: only quick cleanup if the device got removed. If not, we will keep running

    if (deviceRemoved) {
        emit cameraDeviceRemoved();
        arv_shutdown();
        g_clear_object (&camera);
        camera = nullptr;
    }
}

bool SingleCamera::isOpen() {

    return (camera != nullptr);

    //return camera.IsOpen();
}

void SingleCamera::close() {
    std::cout << "SingleCamera: Releasing resources.";

    // TODO: might not necessarily happen here
    stopGrabbing();

    // // NOTE: already done in stopGrabbing
    // arv_stream_stop_thread(callbackData.stream, true);
    // g_clear_object(&callbackData.stream);

    g_clear_object(&camera);
    camera = nullptr;

    /*
    camera.StopGrabbing();
    camera.Close();
    camera.DeregisterImageEventHandler(cameraImageEventHandler);
    camera.DeregisterConfiguration(cameraConfigurationEventHandler);
    if(hardwareTriggerConfiguration) {
        camera.DeregisterConfiguration(hardwareTriggerConfiguration);
    }
    if(softwareTriggerConfiguration) {
        camera.DeregisterConfiguration(softwareTriggerConfiguration);
    }
    */
}

void SingleCamera::enableHardwareTrigger(bool state) {

    GError *error = nullptr;

    qDebug() << "SingleCamera: Enabling Hardware trigger to line source: " + lineSource << " to state: " << state;

    frameCounter->reset();

    try {

        stopGrabbing();

        //auto a = arv_camera_get_acquisition_mode(camera, &error);
        //auto t = arv_camera_get_trigger_source(camera, &error);
        bool isSoftwareTriggerSupported = arv_camera_is_software_trigger_supported(camera, &error);
        auto device = arv_camera_get_device(camera);

        // TODO: set line source if not set
        //setLineSource(lineSource);

        arv_device_set_string_feature_value(device, "TriggerSelector", "FrameStart", &error);
        if(error){
            qDebug() << "Could not set TriggerSelector to value FrameStart.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }

        //auto b = arv_acquisition_mode_from_string("Continuous");
        //if(!error)
        arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error);
        if(error){
            qDebug() << "Could not set ARV_ACQUISITION_MODE_CONTINUOUS.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }

        std::string stateStr = (state) ? "On" : "Off";
        arv_device_set_string_feature_value(device, "TriggerMode", stateStr.c_str(), &error);
        if(error){
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }

        if(!state) {
            arv_camera_software_trigger(camera, &error);
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

        startGrabbing();

        // TODO: for some reason we need this workaround to get things started
        setImageROIwidth(getImageROIwidth());

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

}

void SingleCamera::autoGainOnce() {
    try {
        GError *error = nullptr;

        //if(!camera.IsOpen()) {
        //    camera.Open();
        //}
        if(!camera) {
            return;
        }

        stopGrabbing();

        bool isAuto = arv_camera_is_gain_auto_available(camera, &error);

        if(error) {
            qDebug() << "Auto gain is not available.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(isAuto) {
            arv_camera_set_gain_auto(camera, ArvAuto::ARV_AUTO_ONCE, &error);
            if(error) {
                qDebug() << "Could not et auto gain.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    startGrabbing();
}

void SingleCamera::autoExposureOnce() {
    try {
        GError *error = nullptr;

        //if(!camera.IsOpen()) {
        //    camera.Open();
        //}
        if(!camera) {
            return;
        }

        stopGrabbing();

        bool isAuto = arv_camera_is_exposure_auto_available(camera, &error);

        if(error) {
            qDebug() << "Auto exposure is not available.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(isAuto) {
            arv_camera_set_exposure_time_auto(camera, ArvAuto::ARV_AUTO_ONCE, &error);
            if(error) {
                qDebug() << "Could not set auto exposure.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    startGrabbing();
}

QString SingleCamera::getFriendlyName() {
    GError *error = nullptr;
    QString val = "";
    try {
        QString vendorName, deviceModel, serialNumber = "";

        vendorName = arv_camera_get_vendor_name(camera, &error);
        if(!error) deviceModel = arv_camera_get_model_name(camera, &error);
        if(!error) serialNumber = arv_camera_get_device_serial_number(camera, &error);

        val = vendorName + " " + deviceModel + " (" + serialNumber + ")";

    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

QString SingleCamera::getFullName() {
    // shortcut for Aravis
    return getDeviceID();
}

QString SingleCamera::getDeviceID() {
    GError *error = nullptr;
    QString val = "";
    try {
        val = QString(arv_camera_get_device_id(camera, &error));
        if(error) {
            qDebug() << "Could not get device ID.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int SingleCamera::getExposureTimeValue() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_exposure_time_available(camera, &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            val = (int)round(arv_camera_get_exposure_time(camera, &error));
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

int SingleCamera::getExposureTimeMin() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_exposure_time_available(camera, &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            arv_camera_get_exposure_time_bounds(camera, &valMin, &valMax, &error);
            // extra checks could happen here
            val = (int)round(valMin);
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

int SingleCamera::getExposureTimeMax() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_exposure_time_available(camera, &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            arv_camera_get_exposure_time_bounds(camera, &valMin, &valMax, &error);
            // extra checks could happen here
            val = (int)round(valMax);
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

double SingleCamera::getGainValue() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_gain_available(camera, &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            val = arv_camera_get_gain(camera, &error);
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

double SingleCamera::getGainMin() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_gain_available(camera, &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            arv_camera_get_gain_bounds(camera, &valMin, &valMax, &error);
            // extra checks could happen here
            val = valMin;
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

double SingleCamera::getGainMax() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_gain_available(camera, &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            arv_camera_get_gain_bounds(camera, &valMin, &valMax, &error);
            // extra checks could happen here
            val = valMax;
        }
        if(error) {
            qDebug() << "Could not get gain maximum value.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

void SingleCamera::setGainValue(double value) {
    GError *error = nullptr;
    try {
        bool canGet = arv_camera_is_gain_available(camera, &error);
        double valMin = 0;
        double valMax = 0;
        if(canGet) {
            arv_camera_get_gain_bounds(camera, &valMin, &valMax, &error);
        }
        if(error) {
            qDebug() << "Could not get gain value bounds.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(value <= valMax && value >= valMin) {
            arv_camera_set_gain(camera, value, &error);
            if(error) {
                qDebug() << "Could not set gain value.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
}

void SingleCamera::setExposureTimeValue(int value) {
    GError *error = nullptr;
    try {
        bool canGet = arv_camera_is_exposure_time_available(camera, &error);
        double valMin = 0;
        double valMax = 0;
        if(canGet) {
            arv_camera_get_exposure_time_bounds(camera, &valMin, &valMax, &error);
        }
        if(error) {
            qDebug() << "Could not get exposure time value bounds.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(value <= valMax && value >= valMin) {
            arv_camera_set_exposure_time(camera, (double)value, &error);
            if(error) {
                qDebug() << "Could not set exposure time value.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
}

void SingleCamera::loadFromFile(const std::string &filename) {
    // TODO: It seems aravis does not yet support saving and loading all features. We could iterate through the map
    //  and save what we can, then restore all upon opening, but this requires further larger efforts. Yet unsupported
    /*
    try {
        CFeaturePersistence::Load( filename, &camera.GetNodeMap(), true );
    } catch (const GenericException &e) {
        // Error handling.
        std::cerr << "An exception occurred: " << e.GetDescription() << std::endl;
    }

    if(camera.TriggerMode.GetValueOrDefault("Off") == "On") {
        lineSource = camera.TriggerSource.GetValueOrDefault("Line1");
        enableHardwareTrigger(true);
    }
     */
}

void SingleCamera::saveToFile(const std::string &filename) {
    // TODO: It seems aravis does not yet support saving and loading all features. We could iterate through the map
    //  and save what we can, then restore all upon opening, but this requires further larger efforts. Yet unsupported
    /*
    try {
        CFeaturePersistence::Save(filename, &camera.GetNodeMap() );
    } catch (const GenericException &e) {
        // Error handling.
        std::cerr << "An exception occurred: " << e.GetDescription() << std::endl;
    }
     */
}

bool SingleCamera::isEnabledAcquisitionFrameRate() {
    GError *error = nullptr;
    bool val = false;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        val = arv_camera_get_frame_rate_enable(camera, &error);
        if(error) {
            qDebug() << "Could not get whether acquisition frame rate setting is enabled or not.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

bool SingleCamera::isEmulated() {
    // TODO
    QString device_name = getFriendlyName();
    return (device_name.toLower().contains("emu"));
}

void SingleCamera::enableAcquisitionFrameRate(bool enabled) {
    GError *error = nullptr;

    if(!ARV_IS_CAMERA(camera))
        return;

    try {
        arv_camera_set_frame_rate_enable(camera, enabled, &error);
        if(error) {
            qDebug() << "Could not set acquisition frame rate enabled/disabled.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
}

void SingleCamera::setAcquisitionFPSValue(int value) {
    GError *error = nullptr;
    try {
        bool canGet = arv_camera_is_frame_rate_available(camera, &error);
        if(error) {
            qDebug() << "Acquisition framerate not available.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(canGet) {
            arv_camera_set_frame_rate(camera, (double)value, &error);
            if(error) {
                qDebug() << "Could not set acquisition framerate.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
}

int SingleCamera::getAcquisitionFPSValue() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_frame_rate_available(camera, &error);
        if(error) {
            qDebug() << "Acquisition framerate not available.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(canGet) {
            // NOTE: rounding here
            val = (int)round(arv_camera_get_frame_rate(camera, &error));
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

int SingleCamera::getAcquisitionFPSMin() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_frame_rate_available(camera, &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            arv_camera_get_frame_rate_bounds(camera, &valMin, &valMax, &error);
            // extra checks could happen here
            val = (int)round(valMin);
        }
        if(error) {
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int SingleCamera::getAcquisitionFPSMax() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_frame_rate_available(camera, &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            arv_camera_get_frame_rate_bounds(camera, &valMin, &valMax, &error);
            // extra checks could happen here
            val = (int)round(valMax);
        }
        if(error) {
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

double SingleCamera::getResultingFrameRateValue() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        //auto temp = arv_camera_get_integer(camera, "ResultingFrameRate", &error);
        GValue v = G_VALUE_INIT;
        //g_value_init(&v, G_TYPE_DOUBLE);
        arv_device_get_feature_value(arv_camera_get_device(camera), "ResultingFrameRate", &v, &error);
        float temp = g_value_get_double(&v);

        if(error) {
            qDebug() << "Could not obtain resulting framerate value. This camera might not support it. (It is not a standard GenICam v2.7.1 feature.)";
            qDebug() << "Error during aravis API call. Message: " << error->message;
            qDebug() << "Falling back to acquisition framerate value.";
            GValue h = G_VALUE_INIT;
            //g_value_init(&h, G_TYPE_DOUBLE); // NOTE: already happens in the aravis get feature call
            arv_device_get_feature_value(arv_camera_get_device(camera), "AcquisitionFrameRate", &h, &error);
            temp = g_value_get_double(&h);
        }
        // additional checks could come here
        if(error) {
            qDebug() << "Could neither get resulting framerate, nor acquisition framerate.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else {
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

CameraCalibration *SingleCamera::getCameraCalibration() {
    return cameraCalibration;
}

bool SingleCamera::isHardwareTriggerEnabled() {

    GError *error = nullptr;
    int val = false;
    try {
        // not a boolean but an On/Off "enum"
        QString tval = arv_camera_get_string(camera, "TriggerMode", &error);
        val = (tval == "On");

        if(error) {
            qDebug() << "Could not determine whether hardware triggering is enabled.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }

        //QString resp = QString(arv_camera_get_trigger_source(camera, &error));
        //qDebug() << "RESP = " << resp;
        //resp = resp.toLower();
        //if(resp.contains("hw") || resp.contains("hardware")) {
        //    val = true;
        //}
        //if(error) {
        //    qDebug() << "Error during aravis API call. Message: " << error->message;
        //}
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    hardwareTriggerEnabled = val; // TODO: get rid of this ?
    return val;

    //return hardwareTriggerEnabled;
}

void SingleCamera::synchronizeTime() {

    GError *error = nullptr;

    //GenApi::INodeMap& nodemap = camera.GetNodeMap();

    // Take a "snapshot" of the camera's current timestamp value
    //CCommandParameter(nodemap, "GevTimestampControlLatch").Execute();
    arv_device_execute_command(arv_camera_get_device(camera), "TimestampLatch", &error);
    if(error) {
        qDebug() << "Could not execute TimestampLatch command.";
        qDebug() << "Error during aravis API call. Message: " << error->message;
        qDebug() << "Falling back to deprecated GevTimestampControlLatch command, if this camera only understands that.";
        arv_device_execute_command(arv_camera_get_device(camera), "GevTimestampControlLatch", &error);
    }
    if(error) {
        qDebug() << "Could not execute timestamp latch command.";
        // ...
        return;
    }
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> epoche = std::chrono::time_point<std::chrono::system_clock>{};

    // Get the timestamp value
    cameraTime = arv_camera_get_integer(camera, "TimestampLatchValue", &error);
    if(error) {
        qDebug() << "Could not obtain TimestampLatchValue.";
        qDebug() << "Error during aravis API call. Message: " << error->message;
        qDebug() << "Falling back to deprecated GevTimestampValue, if this camera only understands that.";
        arv_device_execute_command(arv_camera_get_device(camera), "GevTimestampValue", &error);
    }
    if(error) {
        qDebug() << "Could not get camera timestamp latch value.";
        // ...
        return;
    }
    systemTime  = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    std::time_t startTime = std::chrono::system_clock::to_time_t(start);
    std::time_t epochTime = std::chrono::system_clock::to_time_t(epoche);

    std::cout << "Camera Synchronize Time" << std::endl << "=========================" << std::endl;
    std::cout << "Timestamp Camera: " << cameraTime << std::endl;
    std::cout << "Timestamp System: " << systemTime << std::endl;
    std::cout << "System Epoch: " << std::ctime(&epochTime) << std::endl;
    std::cout << "System Time: " << std::ctime(&startTime) << std::endl;
    std::cout << "Time from Epoch (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count() << std::endl;
    std::cout << "Time from Epoch (us): " << std::chrono::duration_cast<std::chrono::microseconds>(start.time_since_epoch()).count() << std::endl;
    std::cout << "=========================" << std::endl;

}

QString SingleCamera::getLineSource() {

    GError *error = nullptr;
    //QString val = "";

    if(!ARV_IS_CAMERA(camera))
        //return val;
        return lineSource;

    try {
        // otherwise we could just query the "TriggerSource" GenICam feature value
        QString temp = QString(arv_camera_get_trigger_source(camera, &error));

        // additional checks could come here
        if(error) {
            qDebug() << "Could not get trigger source.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
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

void SingleCamera::setLineSource(QString value) {

    GError *error = nullptr;
    //QString val = "";

    if(!ARV_IS_CAMERA(camera))
        return;

    try {
        // otherwise we could just set the "TriggerSource" GenICam feature value
        arv_camera_set_trigger_source(camera, value.toStdString().c_str(), &error);

        if(error) {
            qDebug() << "Could not set trigger source.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else {
            lineSource = value;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }

}

CameraImageType SingleCamera::getType() {
    return CameraImageType::LIVE_SINGLE_CAMERA;
}

void SingleCamera::startGrabbing() {

    if (isGrabbing())
        return;

    GError *error = nullptr;

    // TODO: set continous grabbing mode, if not set
//    arv_camera_set_acquisition_mode(camera, ArvAcquisitionMode::ARV_ACQUISITION_MODE_CONTINUOUS, &error);

    //if(!error)
    callbackData.aboutToStopGrabbing = false;
    arv_stream_start_thread(callbackData.stream);
    arv_camera_start_acquisition(camera, &error);

    if(error) {
        qDebug() << "Could not start grabbing.";
        qDebug() << "Error during aravis API call. Message: " << error->message;
    } else {
        isGrabbingV = true;
        qDebug() << "Started grabbing!";
    }

    // pylon version
    //camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
}

void SingleCamera::stopGrabbing() {

    if (!isGrabbing())
        return;

    GError *error = nullptr;
    callbackData.aboutToStopGrabbing = true;
    gboolean delete_buffers = true;
    arv_stream_stop_thread(callbackData.stream, delete_buffers);
    //g_clear_object (&callbackData.stream);
    //callbackData.stream = NULL;
    arv_camera_stop_acquisition(camera, &error);
    // TODO: for some reason it causes errors like the following:
    //  ** (process:8760): CRITICAL **: ...: arv_uv_stream_stop_thread: assertion 'priv->thread == NULL' failed
    //  ** (process:8760): CRITICAL **: ...: arv_uv_stream_start_thread: assertion 'priv->thread == NULL' failed
    //  although stop_thread is called, etc. Possible solution?

    if(error) {
        qDebug() << "Could not gracefully stop grabbing.";
        qDebug() << "Error during aravis API call. Message: " << error->message;
        qDebug() << "Falling back to abort call.";
        arv_camera_abort_acquisition(camera, &error);
    }
    isGrabbingV = false;
    qDebug() << "Stopped grabbing!";

    // pylon version
    //if (camera.IsOpen() && camera.IsGrabbing())
    //    camera.StopGrabbing();
}

QString SingleCamera::getCalibrationFilename() {

    return settingsDirectory.filePath(getFriendlyName() + "_calibration_" +
                                      QString::number(cameraCalibration->getSquareSize()) + "_" +
                                      QString::number(cameraCalibration->getBoardSize().width+1) + "x" +
                                      QString::number(cameraCalibration->getBoardSize().height+1) + ".xml");
}

void SingleCamera::loadCalibrationFile() {
    QString configFile = getCalibrationFilename();
    configFile.replace(" ", "");

    if (QFile::exists(configFile)) {
        std::cout << "Found calibration file in settings directory. Loading: " << configFile.toStdString() << std::endl;
        cameraCalibration->loadFromFile(configFile.toStdString().c_str());
    }
}


int SingleCamera::getImageROIwidth() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        auto temp = arv_camera_get_integer(camera, "Width", &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI Width.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else {
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int SingleCamera::getImageROIheight() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        auto temp = arv_camera_get_integer(camera, "Height", &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI Height.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else {
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int SingleCamera::getImageROIoffsetX() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        auto temp = arv_camera_get_integer(camera, "OffsetX", &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI OffsetX.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else {
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int SingleCamera::getImageROIoffsetY() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        auto temp = arv_camera_get_integer(camera, "OffsetY", &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI OffsetY.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else {
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// NOTE: Binning affects this
int SingleCamera::getImageROIwidthMax() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
//    bool canGet = arv_camera_is_region_offset_available(camera, &error);
//    if(error) {
//        // ...
//    } else if(canGet) {
        gint valMin = 0;
        gint valMax = 0;
        arv_camera_get_width_bounds(camera, &valMin, &valMax, &error);
        // additional checks could come here
        val = valMax;
        if(error) {
            qDebug() << "Could not get image acquisition ROI Width maximum.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
//    }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

// NOTE: Binning affects this
int SingleCamera::getImageROIheightMax() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        gint valMin = 0;
        gint valMax = 0;
        arv_camera_get_height_bounds(camera, &valMin, &valMax, &error);
        // additional checks could come here
        val = valMax;
        if(error) {
            qDebug() << "Could not get image acquisition ROI Height maximum.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

QRectF SingleCamera::getImageROI(){
    GError *error = nullptr;
    QRectF val = {0,0,0,0};

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        gint valXoffset = 0;
        gint valYoffset = 0;
        gint valWidth = 0;
        gint valHeight = 0;
        //arv_camera_get_region(camera, &valXoffset, &valYoffset, &valWidth, &valHeight, &error);

        if(!error) valXoffset = arv_camera_get_integer(camera, "OffsetX", &error);
        if(!error) valYoffset = arv_camera_get_integer(camera, "OffsetY", &error);
        if(!error) valWidth = arv_camera_get_integer(camera, "Width", &error);
        if(!error) valHeight = arv_camera_get_integer(camera, "Height", &error);

        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else {
            val = QRectF(valXoffset, valYoffset, valWidth, valHeight);
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int SingleCamera::getBinningVal() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_binning_available(camera, &error);
        if(error) {
            qDebug() << "Could not get binning value.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(canGet) {
            gint valX = 1;
            gint valY = 1;
            arv_camera_get_binning(camera, &valX, &valY, &error);
            // additional checks could come here
            // TODO reset to larget/smaller (?) value if not equal
            val = valX;
            if(error) {
                // ...
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

double SingleCamera::getTemperature() {

    GError *error = nullptr;
    double val = 0.0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        auto temp = arv_camera_get_float(camera, "DeviceTemperature", &error);
        // fallbacks: set "DeviceTemperatureSelector" value to "Sensor" or "Mainboard"

        // additional checks could come here
        if(error) {
            qDebug() << "Could not get camera temperature reading.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else {
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

bool SingleCamera::isGrabbing() {
    return isGrabbingV;
}

// NOTE: grabbing "pause" is necessary for setting binning
bool SingleCamera::setBinningVal(int value) {

    bool success = false;

    stopGrabbing();

    GError *error = nullptr;
    try {
        bool canGet = arv_camera_is_binning_available(camera, &error);
        gint valXMin = 1;
        gint valXMax = 1;
        gint valYMin = 1;
        gint valYMax = 1;
        if(canGet) {
            arv_camera_get_x_binning_bounds(camera, &valXMin, &valXMax, &error);
            arv_camera_get_y_binning_bounds(camera, &valYMin, &valYMax, &error);
        } else {
            qDebug() << "Binning value is not avaliable.";
        }
        if(error) {
            qDebug() << "Could not get binning value.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if( (value <= valXMax && value >= valXMin) && (value <= valYMax && value >= valYMin) ) {
            // TODO: better, find common number of available X and Y binning values (if they might differ)
            arv_camera_set_binning(camera, value, value, &error);

            resizeStreamBuffer();
            if(error) {
                qDebug() << "Could not set binning value.";
                qDebug() << "Error during aravis API call. Message: " << error->message;
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
bool SingleCamera::setImageROIwidth(int width) {
    //std::cout << "Setting Image ROI width=" << std::to_string(width) << std::endl;
    bool success = false;

    stopGrabbing();

    int maxWidth = getImageROIwidthMax();
    int offsetX = getImageROIoffsetX();

    if(width < 16)
        width=16;

    int modVal=width%16;
    if(modVal != 0)
        width -= modVal;

    int bestWidth = (offsetX+width > maxWidth) ? maxWidth-offsetX-((maxWidth-offsetX)%16) : width;
//    if (offsetX >= maxWidth-16)
//        width = maxWidth-offsetX;

    GError *error = nullptr;
    try {
        auto currentROI = getImageROI();
        arv_camera_set_region(camera, currentROI.x(), currentROI.y(), bestWidth, currentROI.height(), &error);

        resizeStreamBuffer();
        if(error) {
            qDebug() << "Could not set image acquisition ROI Width.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
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
bool SingleCamera::setImageROIheight(int height) {
    //std::cout << "Setting Image ROI height=" << std::to_string(height) << std::endl;
    bool success = false;

    stopGrabbing();

    int maxHeight = getImageROIheightMax();
    int offsetY = getImageROIoffsetY();

    if(height < 16)
        height=16;

    int modVal=height%16;
    if(modVal != 0)
        height -= modVal;

    int bestHeight = (offsetY+height > maxHeight) ? maxHeight-offsetY-((maxHeight-offsetY)%16) : height;
//    if (offsetY >= maxHeight-16)
//        height = maxHeight-offsetY;

    GError *error = nullptr;
    try {
        auto currentROI = getImageROI();
        arv_camera_set_region(camera, currentROI.x(), currentROI.y(), currentROI.width(), bestHeight, &error);

        resizeStreamBuffer();
        if(error) {
            qDebug() << "Could not set image acquisition ROI Height.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
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
bool SingleCamera::setImageROIoffsetX(int offsetX) {
    //std::cout << "Setting Image ROI offsetX=" << std::to_string(offsetX) << std::endl;
    bool success = false;

    stopGrabbing();

    int maxWidth = getImageROIwidthMax();
    int width = getImageROIwidth();

    if(maxWidth - offsetX < 16)
        offsetX = maxWidth - 16;
    //if(width + offsetX > maxWidth)
    //    return;

    int modVal=offsetX%16;
    if(modVal != 0)
        offsetX -= modVal;

    if (width + offsetX > maxWidth) {
        return false;
    }

    GError *error = nullptr;
    try {
        auto currentROI = getImageROI();
        arv_camera_set_region(camera, offsetX, currentROI.y(), currentROI.width(), currentROI.height(), &error);

        resizeStreamBuffer();
        if(error) {
            qDebug() << "Could not set image acquisition ROI OffsetX.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
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
bool SingleCamera::setImageROIoffsetY(int offsetY) {
    //std::cout << "Setting Image ROI offsetY=" << std::to_string(offsetY) << std::endl;
    bool success = false;

    stopGrabbing();

    int maxHeight = getImageROIheightMax();
    int height = getImageROIheight();

    if(maxHeight - offsetY < 16)
        offsetY = maxHeight - 16;
    //if(height + offsetY > maxHeight)
    //    return;

    int modVal=offsetY%16;
    if(modVal != 0)
        offsetY -= modVal;

    if (height + offsetY > maxHeight) {
        return false;
    }

    GError *error = nullptr;
    try {
        auto currentROI = getImageROI();
        arv_camera_set_region(camera, currentROI.x(), offsetY, currentROI.width(), currentROI.height(), &error);

        resizeStreamBuffer();
        if(error) {
            qDebug() << "Could not set image acquisition ROI OffsetY.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
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

#endif
