
#include <pylon/TlFactory.h>
#include <QThread>

#include "singleAravisCamera.h"

SingleAravisCamera::SingleAravisCamera(const QString &friendlyName, QObject* parent)
        : Camera(parent),
        frameCounter(new CameraFrameRateCounter(parent)),
        cameraCalibration(new CameraCalibration()),
        calibrationThread(new QThread()),
//        hardwareTriggerEnabled(false),
        lineSource("Line1") {

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

SingleAravisCamera::~SingleAravisCamera() {
    if (cameraCalibration != nullptr)
        cameraCalibration->deleteLater();
    if (calibrationThread != nullptr) {
        calibrationThread->quit();
        calibrationThread->deleteLater();
    }
}

void SingleAravisCamera::getTEST() {
    ArvCamera *camera;
    GError *error = NULL;

    /* Connect to the first available camera */
    camera = arv_camera_new (NULL, &error);

    if (ARV_IS_CAMERA (camera)) {
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

/*
void SingleAravisCamera::genericExceptionOccured(const GenericException &e) {
    //QThread::msleep(1000);
    std::cerr << "A Pylon exception occurred." << std::endl<< e.GetDescription() << std::endl;
    if (camera.IsCameraDeviceRemoved()) {
        emit cameraDeviceRemoved();
        camera.Close();
        camera.DetachDevice();
        camera.DestroyDevice();
    }
}

bool SingleAravisCamera::isOpen() {
    return camera.IsOpen();
}

void SingleAravisCamera::close() {
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

void SingleAravisCamera::enableHardwareTrigger(bool state) {
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

void SingleAravisCamera::autoGainOnce() {
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

void SingleAravisCamera::autoExposureOnce() {
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

QString SingleAravisCamera::getFriendlyName() {
    try {
        return QString(camera.GetDeviceInfo().GetFriendlyName().c_str());
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return "";
}

QString SingleAravisCamera::getFullName() {
    try {
        return QString(camera.GetDeviceInfo().GetFullName().c_str());
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return "";
}

QString SingleAravisCamera::getDeviceID() {
    try {
        return QString(camera.GetDeviceInfo().GetDeviceID().c_str());
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return "";
}

int SingleAravisCamera::getExposureTimeValue() {
    try {
        if (camera.ExposureTime.IsReadable()) {
            return static_cast<int>(camera.ExposureTime.GetValue());
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleAravisCamera::getExposureTimeMin() {
    try {
        if (camera.ExposureTime.IsReadable()) {
            return static_cast<int>(camera.ExposureTime.GetMin());
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleAravisCamera::getExposureTimeMax() {
    try {
        if (camera.ExposureTime.IsReadable()) {
            return static_cast<int>(camera.ExposureTime.GetMax());
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

double SingleAravisCamera::getGainValue() {
    try {
        if (camera.Gain.IsReadable()) {
            return camera.Gain.GetValue();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

double SingleAravisCamera::getGainMin() {
    try {
        if (camera.Gain.IsReadable()) {
            return camera.Gain.GetMin();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

double SingleAravisCamera::getGainMax() {
    try {
        if (camera.Gain.IsReadable()) {
            return camera.Gain.GetMax();
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

void SingleAravisCamera::setGainValue(double value) {
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

void SingleAravisCamera::setExposureTimeValue(int value) {
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

void SingleAravisCamera::loadFromFile(const std::string &filename) {
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

void SingleAravisCamera::saveToFile(const std::string &filename) {
    try {
        CFeaturePersistence::Save(filename, &camera.GetNodeMap() );
    } catch (const GenericException &e) {
        // Error handling.
        std::cerr << "An exception occurred: " << e.GetDescription() << std::endl;
    }
}

bool SingleAravisCamera::isEnabledAcquisitionFrameRate() {
    try {
        if (camera.AcquisitionFrameRateEnable.IsReadable()) {
            return camera.AcquisitionFrameRateEnable.GetValue();
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return false;
}

bool SingleAravisCamera::isEmulated()
{
    QString device_name = QString(camera.GetDeviceInfo().GetModelName().c_str());
    return (device_name.toLower().contains("emu"));
}

void SingleAravisCamera::enableAcquisitionFrameRate(bool enabled) {
    try {
        if (camera.AcquisitionFrameRateEnable.IsWritable()) {
            camera.AcquisitionFrameRateEnable.TrySetValue(enabled);
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
}

void SingleAravisCamera::setAcquisitionFPSValue(int value) {
    try {
        if (camera.AcquisitionFrameRate.IsWritable()) {
            camera.AcquisitionFrameRate.TrySetValue(value);
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
}

int SingleAravisCamera::getAcquisitionFPSValue() {
    try {
        if (camera.AcquisitionFrameRate.IsReadable()) {
            return static_cast<int>(camera.AcquisitionFrameRate.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleAravisCamera::getAcquisitionFPSMin() {
    try {
        if (camera.AcquisitionFrameRate.IsReadable()) {
            return static_cast<int>(camera.AcquisitionFrameRate.GetMin());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleAravisCamera::getAcquisitionFPSMax() {
    try {
        if (camera.AcquisitionFrameRate.IsReadable()) {
            return static_cast<int>(camera.AcquisitionFrameRate.GetMax());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

double SingleAravisCamera::getResultingFrameRateValue() {
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
*/
CameraCalibration *SingleAravisCamera::getCameraCalibration() {
    return cameraCalibration;
}
/*
bool SingleAravisCamera::isHardwareTriggerEnabled() {
    return hardwareTriggerEnabled;
}

void SingleAravisCamera::synchronizeTime() {

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

std::string SingleAravisCamera::getLineSource() {
    return lineSource;
}

void SingleAravisCamera::setLineSource(std::string value) {
    lineSource = value;
}

CameraImageType SingleAravisCamera::getType() {
    return CameraImageType::LIVE_SINGLE_CAMERA;
}

void SingleAravisCamera::startGrabbing()
{
    if (camera.IsOpen() && !camera.IsGrabbing())
        camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
}

void SingleAravisCamera::stopGrabbing()
{
    if (camera.IsOpen() && camera.IsGrabbing())
        camera.StopGrabbing();
}

QString SingleAravisCamera::getCalibrationFilename() {

    return settingsDirectory.filePath(getFriendlyName() + "_calibration_" +
            QString::number(cameraCalibration->getSquareSize()) + "_" +
            QString::number(cameraCalibration->getBoardSize().width+1) + "x" +
            QString::number(cameraCalibration->getBoardSize().height+1) + ".xml");
}

void SingleAravisCamera::loadCalibrationFile() {
    QString configFile = getCalibrationFilename();
    configFile.replace(" ", "");

    if (QFile::exists(configFile)) {
        std::cout << "Found calibration file in settings directory. Loading: " << configFile.toStdString() << std::endl;
        cameraCalibration->loadFromFile(configFile.toStdString().c_str());
    }
}


int SingleAravisCamera::getImageROIwidth() {
    try {
        if(camera.Width.IsReadable()) {
            return static_cast<int>(camera.Width.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleAravisCamera::getImageROIheight() {
    try {
        if (camera.Height.IsReadable()) {
            return static_cast<int>(camera.Height.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleAravisCamera::getImageROIoffsetX() {
    try {
        if(camera.OffsetX.IsReadable()) {
            return static_cast<int>(camera.OffsetX.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

int SingleAravisCamera::getImageROIoffsetY() {
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
int SingleAravisCamera::getImageROIwidthMax() {
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
int SingleAravisCamera::getImageROIheightMax() {
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

QRectF SingleAravisCamera::getImageROI(){
    return QRectF(getImageROIoffsetX(),getImageROIoffsetY(),getImageROIwidth(), getImageROIheight());
}

int SingleAravisCamera::getBinningVal() {
    //if(camera.BinningHorizontal.GetValue()!=camera.BinningVertical.GetValue())
    //    return 0;
    try {
        return camera.BinningHorizontal.GetValue();
    }
    catch (const GenericException &e) {
        return 1;
    }
}

double SingleAravisCamera::getTemperature() {
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

bool SingleAravisCamera::isGrabbing()
{
    return camera.IsGrabbing();
}

// NOTE: grabbing "pause" is necessary for setting binning
bool SingleAravisCamera::setBinningVal(int value) {

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
bool SingleAravisCamera::setImageROIwidth(int width) {
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
bool SingleAravisCamera::setImageROIheight(int height) {
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
bool SingleAravisCamera::setImageROIoffsetX(int offsetX) {
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
bool SingleAravisCamera::setImageROIoffsetY(int offsetY) {
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

*/