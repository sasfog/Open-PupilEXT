
#include "singleCamera.h"
#include "camTempMonitor.h"
#include <QThread>
#include <QDebug>

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
        throw std::runtime_error("Camera connection problem.");
        //return;
    }

    bool cfound = false;
    CDeviceInfo di;
    Pylon::DeviceInfoList_t::const_iterator deviceIt;
    for (deviceIt = allDevices.begin(); deviceIt != allDevices.end(); ++deviceIt) {

        // TODO: regexp?
        QString matchableFoundDeviceID = deviceIt->GetFriendlyName().c_str();
        matchableFoundDeviceID.replace(" ", "");
        matchableFoundDeviceID.replace("-", "");
        matchableFoundDeviceID.replace("(", "");
        matchableFoundDeviceID.replace(")", "");
        matchableFoundDeviceID = matchableFoundDeviceID.toLower();

        QString matchableTargetDeviceID = friendlyName;
        matchableTargetDeviceID.replace(" ", "");
        matchableTargetDeviceID.replace("-", "");
        matchableTargetDeviceID.replace("(", "");
        matchableTargetDeviceID.replace(")", "");
        matchableTargetDeviceID = matchableTargetDeviceID.toLower();

        if(matchableFoundDeviceID == matchableTargetDeviceID) {
            di = *deviceIt;
            cfound = true;
            break;
        }
    }
    if(!cfound) {
        throw std::runtime_error("The specified camera was not found among the ones currently detected.");
    }
    //bool cfound = false;
    //CDeviceInfo di;
    //Pylon::DeviceInfoList_t::const_iterator deviceIt;
    //for (deviceIt = allDevices.begin(); deviceIt != allDevices.end(); ++deviceIt) {
    //    qDebug() << "deviceIt->GetFriendlyName().c_str() " << deviceIt->GetFriendlyName().c_str();
    //    if(deviceIt->GetFriendlyName().c_str() == friendlyName) {
    //        //qDebug() << "FOUND";
    //        cfound = true;
    //        di = *deviceIt;
    //        break;
    //    }
    //}
    //if(!cfound) {
    //    throw std::runtime_error("The specified camera was not found among the ones currently detected.");
    //}

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

        camera.PixelFormat.TrySetValue(PixelFormat_Mono8);
        enableSensorLevelBinningIfPossible();

        // load calibration if existing
        if(!cameraCalibration->isCalibrated()) {
            // If we already used this camera before, a config file may exists
            loadCalibrationFile();
        }

        CIntegerParameter heartbeat( camera.GetTLNodeMap(), "HeartbeatTimeout" );
        heartbeat.TrySetValue( 1000, IntegerValueCorrection_Nearest );

        // TODO: Revise if this check is needed at ll! If really needed, make it hang on a
        //  thread, or whatever, but do not obstruct grabbing if not needed to
        //if(camera.CanWaitForFrameTriggerReady()) {
            startGrabbing();
        //} else {
        //    std::cout << "Cameras can not be queried whether it is ready to accept the next frame trigger.";
        //}
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
    qCritical() << "A Pylon exception occurred." << Qt::endl << e.GetDescription();
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
    qDebug() << "SingleCamera: Enabling Hardware trigger to line source: " << lineSource << " to state: " << state;

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
            hardwareTriggerConfiguration = new HardwareTriggerConfiguration(lineSource.toStdString().c_str());
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

        // TODO: Revise if this check is needed at ll! If really needed, make it hang on a
        //  thread, or whatever, but do not obstruct grabbing if not needed to
        //if (camera.CanWaitForFrameTriggerReady()) {
            startGrabbing();
        //} else {
        //    std::cout << "Camera can not be queried whether it is ready to accept the next frame trigger.";
        //}
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

