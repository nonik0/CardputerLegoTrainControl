#include "SBrickHub.h"

std::string getHexString(byte *pData, size_t length)
{
    std::string hexString = "0x";
    for (int i = 0; i < length; i++)
    {
        char byteHexChars[3];
        sprintf(byteHexChars, "%02x", pData[i]);
        hexString += std::string(byteHexChars, 2);
    }

    return hexString;
}

class SBrickHubClientCallback : public NimBLEClientCallbacks
{
    SBrickHub *_sbrickHub;

public:
    SBrickHubClientCallback(SBrickHub *sbrickHub) : NimBLEClientCallbacks()
    {
        _sbrickHub = sbrickHub;
    }

    void onConnect(NimBLEClient *bleClient)
    {
    }

    void onDisconnect(NimBLEClient *bleClient)
    {
        _sbrickHub->_isConnecting = false;
        _sbrickHub->_isConnected = false;
        log_d("disconnected client");
    }
};

class SBrickHubAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
    SBrickHub *_sbrickHub;

public:
    SBrickHubAdvertisedDeviceCallbacks(SBrickHub *sbrickHub) : NimBLEAdvertisedDeviceCallbacks()
    {
        _sbrickHub = sbrickHub;
    }

    static void scanEndedCallback(NimBLEScanResults results)
    {
        log_d("Number of devices: %d", results.getCount());
        for (int i = 0; i < results.getCount(); i++)
        {
            log_d("device[%d]: %s", i, results.getDevice(i).toString().c_str());
        }
    }

    void onResult(NimBLEAdvertisedDevice *advertisedDevice)
    {
        // Found a device, check if the service is contained and optional if address fits requested address
        log_d("advertised device: %s", advertisedDevice->toString().c_str());

        // TODO: connect without address
        if (advertisedDevice->getName() == "NickSBrick")
        {
            log_w("found SBrick device!\r\n", advertisedDevice->toString().c_str());
        }

        // SBrick does not advertise the service, so we need to connect directly instead of looking for the service in broadcasted devices
        if (_sbrickHub->_requestedDeviceAddress && advertisedDevice->getAddress().equals(*_sbrickHub->_requestedDeviceAddress))
        {
            advertisedDevice->getScan()->stop();
            _sbrickHub->_pServerAddress = new NimBLEAddress(advertisedDevice->getAddress());
            _sbrickHub->_hubName = advertisedDevice->getName();

            if (advertisedDevice->haveManufacturerData())
            {
                uint8_t *manufacturerData = (uint8_t *)advertisedDevice->getManufacturerData().data();
                uint8_t manufacturerDataLength = advertisedDevice->getManufacturerData().length();
                log_w("manufacturer data: [%s]", getHexString(manufacturerData, manufacturerDataLength).c_str());
            }
            _sbrickHub->_isConnecting = true;
        }
    }
};

SBrickHub::SBrickHub() {}

void SBrickHub::init()
{
    log_d("initializing SBrickHub");

    _isConnected = false;
    _isConnecting = false;
    _bleUuid = NimBLEUUID(SBRICK_SERVICE_UUID);
    _characteristicRemoteControlUuid = NimBLEUUID(SBRICK_CHARACTERISTIC_UUID_REMOTE_CONTROL);
    _characteristicQuickDriveUuid = NimBLEUUID(SBRICK_CHARACTERISTIC_UUID_QUICK_DRIVE);

    NimBLEDevice::init("");
    NimBLEScan *pBLEScan = NimBLEDevice::getScan();

    pBLEScan->setAdvertisedDeviceCallbacks(new SBrickHubAdvertisedDeviceCallbacks(this));

    pBLEScan->setActiveScan(true);
    // start method with callback function to enforce the non blocking scan. If no callback function is used,
    // the scan starts in a blocking manner
    pBLEScan->start(_scanDuration, SBrickHubAdvertisedDeviceCallbacks::scanEndedCallback);
}

