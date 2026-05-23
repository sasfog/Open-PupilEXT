#pragma once

/**
    @author Gabor Benyei
*/

#include <QtCore/QObject>
#include <QByteArray>
#include <QTimer>
#include <QSettings>
#include <QCoreApplication>
#include <QThread>

// TODO: NOT HERE, but might not even be necessary
//Q_DECLARE_METATYPE(std::vector<double>)

/**
    An instance of this class is supposed to be running all the time, looking for an MCU
    and managing it if connected.
*/

class MCUManager : public QObject {

Q_OBJECT
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)

    // same interval used for:
    // - checking if there is available MCU, and try connecting to it
    // - checking if the MCU is still connected
    // - checking illuminator temperature
    const int checkIntervalSec = 5;

    const int illumWarmupStableDeltaTime = 60;
    const double illumWarmupStableDeltaTemp = 0.5;

    const int heartbeatTimeoutSerialMs = 100;
    const int heartbeatTimeoutUDPMs = 100;

    bool m_running;

    std::vector<std::vector<double>> illumTempChecks;

    // TODO: ? make a separate EyeTracker device (physical setup?) management class:
    // - can connect to MCU automatically, via either serial or UDP
    // - can read higher level descriptors from MCU
    // - can connect to camera automatically
    // - can oversee heartbeats from MCU and camera as well
    // - can oversee camera temp checks from MCU and camera as well
    //      below it is:
    //      - the MCU management class (which has a "GUI" in the MCU settings window)
    //      - the camera class..?

    const std::vector<QString> autoConnectAvoidanceList = {
            "emul", // or emu ?
            "amp",
            "Debug",
            "JTAG"
            "OpenBCI",
            "Ganglion",
            "Cyton",
            "RFduino",
            "Brain",
//            "Brain Products",
//            "BrainAmp",
//            "actiCHamp",
//            "LiveAmp",
//            "V-Amp",
//            "BrainVision",
//            "BV Recorder",
            "Recorder",
            "g.tec",
//            "gUSBamp",
//            "gHIamp",
            "g.Nautilus",
            "gtec",
//            "USBamp",
            "BioSemi",
            "ActiveTwo",
            "Emotiv",
            "EPOC",
            "Insight",
            "MindRove",
            "Neurosity",
            "Notion",
            "Neurosity Crown",
            "Muse",
            "InteraXon",
            "NeuroSky",
            "MindWave",
            "ThinkGear",
            "TGAM",
            "BITalino",
            "bitalino",
            "PLUX",
            "biosignalsplux",
            "OpenBAN",
            "MuscleBAN",
            "National Instruments",
            "NI-DAQ",
            "USB-6",
            "USB-621",
            "DAQmx",
            "Blackrock",
            "Cerebus",
            "NeuroPort",
            "NSP",
            "Ripple Neuro",
            "Nomad",
            "Grapevine",
            "Tucker-Davis Technologies",
            "TDT",
            "RZ2",
            "Synapse",
            "Natus",
            "Nicolet",
            "Quantum",
            "Neuroworks",
            "Compumedics",
            "Neuroscan",
//            "SynAmps",
            "Curry",
            "Electrical Geodesics",
            "EGI",
//            "Net Amps",
            "Geodesic",
            "Cognionics",
            "Quick-20",
            "HD-72",
            "Ephys",
            "Rhythm",
            "Intan",
            "RHD2000",
            "RHS2000",
            "USB Interface Board",
            "Polar",
            "Shimmer",
            "Delsys",
            "Trigno",
            "Acquisition",
            "EEG"
    };

    // TODO: also tolower both
    const std::vector<QString> autoConnectPreferenceList = {
            "Nucleo",
            "Arduino",
            "Nano",
            "wch.cn",
            "Atmel",
            "QinHeng",
            "Future Technology",
            "Silicon Labs",
            "Prolific",
            "SparkFun",
            "Adafruit",
            "Seeed",
            "Espressif",
            "Teensy",
//            "CH340",
//            "CH341",
//            "CH343",
            "CH34",
            "HL-340",
            "CH9102",
            "STMicroelectronics",
            "STM32",
            "STLink",
            "ST-LINK",
            "USB Serial",
            "USB2.0-Serial",
            "USB-Enhanced-SERIAL",
            "USB-SERIAL",
            "USB-to-Serial",
            "USB Serial Device",
            "Communications Device Class",
            "USB Serial Gadget",
            "g_serial",
            "Linux USB Serial Gadget",
            "CDC Serial",
            "CDC ACM",
            "HalfKay Bootloader",
            "RawHID",
            "PJRC Serial",
            "USB Serial Port",
//            "FT232R USB UART",
//            "FTDI",
//            "FT230",
//            "FT231",
//            "FT232R",
//            "CP2102",
//            "CP210",
//            "SLAB_USBtoUART",
//            "Prolific",
//            "PL2303",
//            "Future Technology",
//            "BeagleBone",
//            "TI USB",
//            "Texas Instruments XDS100",
//            "TI ICDI",
//            "TinyUSB",
//            "Raspberry",
//            "Pico",
//            "RP2040",
            "duino",
            "Composite",
            "Controller",
            "Comm",
            "ComPort",
            "Port",
            "Bridge",
            "Virtual",
            "TTL232R",
            "RS232"
    };

    bool MCUConnected = false;

public:

    // The temperature that we deem as default value when cannot be measured, and the minimum that can be measured
    // Note: not yet used in recEventTracker (problem accessing a const static value in a nonstatic class, perhaps some include problem?)
    static constexpr double MINIMUM_ILLUMINATOR_TEMPERATURE = -1.0;

    explicit MCUManager();
    bool running() const;

signals:
    void finished();
    void runningChanged(bool running);
    void illumTempChecked(std::vector<double> temperatures);
    void illumWarmupHasDeltaTimeData();
    void illumWarmedUp();
    void illumWarmUpReadingsInvalid();

    void MCUUnexpectedlyDisconnected();
    void MCUConnectionUnstable();

public slots:
    void run();
    void setRunning(bool running);

};


