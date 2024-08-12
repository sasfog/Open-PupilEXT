% This is just a test script

clear
clc


pupilEXT = PupilEXT();
pupilEXT.Enabled = true;

pupilEXT.Method = 0;
pupilEXT.UDP_IP = '127.0.0.1';
pupilEXT.UDP_Port = 6900;

% pupilEXT.Method = 1;
% pupilEXT.COM_Port = 'COM6';
% pupilEXT.COM_BaudRate = 115200;

pupilEXT.RecordingsPath = 'C:/PupilEXT_Recordings';
pupilEXT.ParticipantName = '1234';
pupilEXT.DataRecordingDelimiter = ';';
pupilEXT.ImageRecordingFormat = 'tiff';


pupilEXT = pupilEXT.setupHostConnection();

pupilEXT.incrementTrial();
pupilEXT.sendMessage(['TRIAL ' num2str(1)]);

pupilEXT.openSingleCamera('Basler camera name')
pupilEXT.openStereoCamera('Basler camera name 1', 'Basler camera name 2')
pupilEXT.openUVCCamera(0);
pupilEXT.startTracking();
pupilEXT.stopTracking();
pupilEXT.startDataRecording();
pupilEXT.stopDataRecording();
pupilEXT.startDataStreaming();
pupilEXT.stopDataStreaming();
pupilEXT.startImageRecording();
pupilEXT.stopImageRecording();
pupilEXT.disconnectCamera();
pupilEXT.forceResetTrialCounter();
pupilEXT.setPupilDetectionAlgorithm('ElSe');
pupilEXT.setUsingROIAreaSelection(true);
pupilEXT.setComputeOutlineConfidence(true);

pupilEXT.connectRemoteControlUDP('192.168.40.3', 6900);
pupilEXT.connectRemoteControlCOM('COM1', 115200);
pupilEXT.disconnectRemoteControlUDP();
pupilEXT.disconnectRemoteControlCOM();
pupilEXT.connectStreamingUDP('192.168.40.3', 6900);
pupilEXT.connectStreamingCOM('COM1', 115200);
pupilEXT.disconnectStreamingUDP();
pupilEXT.disconnectStreamingCOM();
pupilEXT.connectMicrocontrollerUDP('192.168.40.200', 7000);
pupilEXT.connectMicrocontrollerCOM('COM1', 115200);
pupilEXT.disconnectMicrocontroller();
pupilEXT.switchToHardwareTriggeringMode();
pupilEXT.switchToSoftwareTriggeringMode();
pupilEXT.startHardwareTriggering();
pupilEXT.stopHardwareTriggering();
pupilEXT.setHardwareTriggeringLineSource(1);
pupilEXT.setHardwareTriggeringRuntimeLength(0);
pupilEXT.setHardwareTriggeringFramerate(50);
pupilEXT.setSoftwareTriggeringFramerateLimitingEnabled(true);
pupilEXT.setSoftwareTriggeringFramerateLimit(50);
pupilEXT.setExposureTimeMicrosec(4000);
pupilEXT.setGain(1.2);

pupilEXT = pupilEXT.closeHostConnection();
