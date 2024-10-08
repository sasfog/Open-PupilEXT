
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/qformlayout.h>
#include <QtWidgets/QLabel>
#include <QtWidgets/QtWidgets>

#include "streamingSettingsDialog.h"
#include "../SVGIconColorAdjuster.h"


StreamingSettingsDialog::StreamingSettingsDialog(
    ConnPoolCOM *connPoolCOM,
    ConnPoolUDP *connPoolUDP,
    PupilDetection *pupilDetection,
    DataStreamer *dataStreamer,
    QWidget *parent) :
    QDialog(parent),
    connPoolCOM(connPoolCOM),
    connPoolUDP(connPoolUDP),
    pupilDetection(pupilDetection),
    dataStreamer(dataStreamer),
    applicationSettings(new QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName(), parent)) {

    this->setMinimumSize(310, 450);
    this->setWindowTitle("Streaming settings");

    createForm();

    updateCOMDevices();
    fillCOMParameters();

    loadSettings();

}

StreamingSettingsDialog::~StreamingSettingsDialog() {
    
}

void StreamingSettingsDialog::createForm() {

    QVBoxLayout *mainLayout = new QVBoxLayout(this);




    udpGroup = new QGroupBox("UDP");
    QFormLayout *udpLayout = new QFormLayout;

    QString udpIp = "127.0.0.1";
    int udpPort = 6900;

    udpIpLabel = new QLabel(tr("IP address:"));
    udpIpBox = new IPCtrl();
    udpIpBox->setValue(udpIp);

    udpLayout->addRow(udpIpLabel, udpIpBox);

    udpPortLabel = new QLabel(tr("Port:"));
    udpPortBox = new QSpinBox();
    udpPortBox->setMinimum(1);
    udpPortBox->setMaximum(9999);
    udpPortBox->setSingleStep(1);
    udpPortBox->setValue(udpPort);
    udpPortBox->setFixedWidth(70);

    udpLayout->addRow(udpPortLabel, udpPortBox);

    //comLayout->addRow(flowControlLabel, flowControlBox);
    // TODO ADD SEPARATOR LINE
    QFrame* line1 = new QFrame;
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    udpLayout->addWidget(line1);

    dataContainerUDPLabel = new QLabel(tr("Data container:"));
    dataContainerUDPBox = new QComboBox();
    dataContainerUDPBox->setFixedWidth(90);
    dataContainerUDPBox->addItem(tr("CSV row"), DataStreamer::DataContainer::CSV);
    dataContainerUDPBox->addItem(tr("JSON"), DataStreamer::DataContainer::JSON);
    dataContainerUDPBox->addItem(tr("XML"), DataStreamer::DataContainer::XML);
    dataContainerUDPBox->addItem(tr("YAML"), DataStreamer::DataContainer::YAML);
    dataContainerUDPBox->setCurrentIndex(0);
    udpLayout->addRow(dataContainerUDPLabel, dataContainerUDPBox);


    QWidget *widgetsRow1 = new QWidget();
    QHBoxLayout *UDPbuttonsLayout = new QHBoxLayout();
    connectUDPButton = new QPushButton("Connect");
    disconnectUDPButton = new QPushButton("Disconnect");
    UDPbuttonsLayout->addWidget(connectUDPButton);
    UDPbuttonsLayout->addWidget(disconnectUDPButton);
    widgetsRow1->setLayout(UDPbuttonsLayout);
    udpLayout->addWidget(widgetsRow1);
    disconnectUDPButton->setEnabled(false);

    udpGroup->setLayout(udpLayout);
    mainLayout->addWidget(udpGroup);

    //////

    comGroup = new QGroupBox("COM");
    QFormLayout *comLayout = new QFormLayout;



    comPortLabel = new QLabel(tr("Serial port:"));

    serialPortInfoListBox = new QComboBox();
    //comPortLayout->addWidget(serialPortInfoListBox);

    const QIcon refreshIcon = SVGIconColorAdjuster::loadAndAdjustColors(QString(":/icons/Breeze/actions/22/view-refresh.svg"), applicationSettings);
    refreshButton = new QPushButton(""); // view-refresh.svg
    refreshButton->setIcon(refreshIcon);
    refreshButton->setFixedWidth(42);


    QHBoxLayout *comRow1 = new QHBoxLayout;
    comRow1->addWidget(serialPortInfoListBox);
    QSpacerItem *sp1 = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum); 
    comRow1->addSpacerItem(sp1);
    comRow1->addWidget(refreshButton);
    //hwTriggerGroupLayout->addRow(comPortLabel, comRow1);

    comLayout->addRow(comPortLabel, comRow1);
    //comPortLayout->addWidget(refreshButton);




    baudRateLabel = new QLabel(tr("Baudrate"));
    dataBitsLabel = new QLabel(tr("Data bits"));
    parityLabel = new QLabel(tr("Parity"));
    stopBitsLabel = new QLabel(tr("Stop bits"));
    flowControlLabel = new QLabel(tr("Flow control"));

    baudRateBox = new QComboBox();
    dataBitsBox = new QComboBox();
    flowControlBox = new QComboBox();
    parityBox = new QComboBox();
    stopBitsBox = new QComboBox();

    serialPortInfoListBox->setMinimumWidth(150);
    baudRateBox->setMinimumWidth(100);
    dataBitsBox->setMinimumWidth(100);
    flowControlBox->setMinimumWidth(100);
    parityBox->setMinimumWidth(100);
    stopBitsBox->setMinimumWidth(100);

    comLayout->addRow(baudRateLabel, baudRateBox);
    comLayout->addRow(dataBitsLabel, dataBitsBox);
    comLayout->addRow(parityLabel, parityBox);
    comLayout->addRow(stopBitsLabel, stopBitsBox);
    comLayout->addRow(flowControlLabel, flowControlBox);

    //comLayout->addRow(flowControlLabel, flowControlBox);
    // TODO ADD SEPARATOR LINE
    QFrame* line2 = new QFrame;
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    comLayout->addWidget(line2);

    dataContainerCOMLabel = new QLabel(tr("Data container:"));
    dataContainerCOMBox = new QComboBox();
    dataContainerCOMBox->setFixedWidth(90);
    dataContainerCOMBox->addItem(tr("CSV row"), DataStreamer::DataContainer::CSV);
    dataContainerCOMBox->addItem(tr("JSON"), DataStreamer::DataContainer::JSON);
    dataContainerCOMBox->addItem(tr("XML"), DataStreamer::DataContainer::XML);
    dataContainerCOMBox->addItem(tr("YAML"), DataStreamer::DataContainer::YAML);
    dataContainerCOMBox->setCurrentIndex(0);
    comLayout->addRow(dataContainerCOMLabel, dataContainerCOMBox);


    QWidget *widgetsRow2 = new QWidget();
    QHBoxLayout *COMbuttonsLayout = new QHBoxLayout();
    connectCOMButton = new QPushButton("Connect");
    disconnectCOMButton = new QPushButton("Disconnect");
    COMbuttonsLayout->addWidget(connectCOMButton);
    COMbuttonsLayout->addWidget(disconnectCOMButton);
    widgetsRow2->setLayout(COMbuttonsLayout);
    comLayout->addWidget(widgetsRow2);
    disconnectCOMButton->setEnabled(false);

    comGroup->setLayout(comLayout);
    mainLayout->addWidget(comGroup);

    connect(connectUDPButton, SIGNAL(clicked()), this, SLOT(onConnectUDPClick()));
    connect(disconnectUDPButton, SIGNAL(clicked()), this, SLOT(disconnectUDP()));
    connect(connectCOMButton, SIGNAL(clicked()), this, SLOT(onConnectCOMClick()));
    connect(disconnectCOMButton, SIGNAL(clicked()), this, SLOT(disconnectCOM()));
    connect(refreshButton, SIGNAL(clicked()), this, SLOT(updateCOMDevices()));

    setLayout(mainLayout);
}