void SBrickHub::init(std::string deviceAddress)
{
    _requestedDeviceAddress = new BLEAddress(deviceAddress);
    init();
}

void SBrickHub::init(uint32_t scanDuration)
{
    _scanDuration = scanDuration;
    init();
}

void SBrickHub::init(std::string deviceAddress, uint32_t scanDuration)
{
    _requestedDeviceAddress = new BLEAddress(deviceAddress);
    _scanDuration = scanDuration;
    init();
}

bool SBrickHub::connectHub()
{
    log_d("connecting to sbrick hub");

    NimBLEAddress pAddress = *_pServerAddress;
    _pClient = nullptr;

    log_d("number of ble clients: %d", NimBLEDevice::getClientListSize());

    /** Check if we have a client we should reuse first **/
    if (NimBLEDevice::getClientListSize())
    {
        /** Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        _pClient = NimBLEDevice::getClientByPeerAddress(pAddress);
        if (_pClient)
        {
            if (!_pClient->connect(pAddress, false))
            {
                log_e("reconnect failed");
                return false;
            }
            log_d("reconnect client");
        }
        /** We don't already have a client that knows this device,
         *  we will check for a client that is disconnected that we can use.
         */
        else
        {
            _pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    /** No client to reuse? Create a new one. */
    if (!_pClient)
    {
        if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS)
        {
            log_w("max clients reached - no more connections available: %d", NimBLEDevice::getClientListSize());
            return false;
        }

        _pClient = NimBLEDevice::createClient();
    }

    if (!_pClient->isConnected())
    {
        if (!_pClient->connect(pAddress))
        {
            log_e("failed to connect");
            return false;
        }
    }

    log_d("connected to: %s, RSSI: %d", _pClient->getPeerAddress().toString().c_str(), _pClient->getRssi());
    NimBLERemoteService *pRemoteService = _pClient->getService(_bleUuid);
    if (pRemoteService == nullptr)
    {
        log_e("failed to get ble client");
        return false;
    }

    _pRemoteCharacteristicRemoteControl = pRemoteService->getCharacteristic(_characteristicRemoteControlUuid);
    _pRemoteCharacteristicQuickDrive = pRemoteService->getCharacteristic(_characteristicQuickDriveUuid);
    if (_pRemoteCharacteristicRemoteControl == nullptr || _pRemoteCharacteristicQuickDrive == nullptr)
    {
        log_e("failed to get ble service");
        return false;
    }

    // notifications are on quick drive characteristic
    if (_pRemoteCharacteristicQuickDrive->canNotify())
    {
        _pRemoteCharacteristicQuickDrive->subscribe(true, std::bind(&SBrickHub::notifyCallback, this, _1, _2, _3, _4), true);
    }

    // add callback instance to get notified if a disconnect event appears
    _pClient->setClientCallbacks(new SBrickHubClientCallback(this));

    // Set states
    _isConnected = true;
    _isConnecting = false;
    return true;
}

void SBrickHub::disconnectHub()
{
    log_d("disconnecting client");
    NimBLEDevice::deleteClient(_pClient);
}

bool SBrickHub::isConnecting()
{
    return _isConnecting;
}

bool SBrickHub::isConnected()
{
    return _isConnected;
}

NimBLEAddress SBrickHub::getHubAddress()
{
    NimBLEAddress pAddress = *_pServerAddress;
    return pAddress;
}

std::string SBrickHub::getHubName() { return _hubName; }

void SBrickHub::configureAdcChannels(byte port)
{
    //log_w("configuring sensor on adcChannel %d", adcChannel);

    byte setPeriodicVoltageMeasurementCommand[7] = {(byte)SBrickCommandType::SET_VOLTAGE_MEASURE, 
        (byte)SBrickAdcChannel::C_C1, (byte)SBrickAdcChannel::C_C2,
        (byte)SBrickAdcChannel::D_C1, (byte)SBrickAdcChannel::D_C2,
        (byte)SBrickAdcChannel::Voltage, (byte)SBrickAdcChannel::Temperature};
    WriteValue(setPeriodicVoltageMeasurementCommand, 7);

    byte setUpPeriodicVoltageNotificationCommand[7] = {(byte)SBrickCommandType::SET_VOLTAGE_NOTIF,
        (byte)SBrickAdcChannel::C_C1, (byte)SBrickAdcChannel::C_C2,
        (byte)SBrickAdcChannel::D_C1, (byte)SBrickAdcChannel::D_C2,
        (byte)SBrickAdcChannel::Voltage, (byte)SBrickAdcChannel::Temperature};
    WriteValue(setUpPeriodicVoltageNotificationCommand, 7);

    // byte setPeriodicVoltageMeasurementCommand[2] = {(byte)SBrickCommandType::SET_VOLTAGE_MEASURE, adcChannel};
    // WriteValue(setPeriodicVoltageMeasurementCommand, 2);

    // byte setUpPeriodicVoltageNotificationCommand[2] = {(byte)SBrickCommandType::SET_VOLTAGE_NOTIF, adcChannel};
    // WriteValue(setUpPeriodicVoltageNotificationCommand, 2);
}

float SBrickHub::readAdcChannel(byte adcChannel)
{
    byte queryAdcCommand[2] = {(byte)SBrickCommandType::QUERY_ADC, adcChannel};
    WriteValue(queryAdcCommand, 2);

    uint16_t rawAdcValue = _pRemoteCharacteristicRemoteControl->readValue<uint16_t>();
    log_w("ADC[%d]: %04x", adcChannel, rawAdcValue);
    if (rawAdcValue)
    {   
        int value = rawAdcValue >> 4;
        int channel = rawAdcValue & 0x000F;
        float voltage = (value * 0.83875F) / 127.0;

        return voltage;
    }

    return 0;
}

void SBrickHub::getActiveChannels()
{
    byte command[1] = {(byte)SBrickCommandType::GET_VOLTAGE_MEASURE};
    WriteValue(command, 1);

    NimBLEAttValue value = _pRemoteCharacteristicRemoteControl->readValue();
    log_w("value: %s", getHexString((byte*)value.data(), value.length()).c_str());
}

void SBrickHub::getActiveChannelNotifications()
{
    byte command[1] = {(byte)SBrickCommandType::GET_VOLTAGE_NOTIF};
    WriteValue(command, 1);

    NimBLEAttValue value = _pRemoteCharacteristicRemoteControl->readValue();
    log_w("value: %s", getHexString((byte*)value.data(), value.length()).c_str());
}

void SBrickHub::rebootHub()
{
    byte rebootCommand[1] = {(byte)SBrickCommandType::REBOOT};
    WriteValue(rebootCommand, 1);
}

void SBrickHub::setHubName(char name[])
{
    int nameLength = strlen(name);
    if (nameLength > 13)
    {
        return;
    }
    _hubName = std::string(name, nameLength);

    int arraySize = 1 + nameLength;
    byte setNameCommand[arraySize] = {(byte)SBrickCommandType::SET_DEVICE_NAME};

    memcpy(setNameCommand, name, nameLength + 1);
    WriteValue(setNameCommand, arraySize);
}

int SBrickHub::getRssi()
{
    return _pClient->getRssi();
}

void SBrickHub::setWatchdogTimeout(uint8_t tenthOfSeconds)
{
    byte setWatchdogCommand[2] = {(byte)SBrickCommandType::SET_WATCHDOG_TIMEOUT, tenthOfSeconds};
    WriteValue(setWatchdogCommand, 2);
}

uint8_t SBrickHub::getWatchdogTimeout()
{
    byte getWatchdogCommand[1] = {(byte)SBrickCommandType::GET_WATCHDOG_TIMEOUT};
    WriteValue(getWatchdogCommand, 1);

    uint8_t rawValue = _pRemoteCharacteristicRemoteControl->readValue<uint8_t>();
    return rawValue;
}

void SBrickHub::WriteValue(byte command[], int size)
{
    log_d("writing command: %s", getHexString(command, size).c_str());

    byte commandBytes[size];
    memcpy(commandBytes, command, size);
    _pRemoteCharacteristicRemoteControl->writeValue(commandBytes, sizeof(commandBytes), false);
}

void SBrickHub::stopMotor(byte port)
{
    byte brakeCommand[2] = {(byte)SBrickCommandType::BRAKE, port};
    WriteValue(brakeCommand, 2);
}

void SBrickHub::setMotorSpeed(byte port, int speed)
{
    if (speed > SBRICK_MAX_CHANNEL_SPEED)
    {
        speed = SBRICK_MAX_CHANNEL_SPEED;
    }
    else if (speed < SBRICK_MIN_CHANNEL_SPEED)
    {
        speed = SBRICK_MIN_CHANNEL_SPEED;
    }
    byte driveCommand[4] = {(byte)SBrickCommandType::DRIVE, port, (speed > 0), (byte)abs(speed)};
    WriteValue(driveCommand, 4);
}

float SBrickHub::getBatteryLevel()
{
    byte voltageCommand[2] = {(byte)SBrickCommandType::QUERY_ADC, (byte)SBrickAdcChannel::Voltage};
    WriteValue(voltageCommand, 2);

    uint16_t rawAdcValue = _pRemoteCharacteristicRemoteControl->readValue<uint16_t>();
    log_w("raw value: %04x", rawAdcValue);
    if (rawAdcValue)
    {   
        int value = rawAdcValue >> 4;
        int channel = rawAdcValue & 0x000F;
        float voltage = (value * 0.83875F) / 127.0;

        log_w("voltage: %f, channel: %d", voltage, channel);
        return voltage;
    }

    // log_w("raw value: %04x", rawValue);
    // if (rawValue)
    // {
    //     float voltage = (rawValue * 0.83875F) / 2047; // 127;
    //     log_w("voltage1: %f", voltage);

    //     uint16_t rawValue2 = rawValue & 0x0FFF; // remove channel
    //     float voltage2 = (rawValue2 * 0.83875F) / 127;
    //     log_w("voltage2: %f", voltage2);

    //     uint16_t rawValue3 = rawValue & 0x0FFF; // remove channel
    //     float voltage3 = (rawValue3 * 0.83875F) / 2047;
    //     log_w("voltage3: %f", voltage3);        

    //     uint16_t rawValue4 = rawValue >> 4; // remove channel
    //     uint8_t channel4 = rawValue & 0x000F;
    //     float voltage4 = (rawValue4 * 0.83875F) / 127;
    //     log_w("voltage4: %f, channel: %d", voltage4, channel4);

    //     return voltage;
    // }

    return 0.0;
}

float SBrickHub::getTemperature()
{
    byte voltageCommand[2] = {(byte)SBrickCommandType::QUERY_ADC, (byte)SBrickAdcChannel::Temperature};
    WriteValue(voltageCommand, 2);

    uint16_t rawAdcValue = _pRemoteCharacteristicRemoteControl->readValue<uint16_t>();
    log_w("raw value: %04x", rawAdcValue);
    if (rawAdcValue)
    {   
        int value = rawAdcValue >> 4;
        int channel = rawAdcValue & 0x000F;
        float temp = (value / 0.13461) - 160.0;
        float temp2 = (value / 118.85795) - 160.0;
        float temp3 = (value * 0.008413396) - 160.0;
        //float temp4 = (result[1] * 256 + result[0]) * 0.008413396 - 160

        log_w("temp: %f, temp2: %f, temp3: %f, channel: %d", temp, temp2, temp3, channel);
        return temp;
    }

    //uint16_t rawValue = _pRemoteCharacteristicRemoteControl->readValue<uint16_t>();
    // log_w("raw value: %04x", rawValue);
    // if (rawValue)
    // {
    //     float temp = (rawValue / 0.13461) - 160.0;
    //     log_w("temp1: %f", temp);

    //     uint16_t rawValue2 = rawValue & 0x0FFF; // remove channel
    //     uint8_t channel2 = (rawValue >> 12) & 0x0F;
    //     float temp2 = (rawValue2 / 0.13461) - 160.0;
    //     log_w("temp2: %f, channel: %d", temp2, channel2);

    //     uint16_t rawValue3 = rawValue >> 4; // remove channel
    //     uint8_t channel3 = rawValue & 0x0F;
    //     float temp3 = (rawValue3 / 0.13461) - 160.0;
    //     log_w("temp3: %f, channel: %d", temp3, channel3);

    //     return temp;
    // }

    return 0.0;
}

void SBrickHub::notifyCallback(
    NimBLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
    log_w("notify callback for characteristic %s", pBLERemoteCharacteristic->getUUID().toString().c_str());
    log_w("data: [%s]", getHexString(pData, length).c_str());

    // TEMP
    uint8_t *pRawAdcValue;
    uint16_t rawAdcValue;
    int value;
    int channel;
    float voltage;

    uint8_t curRecordSize;
    SBrickRecordTypes curRecordType;
    for (int i = 0; i < length; i += curRecordSize + 1) // + 1 to include size byte before each record
    {
        curRecordSize = pData[i];
        curRecordType = (SBrickRecordTypes)pData[i + 1];

        switch (curRecordType)
        {
        case SBrickRecordTypes::ProductType:
            log_w("product type: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());
            break;
        case SBrickRecordTypes::DeviceIdentifier:
            log_w("device identifier: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());
            break;
        case SBrickRecordTypes::SimpleSecurityStatus:
            log_w("simple security status: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());
            break;
        case SBrickRecordTypes::CommandResponse:
            log_w("command response: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());
            break;
        case SBrickRecordTypes::ThermalProtectionStatus:
            log_w("thermal protection status: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());
            break;
        case SBrickRecordTypes::VoltageMeasurement:
            log_w("voltage measurement: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());

            // TODO: parseVoltageMeasurement function?

            pRawAdcValue = pData + i + 2;

            for (int j = 0; j < curRecordSize - 1; j += 2)
            {
                rawAdcValue = pRawAdcValue[j] | (pRawAdcValue[j + 1] << 8);
                log_w("raw value: %04x", rawAdcValue);

                value = rawAdcValue >> 4;
                channel = rawAdcValue & 0x000F;
                voltage = (value * 0.83875F) / 127.0;

                log_w("channel: %d, voltage: %f, ", channel, voltage);
            }

            // rawAdcValue = pData[i + 3] | (pData[i + 2] << 8);
            // log_w("raw value: %04x", rawAdcValue);

            // value = rawAdcValue >> 4;
            // channel = rawAdcValue & 0x000F;
            // voltage = (value * 0.83875F) / 127.0;

            // log_w("voltage: %f, channel: %d", voltage, channel);

            // log_w("datasize: %d", curRecordSize - 1);
            // log_w("channel?: %d", pData[i + 2] & 0x0F);
            // log_w("channel?: %d", pData[i + 2] >> 4);
            // log_w("channel?: %d", pData[i + 3] & 0x0F);
            // log_w("channel?: %d", pData[i + 3] >> 4);
            // //log_w("voltage1?: %f", (mData[2] | (mData[3] << 8)) * 0.83875F / 2047);

            break;
        case SBrickRecordTypes::SignalCompleted:
            log_w("signal completed: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());
            break;
        default:
            log_w("unknown record type: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());
            break;
        }
    }

}
