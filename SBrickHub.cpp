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

void SBrickHub::activateAdcChannel(byte channel, ChannelValueChangeCallback channelValueChangeCallback)
{
    log_d("channel: %x", channel);

    AdcChannel newActiveChannel = {channel, channelValueChangeCallback};
    _activeAdcChannels[_numberOfActiveChannels] = newActiveChannel;
    _numberOfActiveChannels++;

    updateActiveAdcChannels();
}

void SBrickHub::deactivateAdcChannel(byte channel)
{
    log_d("channel: %x", channel);

    bool hasReachedRemovedIndex = false;
    for (int i = 0; i < _numberOfActiveChannels; i++)
    {
        if (hasReachedRemovedIndex)
        {
            _activeAdcChannels[i - 1] = _activeAdcChannels[i];
        }
        if (!hasReachedRemovedIndex && _activeAdcChannels[i].Channel == channel)
        {
            hasReachedRemovedIndex = true;
        }
    }
    if (_numberOfActiveChannels > 0)
    {
        _numberOfActiveChannels--;
    }

    updateActiveAdcChannels();
}

float SBrickHub::readAdcChannel(byte adcChannel)
{
    byte queryAdcCommand[2] = {(byte)SBrickCommandType::QUERY_ADC, adcChannel};
    writeValue(queryAdcCommand, 2);

    uint16_t rawAdcValue = readValue<uint16_t>();
    log_d("ADC[%d]=%04x", adcChannel, rawAdcValue);

    byte channel;
    float voltage;
    parseAdcReading(rawAdcValue, channel, voltage);

    return voltage;
}

float SBrickHub::getBatteryLevel()
{
    byte voltageCommand[2] = {(byte)SBrickCommandType::QUERY_ADC, (byte)SBrickAdcChannel::Voltage};
    writeValue(voltageCommand, 2);

    return readAdcChannel((byte)SBrickAdcChannel::Voltage);
}

float SBrickHub::getTemperature()
{
    byte voltageCommand[2] = {(byte)SBrickCommandType::QUERY_ADC, (byte)SBrickAdcChannel::Temperature};
    writeValue(voltageCommand, 2);

    return readAdcChannel((byte)SBrickAdcChannel::Temperature);
}

void SBrickHub::rebootHub()
{
    byte rebootCommand[1] = {(byte)SBrickCommandType::REBOOT};
    writeValue(rebootCommand, 1);
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
    writeValue(setNameCommand, arraySize);
}

int SBrickHub::getRssi()
{
    return _pClient->getRssi();
}

void SBrickHub::setWatchdogTimeout(uint8_t tenthOfSeconds)
{
    byte setWatchdogCommand[2] = {(byte)SBrickCommandType::SET_WATCHDOG_TIMEOUT, tenthOfSeconds};
    writeValue(setWatchdogCommand, 2);
}

uint8_t SBrickHub::getWatchdogTimeout()
{
    byte getWatchdogCommand[1] = {(byte)SBrickCommandType::GET_WATCHDOG_TIMEOUT};
    writeValue(getWatchdogCommand, 1);

    return readValue<uint8_t>();
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
    writeValue(driveCommand, 4);
}

void SBrickHub::stopMotor(byte port)
{
    byte brakeCommand[2] = {(byte)SBrickCommandType::BRAKE, port};
    writeValue(brakeCommand, 2);
}

void SBrickHub::notifyCallback(
    NimBLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
    log_w("notify callback for characteristic %s", pBLERemoteCharacteristic->getUUID().toString().c_str());
    log_w("data: [%s]", getHexString(pData, length).c_str());

    uint8_t curRecordSize;
    SBrickRecordType curRecordType;
    uint16_t rawAdcValue;
    byte channel;
    float voltage;
    for (int i = 0; i < length; i += curRecordSize + 1) // + 1 to include size byte before each record
    {
        curRecordSize = pData[i] - 1; // subtract 1 to exclude record type bit
        curRecordType = (SBrickRecordType)pData[i + 1];
        uint8_t *pRecordData = pData + i + 2;

        switch (curRecordType)
        {
        case SBrickRecordType::ProductType:
            log_w("product type: %s", getHexString(pRecordData, curRecordSize).c_str());
            break;
        case SBrickRecordType::DeviceIdentifier:
            log_w("device identifier: %s", getHexString(pRecordData, curRecordSize).c_str());
            break;
        case SBrickRecordType::SimpleSecurityStatus:
            log_w("simple security status: %s", getHexString(pRecordData, curRecordSize).c_str());
            break;
        case SBrickRecordType::CommandResponse:
            log_w("command response: %s", getHexString(pRecordData, curRecordSize).c_str());
            break;
        case SBrickRecordType::ThermalProtectionStatus:
            log_w("thermal protection status: %s", getHexString(pRecordData, curRecordSize).c_str());
            break;
        case SBrickRecordType::VoltageMeasurement:
            log_w("voltage measurement: %s", getHexString(pRecordData, curRecordSize).c_str());

            for (int j = 0; j < curRecordSize - 1; j += 2)
            {
                rawAdcValue = pRecordData[j] | (pRecordData[j + 1] << 8); // little endian value
                parseAdcReading(rawAdcValue, channel, voltage);

                if (_activeAdcChannels[channel].Callback != nullptr)
                {
                    _activeAdcChannels[channel].Callback(this, channel, voltage);
                }
            }
            break;
        case SBrickRecordType::SignalCompleted:
            log_w("signal completed: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());
            break;
        default:
            log_w("unknown record type: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());
            break;
        }
    }
}

template <typename T>
T SBrickHub::readValue()
{
    return _pRemoteCharacteristicRemoteControl->readValue<T>();
}

void SBrickHub::writeValue(byte command[], int size)
{
    log_d("writing command: %s", getHexString(command, size).c_str());

    byte commandBytes[size];
    memcpy(commandBytes, command, size);
    _pRemoteCharacteristicRemoteControl->writeValue(commandBytes, sizeof(commandBytes), false);
}

void SBrickHub::parseAdcReading(uint16_t rawReading, byte &channel, float &voltage)
{
    channel = rawReading & 0x000F;
    voltage = ((rawReading >> 4) * 0.83875F) / 127.0;
    log_w("[%04x] -> channel: %d, voltage: %f, ", rawReading, channel, voltage);
}

void SBrickHub::updateActiveAdcChannels()
{
    byte command[1 + SBRICK_ADC_CHANNEL_COUNT] = {(byte)SBrickCommandType::SET_VOLTAGE_MEASURE};
    for (int i = 0; i < _numberOfActiveChannels; i++)
        command[i + 1] = _activeAdcChannels[i].Channel;

    writeValue(command, 1 + _numberOfActiveChannels);

    command[0] = (byte)SBrickCommandType::SET_VOLTAGE_NOTIF;
    writeValue(command, 1 + _numberOfActiveChannels);
}