void StreamingSettingsDialog::connectUDP(const ConnPoolUDPInstanceSettings &p) {
    int index = connPoolUDP->setupAndOpenConnection(p, ConnPoolPurposeFlag::STREAMING);
    if(index >= 0) {
        setLimitationsWhileConnectedUDP(true);

        connPoolUDPIndex = index;
        //connPoolUDP->subscribeListener(index, this, SLOT(readData(QString)));

        // TODO: nicer way for this?
        //  this is necessary because sometimes PRGmainwindow calls this method
        //  Other options include a loadSettings() call in the beginning, or making a new method for these
        udpIpBox->blockSignals(true);
        udpPortBox->blockSignals(true);
        dataContainerUDPBox->blockSignals(true);
        udpIpBox->setValue(p.ipAddress.toString());
        udpPortBox->setValue(p.portNumber);
        dataContainerUDPBox->setCurrentText(applicationSettings->value("StreamingSettings.UDP.dataContainer", "CSV").toString());
        udpIpBox->blockSignals(false);
        udpPortBox->blockSignals(false);
        dataContainerUDPBox->blockSignals(false);

        emit onUDPConnect();
        //emit onConnStateChanged();
    } else {
        if(connPoolUDP->getInstance(connPoolUDPIndex) != nullptr) {
            QString errMsg = connPoolUDP->getInstance(connPoolUDPIndex)->errorString();
            QMessageBox::critical(this, tr("Error"), connPoolUDP->getInstance(connPoolUDPIndex)->errorString());
        }
        qDebug() << "Error while connecting to the specified UDP port. It is possibly already in use by another application.";
    }
}