bool SingleCamera::isAutoGainAvailable() {
    return (camera.GainAuto.IsReadable() && camera.GainAuto.IsWritable() && camera.GainAuto.CanSetValue(GainAuto_Once));
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

                std::cout << "Starting AutoGain...";

                //camera.Gain.SetToMaximum();
                //camera.Gain.TrySetToMaximum();

                if (!camera.GainAuto.IsWritable()) {
                    std::cout << "The camera does not support Gain Auto.";
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

                    camera.AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI1);
                    camera.AutoFunctionROIUseBrightness.TrySetValue(true);   // ROI 1 is used for brightness control
                    camera.AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI2);
                    camera.AutoFunctionROIUseBrightness.TrySetValue(false);   // ROI 2 is not used for brightness control

                    // Set the ROI (in this example the complete sensor is used)
                    camera.AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI1);  // configure ROI 1
                    camera.AutoFunctionROIOffsetX.TrySetValue(camera.OffsetX.GetMin());
                    camera.AutoFunctionROIOffsetY.TrySetValue(camera.OffsetY.GetMin());
                    camera.AutoFunctionROIWidth.TrySetValue(camera.Width.GetMax());
                    camera.AutoFunctionROIHeight.TrySetValue(camera.Height.GetMax());
                }

                if (camera.GetSfncVersion() >= Sfnc_2_0_0) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    // Set the target value for luminance control.
                    // A value of 0.3 means that the target brightness is 30 % of the maximum brightness of the raw pixel value read out from the sensor.
                    // A value of 0.4 means 40 % and so forth.
                    camera.AutoTargetBrightness.TrySetValue(0.3);

                    // We are going to try GainAuto = Once.

                    std::cout << "Trying 'GainAuto = Once'.";
                    std::cout << "Initial Gain = " << camera.Gain.GetValue();

                    // Set the gain ranges for luminance control.
                    camera.AutoGainLowerLimit.TrySetValue(camera.Gain.GetMin());
                    camera.AutoGainUpperLimit.TrySetValue(camera.Gain.GetMax());
                }

                camera.GainAuto.TrySetValue(GainAuto_Once);

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

                std::cout << "GainAuto went back to 'Off' after " << n << " frames.";
                if(camera.Gain.IsReadable()) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    std::cout << "Final Gain = " << camera.Gain.GetValue();
                }

                startGrabbing();
            }
        } else {
            qWarning() << "Only area scan cameras support auto functions.";
        }
    } catch (const TimeoutException &e) {
        // Auto functions did not finish in time.
        // Maybe the cap on the lens is still on or there is not enough light.
        qWarning() << "A timeout has occurred: " << Qt::endl << e.GetDescription();
        qWarning() << "Please make sure you remove the cap from the camera lens before running auto gain.";
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

bool SingleCamera::isAutoExposureAvailable() {
    return (camera.ExposureAuto.IsReadable() && camera.ExposureAuto.IsWritable() && camera.ExposureAuto.CanSetValue(ExposureAuto_Once));
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
                    std::cout << "The camera does not support Exposure Auto.";
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
                    camera.AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI1);
                    camera.AutoFunctionROIUseBrightness.TrySetValue(true);   // ROI 1 is used for brightness control
                    camera.AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI2);
                    camera.AutoFunctionROIUseBrightness.TrySetValue(false);   // ROI 2 is not used for brightness control

                    // Set the ROI (in this example the complete sensor is used)
                    camera.AutoFunctionROISelector.TrySetValue(AutoFunctionROISelector_ROI1);  // configure ROI 1
                    camera.AutoFunctionROIOffsetX.TrySetValue(camera.OffsetX.GetMin());
                    camera.AutoFunctionROIOffsetY.TrySetValue(camera.OffsetY.GetMin());
                    camera.AutoFunctionROIWidth.TrySetValue(camera.Width.GetMax());
                    camera.AutoFunctionROIHeight.TrySetValue(camera.Height.GetMax());
                }

                if (camera.GetSfncVersion() >= Sfnc_2_0_0) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    // Set the target value for luminance control.
                    // A value of 0.3 means that the target brightness is 30 % of the maximum brightness of the raw pixel value read out from the sensor.
                    // A value of 0.4 means 40 % and so forth.
                    camera.AutoTargetBrightness.TrySetValue(0.3);

                    // Try ExposureAuto = Once.
                    qDebug() << "Trying 'ExposureAuto = Once'.";
                    qDebug() << "Initial exposure time = " << camera.ExposureTime.GetValue() << " us";

                    // Set the exposure time ranges for luminance control.
                    camera.AutoExposureTimeLowerLimit.TrySetValue(camera.AutoExposureTimeLowerLimit.GetMin());
                    camera.AutoExposureTimeUpperLimit.TrySetValue(camera.AutoExposureTimeLowerLimit.GetMax());

                    camera.ExposureAuto.TrySetValue(ExposureAuto_Once);
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

                qDebug() << "ExposureAuto went back to 'Off' after " << n << " frames.";
                qDebug() << "Final exposure time = ";

                if (camera.ExposureTime.IsReadable()) // Cameras based on SFNC 2.0 or later, e.g., USB cameras
                {
                    qDebug() << camera.ExposureTime.GetValue() << " us";
                }

                startGrabbing();
            }
        } else {
            qWarning() << "Only area scan cameras support auto functions.";
        }
    } catch (const TimeoutException &e) {
        // Auto functions did not finish in time.
        // Maybe the cap on the lens is still on or there is not enough light.
        qWarning() << "A timeout has occurred: " << e.GetDescription();
        qWarning() << "Please make sure you remove the cap from the camera lens before running this sample.";
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

            // TODO: do this properly, and add a GUI tickbox for Continous auto vs Auto once and the spinbox.
            //  Also correct Aravis implementation for this
            GenApi_3_1_Basler_pylon_v3::INodeMap& nodemap = camera.GetNodeMap();
            //CEnumParameter(nodemap, "ExposureAuto").TrySetValue("Continuous");
            CEnumParameter(nodemap, "GainAuto").TrySetValue("Off");

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

            // TODO: do this properly, and add a GUI tickbox for Continous auto vs Auto once and the spinbox.
            //  Also correct Aravis implementation for this
            GenApi_3_1_Basler_pylon_v3::INodeMap& nodemap = camera.GetNodeMap();
            //CEnumParameter(nodemap, "ExposureAuto").TrySetValue("Continuous");
            CEnumParameter(nodemap, "ExposureAuto").TrySetValue("Off");

            camera.ExposureTime.TrySetValue(value);
        }
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
}

void SingleCamera::loadFromFile(const QString &filename) {
    try {
        CFeaturePersistence::Load( filename.toStdString().c_str(), &camera.GetNodeMap(), true );
    } catch (const GenericException &e) {
        // Error handling.
        qCritical() << "An exception occurred: " << e.GetDescription();
    }

    if(camera.TriggerMode.GetValueOrDefault("Off") == "On") {
        lineSource = camera.TriggerSource.GetValueOrDefault("Line1");
        enableHardwareTrigger(true);
    }
}

void SingleCamera::saveToFile(const QString &filename) {
    try {
        CFeaturePersistence::Save(filename.toStdString().c_str(), &camera.GetNodeMap() );
    } catch (const GenericException &e) {
        // Error handling.
        qCritical() << "An exception occurred: " << e.GetDescription();
    }
}