void StreamingSettingsDialog::onConnectUDPClick() {
    updateSettings();
    connectUDP(m_currentSettingsUDP);
}

void StreamingSettingsDialog::disconnectUDP() {
    if(connPoolUDPIndex < 0) {
        qDebug() << "StreamingSettingsDialog::disconnectUDP(): connPoolUDPIndex value is invalid";
        return;
    }

    if(connPoolUDP->getInstance(connPoolUDPIndex)->isOpen()) {
        //connPoolUDP->unsubscribeListener(connPoolUDPIndex, this, SLOT(readData(QString)));
        connPoolUDP->closeConnection(connPoolUDPIndex, ConnPoolPurposeFlag::STREAMING);
    }
    connPoolUDPIndex = -1;

    setLimitationsWhileConnectedUDP(false);

    emit onUDPDisconnect();
    //emit onConnStateChanged();
}

void StreamingSettingsDialog::connectCOM(const ConnPoolCOMInstanceSettings &p) {
    int index = connPoolCOM->setupAndOpenConnection(p, ConnPoolPurposeFlag::STREAMING);
    if(index >= 0) {
        setLimitationsWhileConnectedCOM(true);

        connPoolCOMIndex = index;
        //connPoolCOM->subscribeListener(index, this, SLOT(readData(QString)));

        // TODO: nicer way for this?
        //  this is necessary because sometimes PRGmainwindow calls this method
        //  Other options include a loadSettings() call in the beginning, or making a new method for these
        baudRateBox->blockSignals(true);
        dataBitsBox->blockSignals(true);
        flowControlBox->blockSignals(true);
        parityBox->blockSignals(true);
        stopBitsBox->blockSignals(true);
        dataContainerCOMBox->blockSignals(true);
        baudRateBox->setCurrentIndex(baudRateBox->findData(p.baudRate));
        dataBitsBox->setCurrentIndex(dataBitsBox->findData(p.dataBits));
        flowControlBox->setCurrentIndex(flowControlBox->findData(p.flowControl));
        parityBox->setCurrentIndex(parityBox->findData(p.parity));
        stopBitsBox->setCurrentIndex(stopBitsBox->findData(p.stopBits));
        dataContainerCOMBox->setCurrentText(applicationSettings->value("StreamingSettings.COM.dataContainer", "CSV").toString());
        baudRateBox->blockSignals(false);
        dataBitsBox->blockSignals(false);
        flowControlBox->blockSignals(false);
        parityBox->blockSignals(false);
        stopBitsBox->blockSignals(false);
        dataContainerCOMBox->blockSignals(false);

        emit onCOMConnect();
        //emit onConnStateChanged();
    } else {
        if(connPoolCOM->getInstance(connPoolCOMIndex) != nullptr) {
            QString errMsg = connPoolCOM->getInstance(connPoolCOMIndex)->errorString();
            QMessageBox::critical(this, tr("Error"), connPoolCOM->getInstance(connPoolCOMIndex)->errorString());
        }
        qDebug() << "Error while connecting to the specified COM port. It is possibly already in use by another application.";
    }
}

void StreamingSettingsDialog::onConnectCOMClick() {
    updateSettings();
    connectCOM(m_currentSettingsCOM);
}

void StreamingSettingsDialog::disconnectCOM() {
    if(connPoolCOMIndex < 0) {
        qDebug() << "StreamingSettingsDialog::disconnectCOM(): connPoolCOMIndex value is invalid";
        return;
    }

    if(connPoolCOM->getInstance(connPoolCOMIndex)->isOpen()) {
        //connPoolCOM->unsubscribeListener(connPoolCOMIndex, this, SLOT(readData(QString)));
        connPoolCOM->closeConnection(connPoolCOMIndex, ConnPoolPurposeFlag::STREAMING);
    }
    connPoolCOMIndex = -1;

    setLimitationsWhileConnectedCOM(false);

    emit onCOMDisconnect();
    //emit onConnStateChanged();
}

void StreamingSettingsDialog::fillCOMParameters() {
    baudRateBox->addItem(QStringLiteral("9600"), QSerialPort::Baud9600);
    baudRateBox->addItem(QStringLiteral("19200"), QSerialPort::Baud19200);
    baudRateBox->addItem(QStringLiteral("38400"), QSerialPort::Baud38400);
    baudRateBox->addItem(QStringLiteral("115200"), QSerialPort::Baud115200);
    baudRateBox->setCurrentIndex(3);
    //baudRateBox->addItem(tr("Custom"));

    dataBitsBox->addItem(QStringLiteral("5"), QSerialPort::Data5);
    dataBitsBox->addItem(QStringLiteral("6"), QSerialPort::Data6);
    dataBitsBox->addItem(QStringLiteral("7"), QSerialPort::Data7);
    dataBitsBox->addItem(QStringLiteral("8"), QSerialPort::Data8);
    dataBitsBox->setCurrentIndex(3);

    parityBox->addItem(tr("None"), QSerialPort::NoParity);
    parityBox->addItem(tr("Even"), QSerialPort::EvenParity);
    parityBox->addItem(tr("Odd"), QSerialPort::OddParity);
    parityBox->addItem(tr("Mark"), QSerialPort::MarkParity);
    parityBox->addItem(tr("Space"), QSerialPort::SpaceParity);

    stopBitsBox->addItem(QStringLiteral("1"), QSerialPort::OneStop);
#ifdef Q_OS_WIN
    stopBitsBox->addItem(tr("1.5"), QSerialPort::OneAndHalfStop);
#endif
    stopBitsBox->addItem(QStringLiteral("2"), QSerialPort::TwoStop);

    flowControlBox->addItem(tr("None"), QSerialPort::NoFlowControl);
    flowControlBox->addItem(tr("RTS/CTS"), QSerialPort::HardwareControl);
    flowControlBox->addItem(tr("XON/XOFF"), QSerialPort::SoftwareControl);
}

bool StreamingSettingsDialog::isAnyConnected() {
    return (isUDPConnected() || isCOMConnected());
}

bool StreamingSettingsDialog::isUDPConnected() {
    if(connPoolUDPIndex < 0 || !connPoolUDP->getInstance(connPoolUDPIndex)->isOpen())
    /*->state() == QAbstractSocket::ClosingState || 
        connPoolUDP->getInstance(connPoolUDPIndex)->state() == QAbstractSocket::UnconnectedState */
        return false;
    else
        return true;
}

bool StreamingSettingsDialog::isCOMConnected() {
    if(connPoolCOMIndex < 0 || !connPoolCOM->getInstance(connPoolCOMIndex)->isOpen())
        return false;
    else
        return true;
}