bool SingleCamera::isEnabledAcquisitionFrameRate() {
    try {
        if (isAcquisitionFrameRateAvailable()) {
            return camera.AcquisitionFrameRateEnable.GetValue();
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return false;
}

bool SingleCamera::isAcquisitionFrameRateAvailable() {
    try {
        // TODO: IF THE CAMERA TYPE IS ON WHITELIST, FORCEFULLY RETURN TRUE
        //  e.g. daA1280-54um returns false properly, but still it works surprisingly if we forcefully set
        //  aquisition framerate. So a whitelist could be used, and that camera added to it at least

        if (camera.AcquisitionFrameRateEnable.IsReadable() && camera.AcquisitionFrameRateEnable.IsWritable()) {
            return true;
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

    // IMPORTANT: For some strange reason, there are cameras (e.g. Basler puA1280-54um) where the
    //  AcquisitionFrameRateEnable can not be set to true, thus the acquisition frame rate should be unusable,
    //  however setting the AcquisitionFrameRate value takes effect. And in these cases, even if we set it to false,
    //  the silently enabled value will remain, limiting the FPS. On top of that, we have to set the FPS value before
    //  setting the enable value to false. It is like as if the feature was still available, but we cannot get
    //  informed about its state from the outside.

    try {
        if(!enabled && camera.AcquisitionFrameRate.IsWritable())
            camera.AcquisitionFrameRate.TrySetValue(9999);

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

    qInfo() << "Camera Synchronize Time";
    qInfo() << "=========================";
    qInfo() << "Timestamp Camera: " << cameraTime;
    qInfo() << "Timestamp System: " << systemTime;
    qInfo() << "System Epoch: " << std::ctime(&epochTime);
    qInfo() << "System Time: " << std::ctime(&startTime);
    qInfo() << "Time from Epoch (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    qInfo() << "Time from Epoch (us): " << std::chrono::duration_cast<std::chrono::microseconds>(start.time_since_epoch()).count();
    qInfo() << "=========================";
}

QString SingleCamera::getLineSource() {
    return lineSource;
}

void SingleCamera::setLineSource(QString value) {
    lineSource = value;
}

CameraImageType SingleCamera::getType() {
    return CameraImageType::LIVE_SINGLE_CAMERA;
}

void SingleCamera::startGrabbing() {
    // TODO: start grabbing only if frame trigger redy? But really needed?

    if (camera.IsOpen() && !camera.IsGrabbing())
        camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
}

void SingleCamera::stopGrabbing() {
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
        qDebug() << "Found calibration file in settings directory. Loading: " << configFile.toStdString();
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

int SingleCamera::getImageROIoffsetXInc() {
    try {
        if(camera.OffsetX.IsReadable()) {
            return static_cast<int>(camera.OffsetX.GetInc());
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

int SingleCamera::getImageROIoffsetYInc() {
    try {
        if (camera.OffsetY.IsReadable()) {
            return static_cast<int>(camera.OffsetY.GetInc());
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

int SingleCamera::getImageROIwidthInc() {
    try {
        if(camera.Width.IsReadable()) {
            return static_cast<int>(camera.Width.GetInc());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 16;
}

// NOTE: Binning affects this
int SingleCamera::getImageROIheightMax() {
    // Classic/U/L GigE cameras
  //  return (int)camera.Height.GetMax();
    // other cameras
    try {
        if (camera.HeightMax.IsReadable()) {
            return static_cast<int>(camera.HeightMax.GetValue());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 0;
}

// NOTE: Binning affects this
int SingleCamera::getImageROIheightInc() {
    try {
        if (camera.Height.IsReadable()) {
            return static_cast<int>(camera.Height.GetInc());
        }
    } catch(const GenericException &e) {
        genericExceptionOccured(e);
    }
    return 16;
}

QRectF SingleCamera::getImageROI(){
    return QRectF(getImageROIoffsetX(),getImageROIoffsetY(),getImageROIwidth(), getImageROIheight());
}

bool SingleCamera::isBinningAvailable() {

    bool val = false;

    try {
        bool wasGrabbing = false;
        if(camera.IsGrabbing()) {
            wasGrabbing = true;
            camera.StopGrabbing();
        }

        // IMPORTANT: there are camera models (e.g. Basler puA1280-54um) where the Pylon API will tell that the
        //  Horizontal Binning levels are minimum 1 and maxumim 2, however the Vertical Binning level is only 1.
        //  This is not an error in the APi, it is because the camera only supports the following binning
        //  combinations (H x V): 1x1, 2x1, 2x2 and the vertical 2 option will only show if the Horizontal
        //  is set to 2 already, as the 1x1 is not supported. So the y axis binning does not have to be checked here.

        // NOTE: .GetListOfValidValues().size() does not work here
        if( camera.BinningHorizontal.IsReadable() &&
            camera.BinningHorizontal.IsWritable() &&
            (camera.BinningHorizontal.GetMax() != camera.BinningHorizontal.GetMin()) ) {

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

int SingleCamera::getBinningMax() {
    //if(camera.BinningHorizontal.GetValue()!=camera.BinningVertical.GetValue())
    //    return 0;
    try {
        return camera.BinningHorizontal.GetMax();
    }
    catch (const GenericException &e) {
        return 1;
    }
}

bool SingleCamera::isTemperatureReadingSupported() {
    try {
        camera.DeviceTemperatureSelector.TrySetValue(Basler_UniversalCameraParams::DeviceTemperatureSelectorEnums::DeviceTemperatureSelector_Coreboard);

        if(camera.DeviceTemperature.IsReadable() && camera.DeviceTemperature.GetValue() > CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE)
            return true;
    } catch (const GenericException &e) {
        genericExceptionOccured(e);
    }
    return false;
}

double SingleCamera::getTemperature() {
    double d = CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE;
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

void SingleCamera::enableSensorLevelBinningIfPossible() {

    // Binning***Mode_Average setting is only possible if the BinningSelector setting is not in Sensor mode
    //  and Binning***Mode_Sum only really has a meaning if BinningSelector_Sensor is the case, to compensate
    //  image brightness lost due to smaller area per "pixel", i.e. to retain same brightness even if switched
    //  to binning level 2 or 4 later.
    //  So we set Binning***Mode_Sum only if BinningSelector_Sensor is the case, and to
    //  Binning***Mode_Average if BinningSelector is set otherwise.

    // Try enable sensor level binning, to improve max possible FPS
    if(camera.BinningSelector.IsWritable() && camera.BinningSelector.CanSetValue(BinningSelector_Sensor)) {
        if(camera.BinningHorizontalMode.IsWritable() && camera.BinningHorizontalMode.CanSetValue(BinningHorizontalMode_Sum)) {
            camera.BinningHorizontalMode.TrySetValue(BinningHorizontalMode_Sum);
        }
        if(camera.BinningVerticalMode.IsWritable() && camera.BinningVerticalMode.CanSetValue(BinningVerticalMode_Sum)) {
            camera.BinningVerticalMode.TrySetValue(BinningVerticalMode_Sum);
        }

        camera.BinningSelector.TrySetValue(BinningSelector_Sensor);
    } else {
        if(camera.BinningHorizontalMode.IsWritable() && camera.BinningHorizontalMode.CanSetValue(BinningHorizontalMode_Average)) {
            camera.BinningHorizontalMode.TrySetValue(BinningHorizontalMode_Average);
        }
        if(camera.BinningVerticalMode.IsWritable() && camera.BinningVerticalMode.CanSetValue(BinningVerticalMode_Average)) {
            camera.BinningVerticalMode.TrySetValue(BinningVerticalMode_Average);
        }
    }
}

// NOTE: grabbing "pause" is necessary for setting binning
bool SingleCamera::setBinningVal(int value) {

    bool success = false;

    if(camera.IsGrabbing())
        camera.StopGrabbing();

    // IMPORTANT: Horizontal binning has to be set first

    // in case of our Basler cameras here, only mode=1,2,4 are only valid values
    if(isBinningAvailable()) {

        enableSensorLevelBinningIfPossible();

        if(value==2 || value==3) {
            success = camera.BinningHorizontal.TrySetValue(2) &&
                    camera.BinningVertical.TrySetValue(2);
            qDebug() << "Setting binning to 2 on both axes";
        } else if(value==4) {
            success = camera.BinningHorizontal.TrySetValue(4) &&
                    camera.BinningVertical.TrySetValue(4);
            qDebug() << "Setting binning to 4 on both axes";
        } else { //if(value==1) {
            success = camera.BinningHorizontal.TrySetValue(1) &&
                    camera.BinningVertical.TrySetValue(1);
            qDebug() << "Setting binning to 1 (no binning) on both axes";
        }
    }
    startGrabbing();
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool SingleCamera::setImageROIwidth(int width) {
    //qDebug() << "Setting Image ROI width=" << std::to_string(width);
    bool success = false;

    if(camera.IsGrabbing())
        camera.StopGrabbing();

    // ace Classic/U/L GigE Cameras
   // int maxWidth = camera.Width.GetMax();
   // int maxHeight = camera.Height.GetMax();
    // other cameras
    int maxWidth = getImageROIwidthMax();
    int offsetX = getImageROIoffsetX();

    if(width < camera.Width.GetMin())
        width = camera.Width.GetMin();

    int modVal = width % camera.Width.GetInc();
    if(modVal != 0)
        width -= modVal;

    int bestWidth = (offsetX+width > maxWidth) ? maxWidth-offsetX-((maxWidth-offsetX) % camera.Width.GetInc()) : width;
//    if (offsetX >= maxWidth-16)
//        width = maxWidth-offsetX;

    if (camera.Width.IsWritable() ) {
        success = camera.Width.TrySetValue(bestWidth);
    }
    startGrabbing();
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool SingleCamera::setImageROIheight(int height) {
    //qDebug() << "Setting Image ROI height=" << std::to_string(height);
    bool success = false;

    if(camera.IsGrabbing())
        camera.StopGrabbing();

    // ace Classic/U/L GigE Cameras
   // int maxWidth = camera.Width.GetMax();
   // int maxHeight = camera.Height.GetMax();
    // other cameras
    int maxHeight = getImageROIheightMax();
    int offsetY = getImageROIoffsetY();

    if(height < camera.Height.GetMin())
        height = camera.Height.GetMin();

    int modVal = height % camera.Height.GetInc();
    if(modVal != 0)
        height -= modVal;

    int bestHeight = (offsetY+height > maxHeight) ? maxHeight-offsetY-((maxHeight-offsetY) % camera.Height.GetInc()) : height;
//    if (offsetY >= maxHeight-16)
//        height = maxHeight-offsetY;

    if (camera.Height.IsWritable() ) {
        success = camera.Height.TrySetValue(bestHeight);
    } 
    startGrabbing();
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool SingleCamera::setImageROIoffsetX(int offsetX) {
    //qDebug() << "Setting Image ROI offsetX=" << std::to_string(offsetX);
    bool success = false;

    // NOTE: LIKELY the offsetX and offsetY (unlike height and width) could even be set on most cameras during
    //  grabbing as well, but it is not guaranteed. So yet we simply use the safest solution, stop, then change, then
    //  (re)start grabbing. But if you really want to play around with this to achieve a sliding window ROI or whatever
    //  for highspeed eye detection (the way SMI likely does this anyway), feel free to try. Expo timing could fail btw
    if(camera.IsGrabbing())
        camera.StopGrabbing();

    // ace Classic/U/L GigE Cameras
   // int maxWidth = camera.Width.GetMax();
   // int maxHeight = camera.Height.GetMax();
    // other cameras
    int maxWidth = getImageROIwidthMax();
    int width = getImageROIwidth();

    if(maxWidth - offsetX < camera.OffsetX.GetInc())
        offsetX = maxWidth - camera.OffsetX.GetInc();
    //if(width + offsetX > maxWidth)
    //    return;
    
    int modVal = offsetX % camera.OffsetX.GetInc();
    if(modVal != 0)
        offsetX -= modVal;

    if (width + offsetX <= maxWidth && camera.OffsetX.IsWritable() ) {
        success = camera.OffsetX.TrySetValue(offsetX);
    }
    startGrabbing();
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool SingleCamera::setImageROIoffsetY(int offsetY) {
    //qDebug() << "Setting Image ROI offsetY=" << std::to_string(offsetY);
    bool success = false;

    // NOTE: LIKELY the offsetX and offsetY (unlike height and width) could even be set on most cameras during
    //  grabbing as well, but it is not guaranteed. So yet we simply use the safest solution, stop, then change, then
    //  (re)start grabbing. But if you really want to play around with this to achieve a sliding window ROI or whatever
    //  for highspeed eye detection (the way SMI likely does this anyway), feel free to try. Expo timing could fail btw
    if(camera.IsGrabbing())
        camera.StopGrabbing();

    // ace Classic/U/L GigE Cameras
   // int maxWidth = camera.Width.GetMax();
   // int maxHeight = camera.Height.GetMax();
    // other cameras
    int maxHeight = getImageROIheightMax();
    int height = getImageROIheight();

    if(maxHeight - offsetY < camera.OffsetY.GetInc())
        offsetY = maxHeight - camera.OffsetY.GetInc();
    //if(height + offsetY > maxHeight)
    //    return;

    int modVal=offsetY % camera.OffsetY.GetInc();
    if(modVal != 0)
        offsetY -= modVal;

    if (height + offsetY <= maxHeight && camera.OffsetY.IsWritable() ) {
        success = camera.OffsetY.TrySetValue(offsetY);
    } 
    startGrabbing();
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
        throw std::runtime_error("Camera connection problem.");
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
        matchableFoundDeviceID = matchableFoundDeviceID.toLower();

        QString matchableTargetDeviceID = friendlyName;
        matchableTargetDeviceID.replace(" ", "");
        matchableTargetDeviceID.replace("-", "");
        matchableTargetDeviceID.replace("(", "");
        matchableTargetDeviceID.replace(")", "");
        matchableTargetDeviceID = matchableTargetDeviceID.toLower();

        if(matchableFoundDeviceID == matchableTargetDeviceID)
            break;
        i++;
    }
    if(i >= n) {
        throw std::runtime_error("The specified camera was not found among the ones currently detected.");
        //return;
    }

    GError *error = NULL;
    camera = arv_camera_new(arv_get_device_id(i), &error);

    if(!ARV_IS_CAMERA(camera)) {

        // TODO: this sometimes happens with USB3 - just this happens:
        //  "Failed to bootstrap USB device '(NULL)-(NULL)-(NULL)-267601593BB0'"
        //  I think either cold reset /power cylcing or unplug-replug can help, maybe driver detach-reattach also
        emit manualDeviceResetNecessary();
        qDebug() << "-----------------------------------------------------------------";

        g_clear_object (&camera);
        camera = nullptr;

        throw std::runtime_error("Could not initialize Aravis camera.");
        //return;
    }
    if(error != NULL) {
        throw std::runtime_error(error->message);
    }

    qDebug() << "To our best knowledge, the camera was successfully opened: " << arv_get_device_id(i);
    size_t xmls;
    qDebug() << "GenICam XML:\n" << arv_device_get_genicam_xml(arv_camera_get_device(camera), &xmls);





    GValue v = G_VALUE_INIT;
    auto device_type = arv_device_get_type();
    error = nullptr;
    if(device_type == ARV_TYPE_UV_DEVICE) {
        qDebug() << "This is a USB3 camera.";
    } else {
        // NOTE: if(device_type == ARV_TYPE_GV_DEVICE) does not always work. I dont know why.
        //  We assume it is always GigE if not USB3
        qDebug() << "This is LIKELY a GigE camera.";
        //if(arv_device_is_feature_available(arv_camera_get_device(camera), "AcquisitionStatusSelector", NULL)) {
        //    error = nullptr;
        //    arv_device_get_feature_value(arv_camera_get_device(camera), "AcquisitionStatusSelector", &v, NULL);
        //    auto vs = g_value_get_string(&v);
        //    qDebug() << "AcquisitionStatus = " << vs;
        //}
        //if(arv_device_is_feature_available(arv_camera_get_device(camera), "AcquisitionStatus", NULL)) {
        //    error = nullptr;
        //    arv_device_get_feature_value(arv_camera_get_device(camera), "AcquisitionStatus", &v, NULL);
        //    auto vs = g_value_get_boolean(&v);
        //    qDebug() << "AcquisitionStatus = " << vs;
        //}
        if(arv_device_is_feature_available(arv_camera_get_device(camera), "GevCCP", NULL)) {
            arv_device_get_feature_value(arv_camera_get_device(camera), "GevCCP", &v, NULL);
            auto vs = g_value_get_string(&v);
            qDebug() << "GevCCP = " << vs;
        }
    }

    error = nullptr;
    arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error);
    if(error){
        qDebug() << "Could not set ARV_ACQUISITION_MODE_CONTINUOUS.";
        qDebug() << "Error during aravis API call. Message: " << error->message;
    }
    //auto am = arv_camera_get_acquisition_mode(camera, &error);
    //switch(am) {
    //    case ARV_ACQUISITION_MODE_CONTINUOUS:
    //        qDebug() << "ARV_ACQUISITION_MODE_CONTINUOUS";
    //        break;
    //    case ARV_ACQUISITION_MODE_MULTI_FRAME:
    //        qDebug() << "ARV_ACQUISITION_MODE_MULTI_FRAME";
    //        break;
    //    case ARV_ACQUISITION_MODE_SINGLE_FRAME:
    //        qDebug() << "ARV_ACQUISITION_MODE_SINGLE_FRAME";
    //        break;
    //}

    if(arv_device_is_feature_available(arv_camera_get_device(camera), "GevHeartbeatTimeout", NULL)) {

        //// DEV: disable heartbeat, not to lose control
        ////arv_device_set_string_feature_value(device, "TriggerSelector", "FrameStart", &error);
        arv_device_set_integer_feature_value(arv_camera_get_device(camera), "GevHeartbeatTimeout", 5000, &error);
        ////arv_device_set_string_feature_value(arv_camera_get_device(camera), "GevGVCPHeartbeatDisable", "False", &error);
        //arv_device_set_integer_feature_value(arv_camera_get_device(camera), "DeviceLinkHeartbeatTimeout", 1000, &error);
        //arv_device_set_string_feature_value(arv_camera_get_device(camera), "DeviceLinkHeartbeatMode", "Off", &error);

        GValue s = G_VALUE_INIT;
        arv_device_get_feature_value(arv_camera_get_device(camera), "GevHeartbeatTimeout", &v, NULL);
        auto si = g_value_get_int(&s);
        qDebug() << "GevHeartbeatTimeout = " << si;
    }

    if(arv_device_is_feature_available(arv_camera_get_device(camera), "BinningModeHorizontal", NULL)) {
        arv_device_set_string_feature_value(arv_camera_get_device(camera), "BinningModeHorizontal", "Averaging", &error);
    }
    if(arv_device_is_feature_available(arv_camera_get_device(camera), "BinningModeVertical", NULL)) {
        arv_device_set_string_feature_value(arv_camera_get_device(camera), "BinningModeVertical", "Averaging", &error);
    }

    if(arv_device_is_feature_available(arv_camera_get_device(camera), "PixelFormat", NULL)) {
        arv_device_set_string_feature_value(arv_camera_get_device(camera), "PixelFormat", "Mono8", &error);
    }
    enableSensorLevelBinningIfPossible();

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

    startGrabbing();

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

    // TODO: manual reset
    // TODO: reset when error occurs
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

    // TODO: UNNECESSARY
    stopGrabbing();

    GError *error = nullptr;

    // Create the stream object with callback
    callbackData.stream = arv_camera_create_stream(camera, cameraImageEventHandler->stream_callback, &callbackData, &error);

    if (ARV_IS_STREAM (callbackData.stream)) {
        int i;
        size_t payload;

        // Retrieve the payload size for buffer creation
        error = nullptr;
        payload = arv_camera_get_payload(camera, &error);
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

void SingleCamera::genericExceptionOccured(const std::exception &e, const GError &lastAravisError) {
    //QThread::msleep(1000);
    qCritical() << "An Aravis exception occurred." << Qt::endl << e.what();

    // TODO: sketchy check if the device was removed or not
    bool deviceRemoved = false;
    if(QString::fromStdString(lastAravisError.message).toLower().contains("remov")) {
        deviceRemoved = true;
    }

    genericExceptionOccured(e, deviceRemoved);
}

void SingleCamera::genericExceptionOccured(const std::exception &e, bool deviceRemoved) {
    //QThread::msleep(1000);
    qCritical() << "An Aravis exception occurred." << Qt::endl << e.what();

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
    qDebug() << "SingleCamera: Releasing resources.";

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

        ////auto a = arv_camera_get_acquisition_mode(camera, &error);
        ////auto t = arv_camera_get_trigger_source(camera, &error);
        //bool isSoftwareTriggerSupported = arv_camera_is_software_trigger_supported(camera, &error);
        auto device = arv_camera_get_device(camera);

        // TODO: set line source if not set
        //setLineSource(lineSource);

        //if(state)
            arv_device_set_string_feature_value(device, "TriggerSelector", "FrameStart", &error);
        //else
        //    arv_device_set_string_feature_value(device, "TriggerSelector", "AcquisitionStart", &error);

        if(error){
            qDebug() << "Could not set TriggerSelector to value FrameStart.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }

        //auto b = arv_acquisition_mode_from_string("Continuous");
        //if(!error)
        error = nullptr;
        arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error);
        if(error){
            qDebug() << "Could not set ARV_ACQUISITION_MODE_CONTINUOUS.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }

        std::string stateStr = (state) ? "On" : "Off";
        error = nullptr;
        arv_device_set_string_feature_value(device, "TriggerMode", stateStr.c_str(), &error);
        if(error){
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }

        if(!state) {
            error = nullptr;
            arv_camera_software_trigger(camera, &error);
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

}

bool SingleCamera::isAutoGainAvailable() {
    GError *error = nullptr;
    bool val = false;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        val = arv_camera_is_gain_auto_available(camera, &error);
        if(error) {
            qDebug() << "Could not get whether auto gain setting is available, assuming not.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

bool SingleCamera::isAutoExposureAvailable() {
    GError *error = nullptr;
    bool val = false;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        val = arv_camera_is_exposure_auto_available(camera, &error);
        if(error) {
            qDebug() << "Could not get whether auto exposure setting is available, assuming not.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

void SingleCamera::autoGainOnce() {
    try {
        GError *error = nullptr;

        if(!camera) {
            return;
        }

        stopGrabbing();

        bool isAuto = arv_camera_is_gain_auto_available(camera, &error);

        if(error) {
            qDebug() << "Auto gain is not available.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(isAuto) {
            error = nullptr;
            arv_camera_set_gain_auto(camera, ArvAuto::ARV_AUTO_ONCE, &error);
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

void SingleCamera::autoExposureOnce() {
    try {
        GError *error = nullptr;

        if(!camera) {
            return;
        }

        stopGrabbing();

        bool isAuto = arv_camera_is_exposure_auto_available(camera, &error);

        if(error) {
            qDebug() << "Auto exposure is not available.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(isAuto) {
            error = nullptr;
            arv_camera_set_exposure_time_auto(camera, ArvAuto::ARV_AUTO_ONCE, &error);
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
            error = nullptr;
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
            error = nullptr;
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
            error = nullptr;
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
    double val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_gain_available(camera, &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            error = nullptr;
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
    double val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_gain_available(camera, &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            error = nullptr;
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
    double val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_gain_available(camera, &error);
        if(canGet) {
            // Here we round (to integer amount in microseconds. Should be fine enough I think)
            double valMin = 0;
            double valMax = 0;
            error = nullptr;
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
            error = nullptr;
            arv_camera_get_gain_bounds(camera, &valMin, &valMax, &error);
        }
        if(error) {
            qDebug() << "Could not get gain value bounds.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(value <= valMax && value >= valMin) {
            error = nullptr;
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
            error = nullptr;
            arv_camera_get_exposure_time_bounds(camera, &valMin, &valMax, &error);
        }
        if(error) {
            qDebug() << "Could not get exposure time value bounds.";
            qDebug() << "Error during aravis API call. Message: " << error->message;
        } else if(value <= valMax && value >= valMin) {
            error = nullptr;
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

void SingleCamera::loadFromFile(const QString &filename) {
    // TODO: It seems aravis does not yet support saving and loading all features. We could iterate through the map
    //  and save what we can, then restore all upon opening, but this requires further larger efforts. Yet unsupported
    /*
    try {
        CFeaturePersistence::Load( filename, &camera.GetNodeMap(), true );
    } catch (const GenericException &e) {
        // Error handling.
        qCritical() << "An exception occurred: " << e.GetDescription();
    }

    if(camera.TriggerMode.GetValueOrDefault("Off") == "On") {
        lineSource = camera.TriggerSource.GetValueOrDefault("Line1");
        enableHardwareTrigger(true);
    }
     */
}

void SingleCamera::saveToFile(const QString &filename) {
    // TODO: It seems aravis does not yet support saving and loading all features. We could iterate through the map
    //  and save what we can, then restore all upon opening, but this requires further larger efforts. Yet unsupported
    /*
    try {
        CFeaturePersistence::Save(filename, &camera.GetNodeMap() );
    } catch (const GenericException &e) {
        // Error handling.
        qCritical() << "An exception occurred: " << e.GetDescription();
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
            qWarning() << "Could not get whether acquisition frame rate setting is enabled or not.";
            qWarning() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

bool SingleCamera::isEmulated() {
    // TODO
    return (getFriendlyName().toLower().contains("emu"));
}

void SingleCamera::enableAcquisitionFrameRate(bool enabled) {
    GError *error = nullptr;

    if(!ARV_IS_CAMERA(camera))
        return;

    try {
        arv_camera_set_frame_rate_enable(camera, enabled, &error);
        if(error) {
            qWarning() << "Could not set acquisition frame rate enabled/disabled.";
            qWarning() << "Error during aravis API call. Message: " << error->message;
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
            qWarning() << "Acquisition framerate not available.";
            qWarning() << "Error during aravis API call. Message: " << error->message;
        } else if(canGet) {
            error = nullptr;
            arv_camera_set_frame_rate(camera, (double)value, &error);
            if(error) {
                qWarning() << "Could not set acquisition framerate.";
                qWarning() << "Error during aravis API call. Message: " << error->message;
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
            qWarning() << "Acquisition framerate not available.";
            qWarning() << "Error during aravis API call. Message: " << error->message;
        } else if(canGet) {
            // NOTE: rounding here
            error = nullptr;
            val = (int)round(arv_camera_get_frame_rate(camera, &error));
            if(error) {
                qWarning() << "Could not get acquisition framerate.";
                qWarning() << "Error during aravis API call. Message: " << error->message;
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
            error = nullptr;
            arv_camera_get_frame_rate_bounds(camera, &valMin, &valMax, &error);
            // extra checks could happen here
            val = (int)round(valMin);
        }
        if(error) {
            qWarning() << "Error during aravis API call. Message: " << error->message;
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
            error = nullptr;
            arv_camera_get_frame_rate_bounds(camera, &valMin, &valMax, &error);
            // extra checks could happen here
            val = (int)round(valMax);
        }
        if(error) {
            qWarning() << "Error during aravis API call. Message: " << error->message;
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


        if(arv_device_is_feature_available(arv_camera_get_device(camera), "ResultingFrameRate", NULL)) {
            arv_device_get_feature_value(arv_camera_get_device(camera), "ResultingFrameRate", &v, &error);
        } else if(arv_device_is_feature_available(arv_camera_get_device(camera), "ResultingFrameRateAbs", NULL)) {
            arv_device_get_feature_value(arv_camera_get_device(camera), "ResultingFrameRateAbs", &v, &error);
        } else {
            qDebug() << "Resulting framerate values are not available.";
            qDebug() << "Could not obtain resulting framerate value. This camera might not support it.";
            qDebug() << "Falling back to acquisition framerate value.";

            return getAcquisitionFPSValue();
        }

        if(error) {
            qWarning() << "Could not obtain resulting framerate value. This camera might not support it.";
            qWarning() << "Error during aravis API call. Message: " << error->message;

            // TODO
        }
        auto temp = g_value_get_double(&v);
        // additional checks could come here
        if(error) {
            qWarning() << "Could neither get resulting framerate, nor acquisition framerate.";
            qWarning() << "Error during aravis API call. Message: " << error->message;
        } else {
            val = (int)temp;
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
            qWarning() << "Could not determine whether hardware triggering is enabled.";
            qWarning() << "Error during aravis API call. Message: " << error->message;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    hardwareTriggerEnabled = val; // TODO: get rid of this ?
    return val;
}

void SingleCamera::synchronizeTime() {

    GError *error = nullptr;

    if(arv_device_is_feature_available(arv_camera_get_device(camera), "TimestampLatch", NULL)) {
        qDebug() << "Executing TimestampLatch command.";
        arv_device_execute_command(arv_camera_get_device(camera), "TimestampLatch", &error);
    } else if(arv_device_is_feature_available(arv_camera_get_device(camera), "GevTimestampControlLatch", NULL)) {
        qDebug() << "Falling back to deprecated GevTimestampControlLatch command, if this camera only understands that.";
        arv_device_execute_command(arv_camera_get_device(camera), "GevTimestampControlLatch", &error);
    } else {
        qDebug() << "Timestamp Latch command not available.";
    }
    //arv_device_execute_command(arv_camera_get_device(camera), "TimestampLatch", &error);
    //if(error) {
    //    qDebug() << "Could not execute TimestampLatch command.";
    //    qDebug() << "Error during aravis API call. Message: " << error->message;
    //    qDebug() << "Falling back to deprecated GevTimestampControlLatch command, if this camera only understands that.";
    //    error = nullptr;
    //    arv_device_execute_command(arv_camera_get_device(camera), "GevTimestampControlLatch", &error);
    //}
    if(error) {
        qDebug() << "Could not execute timestamp latch command.";

        wrappedErrorOccured(error);
        // TODO

        return;
    }
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> epoche = std::chrono::time_point<std::chrono::system_clock>{};

    // Get the timestamp value
    error = nullptr;
    if(arv_device_is_feature_available(arv_camera_get_device(camera), "TimestampLatchValue", NULL)) {
        cameraTime = arv_camera_get_integer(camera, "TimestampLatchValue", &error);
    } else if(arv_device_is_feature_available(arv_camera_get_device(camera), "GevTimestampValue", NULL)) {
        cameraTime = arv_camera_get_integer(camera, "GevTimestampValue", &error);
    } else {
        qDebug() << "TimestampLatchValue not available.";
    }
    // TODO
    if(error) {
        qDebug() << "Could not get camera timestamp latch value.";
        wrappedErrorOccured(error);
        // TODO set default values for camera and system time?
        return;
    }
    systemTime  = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    std::time_t startTime = std::chrono::system_clock::to_time_t(start);
    std::time_t epochTime = std::chrono::system_clock::to_time_t(epoche);

    qInfo() << "Camera Synchronize Time";
    qInfo() << "=========================";
    qInfo() << "Timestamp Camera: " << cameraTime;
    qInfo() << "Timestamp System: " << systemTime;
    qInfo() << "System Epoch: " << std::ctime(&epochTime);
    qInfo() << "System Time: " << std::ctime(&startTime);
    qInfo() << "Time from Epoch (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count();
    qInfo() << "Time from Epoch (us): " << std::chrono::duration_cast<std::chrono::microseconds>(start.time_since_epoch()).count();
    qInfo() << "=========================";

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
            wrappedErrorOccured(error);
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
////    arv_stream_start_thread(callbackData.stream);
//    arv_camera_start_acquisition(camera, &error);

    arv_camera_start_acquisition(camera, &error);
    //arv_stream_try_pop_buffer(callbackData.stream);
    if(error) {
        qDebug() << "Could not start grabbing.";
        wrappedErrorOccured(error);
    } else {
        isGrabbingV = true;
        qDebug() << "Started grabbing!";
    }
    // TODO: here something is wrong in case of gv
    //  arv_gv_stream_start_thread: assertion 'priv->thread == NULL' failed
    arv_stream_start_thread(callbackData.stream);
}

void SingleCamera::stopGrabbing() {
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

    arv_camera_stop_acquisition(camera, &error);
    if(error) {
        qDebug() << "Could not gracefully stop grabbing.";
        qDebug() << "Error during aravis API call. Message: " << error->message;
        qDebug() << "Falling back to abort call.";
        arv_camera_abort_acquisition(camera, NULL);
    }
    gboolean delete_buffers = true;
    arv_stream_stop_thread(callbackData.stream, delete_buffers);
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
        qDebug() << "Found calibration file in settings directory. Loading: " << configFile.toStdString();
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
            wrappedErrorOccured(error);
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
            wrappedErrorOccured(error);
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
            wrappedErrorOccured(error);
        } else {
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int SingleCamera::getImageROIoffsetXInc() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        auto temp = arv_camera_get_x_offset_increment(camera, &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI OffsetX increment.";
            wrappedErrorOccured(error);
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
            wrappedErrorOccured(error);
        } else {
            val = temp;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int SingleCamera::getImageROIoffsetYInc() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        auto temp = arv_camera_get_y_offset_increment(camera, &error);
        // additional checks could come here
        if(error) {
            qDebug() << "Could not get image acquisition ROI OffsetY increment.";
            wrappedErrorOccured(error);
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
        gint valMin = 0;
        gint valMax = 0;
        arv_camera_get_width_bounds(camera, &valMin, &valMax, &error);
        // additional checks could come here

        // NOTE: the Aravis library provides the maximum with the offset already subtracted.
        //  But in camera settings GUI, etc we want to know the max possible value, and the
        //  offset is already taken care of separately. So just add that.
        val = valMax + getImageROIoffsetX();

        if(error) {
            qDebug() << "Could not get image acquisition ROI Width maximum.";
            wrappedErrorOccured(error);
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int SingleCamera::getImageROIwidthInc() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        val = arv_camera_get_width_increment(camera, &error);
        if(error) {
            qDebug() << "Could not get image acquisition ROI Width increment.";
            wrappedErrorOccured(error);
        }
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

        // NOTE: the Aravis library provides the maximum with the offset already subtracted.
        //  But in camera settings GUI, etc we want to know the max possible value, and the
        //  offset is already taken care of separately. So just add that.
        val = valMax + getImageROIoffsetY();

        if(error) {
            qDebug() << "Could not get image acquisition ROI Height maximum.";
            wrappedErrorOccured(error);
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int SingleCamera::getImageROIheightInc() {
    GError *error = nullptr;
    int val = 0;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        val = arv_camera_get_height_increment(camera, &error);
        if(error) {
            qDebug() << "Could not get image acquisition ROI Height increment.";
            wrappedErrorOccured(error);
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
            wrappedErrorOccured(error);
        } else {
            val = QRectF(valXoffset, valYoffset, valWidth, valHeight);
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

bool SingleCamera::isBinningAvailable() {
    GError *error = nullptr;
    bool val = false;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool tval = arv_camera_is_binning_available(camera, &error);
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
        arv_camera_get_x_binning_bounds(camera, &bxmin, &bxmax, &error);

        if(error) {
            qDebug() << "Could not get whether binning setting is available, assuming not.";
            wrappedErrorOccured(error);
        } else if(bxmin != bxmax) {
            val = tval;
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
            wrappedErrorOccured(error);
        } else if(canGet) {
            gint valX = 1;
            gint valY = 1;
            error = nullptr;
            arv_camera_get_binning(camera, &valX, &valY, &error);
            // additional checks could come here
            if(error) {
                wrappedErrorOccured(error);
            } else {
                // TODO reset to larget/smaller (?) value if not equal
                val = valX;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

int SingleCamera::getBinningMax() {
    GError *error = nullptr;
    int val = 1;

    if(!ARV_IS_CAMERA(camera))
        return val;

    try {
        bool canGet = arv_camera_is_binning_available(camera, &error);
        if(error) {
            qDebug() << "Could not get binning value.";
            wrappedErrorOccured(error);
        } else if(canGet) {
            gint minX = 1;
            gint maxX = 1;
            error = nullptr;
            arv_camera_get_x_binning_bounds(camera, &minX, &maxX, &error);
            // additional checks could come here
            if(error) {
                wrappedErrorOccured(error);
            } else {
                val = maxX;
            }
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return val;
}

bool SingleCamera::isTemperatureReadingSupported() {

    GError *error = nullptr;
    bool isit = false;

    if(!ARV_IS_CAMERA(camera))
        return isit;

    try {
        //auto temp = arv_camera_get_float(camera, "DeviceTemperature", &error);
        //// fallbacks: set "DeviceTemperatureSelector" value to "Sensor" or "Mainboard"

        if(arv_device_is_feature_available(arv_camera_get_device(camera), "DeviceTemperature", NULL)) {
            isit = true;
        }
    } catch (const std::exception &e) {
        genericExceptionOccured(e);
    }
    return isit;
}

double SingleCamera::getTemperature() {

    GError *error = nullptr;
    double val = CamTempMonitor::MINIMUM_DEVICE_TEMPERATURE;

    if(!ARV_IS_CAMERA(camera) || !isTemperatureReadingSupported())
        return val;

    try {
        auto temp = arv_camera_get_float(camera, "DeviceTemperature", &error);
        // fallbacks: set "DeviceTemperatureSelector" value to "Sensor" or "Mainboard"

        // additional checks could come here
        if(error) {
            qDebug() << "Could not get camera temperature reading.";
            wrappedErrorOccured(error);
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

void SingleCamera::enableSensorLevelBinningIfPossible() {

    // Average BinningMode setting is only possible if the BinningSelector setting is not in Sensor mode
    //  and Sum BinningMode only really has a meaning if Sensor BinningSelector is the case, to compensate
    //  image brightness lost due to smaller area per "pixel", i.e. to retain same brightness even if switched
    //  to binning level 2 or 4 later.
    //  So we set Sum BinningMode only if Sensor BinningSelector is the case, and to
    //  Average BinningMode if BinningSelector is set otherwise.

    GError *error = nullptr;
    try {

        if(arv_device_is_feature_available(arv_camera_get_device(camera), "BinningSelector", NULL)) {

            if(arv_device_is_feature_available(arv_camera_get_device(camera), "BinningHorizontalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(camera), "BinningHorizontalMode", "Sum", &error);
            }
            if(!error && arv_device_is_feature_available(arv_camera_get_device(camera), "BinningVerticalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(camera), "BinningVerticalMode", "Sum", &error);
            }

            if(!error) arv_device_set_string_feature_value(arv_camera_get_device(camera), "BinningSelector", "Sensor", &error);

            if(error) {
                qDebug() << "Could not set sensor level binning.";
                wrappedErrorOccured(error);
            }
        } else {

            if(arv_device_is_feature_available(arv_camera_get_device(camera), "BinningHorizontalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(camera), "BinningHorizontalMode", "Average", &error);
            }
            if(!error && arv_device_is_feature_available(arv_camera_get_device(camera), "BinningVerticalMode", NULL)) {
                arv_device_set_string_feature_value(arv_camera_get_device(camera), "BinningVerticalMode", "Average", &error);
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
bool SingleCamera::setBinningVal(int value) {

    bool success = false;

    stopGrabbing();

    GError *error = nullptr;
    try {

        // TODO only do this when opening camera
        enableSensorLevelBinningIfPossible();

        error = nullptr;
        bool canGet = arv_camera_is_binning_available(camera, &error);
        gint valXMin = 1;
        gint valXMax = 1;
        gint valYMin = 1;
        gint valYMax = 1;
        if(canGet) {
            error = nullptr;
            arv_camera_get_x_binning_bounds(camera, &valXMin, &valXMax, &error);
            if(!error) arv_camera_get_y_binning_bounds(camera, &valYMin, &valYMax, &error);
        } else {
            qDebug() << "Binning value is not avaliable.";
        }
        if(error) {
            qDebug() << "Could not get binning value.";
            wrappedErrorOccured(error);
        } else if( (value <= valXMax && value >= valXMin) && (value <= valYMax && value >= valYMin) ) {

            // TODO: better, find common number of available X and Y binning values (if they might differ)
            arv_camera_set_binning(camera, value, value, &error);

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
bool SingleCamera::setImageROIwidth(int width) {
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
        arv_camera_set_region(camera, currentROI.x(), currentROI.y(), bestWidth, currentROI.height(), &error);

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
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool SingleCamera::setImageROIheight(int height) {
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
        arv_camera_set_region(camera, currentROI.x(), currentROI.y(), currentROI.width(), bestHeight, &error);

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
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool SingleCamera::setImageROIoffsetX(int offsetX) {
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
        arv_camera_set_region(camera, offsetX, currentROI.y(), currentROI.width(), currentROI.height(), &error);

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
    return success;
}

// NOTE: grabbing "pause" is necessary for setting image ROI
bool SingleCamera::setImageROIoffsetY(int offsetY) {
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
        arv_camera_set_region(camera, currentROI.x(), offsetY, currentROI.width(), currentROI.height(), &error);

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
    return success;
}

void SingleCamera::wrappedErrorOccured(GError *error) {

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
        bool isit = arv_device_is_feature_available(arv_camera_get_device(camera), "DeviceReset", &error2);
        if(isit) {
            arv_device_execute_command(arv_camera_get_device(camera), "DeviceReset", &error2);

            if(error2) {
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