// Update current settings with the configuration from the form
void StreamingSettingsDialog::updateSettings()
{
    m_currentSettingsUDP.ipAddress = QHostAddress(udpIpBox->getValue());

    m_currentSettingsUDP.portNumber = udpPortBox->value();

    //

    m_currentSettingsCOM.name = serialPortInfoListBox->currentText();

    if (baudRateBox->currentIndex() == 4) {
        m_currentSettingsCOM.baudRate = baudRateBox->currentText().toInt();
    } else {
        m_currentSettingsCOM.baudRate = static_cast<QSerialPort::BaudRate>(
                    baudRateBox->itemData(baudRateBox->currentIndex()).toInt());
    }
    m_currentSettingsCOM.stringBaudRate = QString::number(m_currentSettingsCOM.baudRate);

    m_currentSettingsCOM.dataBits = static_cast<QSerialPort::DataBits>(
                dataBitsBox->itemData(dataBitsBox->currentIndex()).toInt());
    m_currentSettingsCOM.stringDataBits = dataBitsBox->currentText();

    m_currentSettingsCOM.parity = static_cast<QSerialPort::Parity>(
                parityBox->itemData(parityBox->currentIndex()).toInt());
    m_currentSettingsCOM.stringParity = parityBox->currentText();

    m_currentSettingsCOM.stopBits = static_cast<QSerialPort::StopBits>(
                stopBitsBox->itemData(stopBitsBox->currentIndex()).toInt());
    m_currentSettingsCOM.stringStopBits = stopBitsBox->currentText();

    m_currentSettingsCOM.flowControl = static_cast<QSerialPort::FlowControl>(
                flowControlBox->itemData(flowControlBox->currentIndex()).toInt());
    m_currentSettingsCOM.stringFlowControl = flowControlBox->currentText();

    //m_currentSettings.localEchoEnabled = localEchoCheckBox->isChecked();

    saveSettings();
}

// Loads the serial port settings from application settings
void StreamingSettingsDialog::loadSettings() {

    dataContainerUDPBox->setCurrentText(applicationSettings->value("StreamingSettings.UDP.dataContainer", dataContainerUDPBox->itemText(0)).toString());
    dataContainerCOMBox->setCurrentText(applicationSettings->value("StreamingSettings.COM.dataContainer", dataContainerCOMBox->itemText(0)).toString());

    udpIpBox->setValue(applicationSettings->value("StreamingSettings.UDP.ipAddress", udpIpBox->getValue()).toString());
    udpPortBox->setValue(applicationSettings->value("StreamingSettings.UDP.portNumber", udpPortBox->value()).toInt());

    serialPortInfoListBox->setCurrentText(applicationSettings->value("StreamingSettings.COM.name", serialPortInfoListBox->itemText(0)).toString());
    baudRateBox->setCurrentText(applicationSettings->value("StreamingSettings.COM.baudRate", baudRateBox->itemText(3)).toString());
    dataBitsBox->setCurrentText(applicationSettings->value("StreamingSettings.COM.dataBits", dataBitsBox->itemText(0)).toString());
    parityBox->setCurrentText(applicationSettings->value("StreamingSettings.COM.parity", parityBox->itemText(0)).toString());
    stopBitsBox->setCurrentText(applicationSettings->value("StreamingSettings.COM.stopBits", stopBitsBox->itemText(0)).toString());
    flowControlBox->setCurrentText(applicationSettings->value("StreamingSettings.COM.flowControl", flowControlBox->itemText(0)).toString());
    // localEchoCheckBox->setChecked(SupportFunctions::readBoolFromQSettings("StreamingSettings.COM.localEchoEnabled", localEchoCheckBox->isChecked(), applicationSettings));

    updateSettings();
}

// Saves serial port settings to application settings
void StreamingSettingsDialog::saveSettings() {

    applicationSettings->setValue("StreamingSettings.UDP.dataContainer", dataContainerUDPBox->currentText());
    applicationSettings->setValue("StreamingSettings.COM.dataContainer", dataContainerCOMBox->currentText());

    applicationSettings->setValue("StreamingSettings.UDP.ipAddress", udpIpBox->getValue());
    applicationSettings->setValue("StreamingSettings.UDP.portNumber", udpPortBox->value());
    
    applicationSettings->setValue("StreamingSettings.COM.name", serialPortInfoListBox->currentText());
    applicationSettings->setValue("StreamingSettings.COM.baudRate", baudRateBox->currentText().toInt());
    applicationSettings->setValue("StreamingSettings.COM.dataBits", dataBitsBox->currentText().toInt());
    applicationSettings->setValue("StreamingSettings.COM.parity", parityBox->currentText());
    applicationSettings->setValue("StreamingSettings.COM.stopBits", stopBitsBox->currentText().toFloat());
    applicationSettings->setValue("StreamingSettings.COM.flowControl", flowControlBox->currentText());
    //applicationSettings->setValue("StreamingSettings.COM.localEchoEnabled", localEchoCheckBox->isChecked());
}

/*
bool StreamingSettingsDialog::isCOMConnected() {
    return serialPort->isOpen();
}
*/

void StreamingSettingsDialog::updateCOMDevices() {
    serialPortInfoListBox->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        serialPortInfoListBox->addItem(info.portName());
    }

    // NOTE: also add item(s) assigned from our internal COM pool (which can be reused)
    // later NOTE: no need for this, because qt also seems to show the ports that PupilEXT has already opened
//    std::vector<QString> poolPortNames = connPoolCOM->getOpenedNames();
//    for(int i=0; i<poolPortNames.size(); i++) {
//        serialPortInfoListBox->addItem(poolPortNames[i]);
//    }
}

int StreamingSettingsDialog::getConnPoolUDPIndex() {
    return connPoolUDPIndex;
}

int StreamingSettingsDialog::getConnPoolCOMIndex() {
    return connPoolCOMIndex;
}

DataStreamer::DataContainer StreamingSettingsDialog::getDataContainerUDP() {
    DataStreamer::DataContainer cn = static_cast<DataStreamer::DataContainer>(
                dataContainerUDPBox->itemData(dataContainerUDPBox->currentIndex()).toInt());
    return cn;
}

DataStreamer::DataContainer StreamingSettingsDialog::getDataContainerCOM() {
    DataStreamer::DataContainer cn = static_cast<DataStreamer::DataContainer>(
                dataContainerCOMBox->itemData(dataContainerCOMBox->currentIndex()).toInt());
    return cn;
}

void StreamingSettingsDialog::setLimitationsWhileConnectedUDP(bool state) {  

    udpIpLabel->setDisabled(state);
    udpPortLabel->setDisabled(state);

    udpIpBox->setDisabled(state);
    udpPortBox->setDisabled(state);

    connectUDPButton->setDisabled(state);
    disconnectUDPButton->setDisabled(!state);
}

void StreamingSettingsDialog::setLimitationsWhileStreamingUDP(bool state) {  
    
    dataContainerUDPBox->setDisabled(state);
    dataContainerUDPLabel->setDisabled(state);
}

void StreamingSettingsDialog::setLimitationsWhileConnectedCOM(bool state) {  
    
    serialPortInfoListBox->setDisabled(state);
    refreshButton->setDisabled(state);

    comPortLabel->setDisabled(state);
    baudRateLabel->setDisabled(state);
    dataBitsLabel->setDisabled(state);
    parityLabel->setDisabled(state);
    stopBitsLabel->setDisabled(state);
    flowControlLabel->setDisabled(state);

    baudRateBox->setDisabled(state);
    dataBitsBox->setDisabled(state);
    flowControlBox->setDisabled(state);
    parityBox->setDisabled(state);
    stopBitsBox->setDisabled(state);

    connectCOMButton->setDisabled(state);
    disconnectCOMButton->setDisabled(!state);
}

void StreamingSettingsDialog::setLimitationsWhileStreamingCOM(bool state) {  
    
    dataContainerCOMBox->setDisabled(state);
    dataContainerCOMLabel->setDisabled(state);
}

/*
void StreamingSettingsDialog::setLimitationsWhileStreaming(bool state) {  
    
    if(isUDPConnected()) {
        dataContainerUDPBox->setDisabled(state);
        dataContainerUDPLabel->setDisabled(state);
    } else {
        // group of the streaming method that is not used is disabled while streaming is on from another streaming method
        udpGroup->setDisabled(state);
    }

    if(isCOMConnected()) {
        dataContainerCOMBox->setDisabled(state);
        dataContainerCOMLabel->setDisabled(state);
    } else {
        // group of the streaming method that is not used is disabled while streaming is on from another streaming method
        comGroup->setDisabled(state);
    }
}
*/

