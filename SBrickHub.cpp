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

        // use any device that contains "SBrick" in the name
        if ((advertisedDevice->getName().find("SBrick") != std::string::npos) || (_sbrickHub->_requestedDeviceAddress && advertisedDevice->getAddress().equals(*_sbrickHub->_requestedDeviceAddress)))
        {
            advertisedDevice->getScan()->stop();
            _sbrickHub->_pServerAddress = new NimBLEAddress(advertisedDevice->getAddress());
            _sbrickHub->_hubName = advertisedDevice->getName();

            if (advertisedDevice->haveManufacturerData())
            {
                uint8_t *manufacturerData = (uint8_t *)advertisedDevice->getManufacturerData().data();
                uint8_t manufacturerDataLength = advertisedDevice->getManufacturerData().length();
                log_d("manufacturer data: [%s]", getHexString(manufacturerData, manufacturerDataLength).c_str());
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

    // temperature and voltage channels always active
    _activeAdcChannels[0] = {(byte)SBrickAdcChannel::Temperature, nullptr};
    _activeAdcChannels[1] = {(byte)SBrickAdcChannel::Voltage, nullptr};
    _numberOfActiveChannels = 2;

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

int SBrickHub::getRssi()
{
    return _pClient->getRssi();
}

uint8_t SBrickHub::getWatchdogTimeout()
{
    byte getWatchdogCommand[1] = {(byte)SBrickCommandType::GET_WATCHDOG_TIMEOUT};
    writeValue(getWatchdogCommand, 1);

    return readValue<uint8_t>();
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

void SBrickHub::setWatchdogTimeout(uint8_t tenthOfSeconds)
{
    byte setWatchdogCommand[2] = {(byte)SBrickCommandType::SET_WATCHDOG_TIMEOUT, tenthOfSeconds};
    writeValue(setWatchdogCommand, 2);
}

void SBrickHub::activateAdcChannel(byte channel)
{
    log_d("channel: %x", channel);

    AdcChannel newActiveChannel = {channel, nullptr};
    _activeAdcChannels[_numberOfActiveChannels] = newActiveChannel;
    _numberOfActiveChannels++;

    updateActiveAdcChannels();
}

void SBrickHub::deactivateAdcChannel(byte channel)
{
    log_d("channel: %x", channel);

    if (channel == (byte)SBrickAdcChannel::Temperature || channel == (byte)SBrickAdcChannel::Voltage)
    {
        return;
    }

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

void SBrickHub::subscribeAdcChannel(byte channel, ChannelValueChangeCallback channelValueChangeCallback)
{
    log_d("channel: %x", channel);

    int channelIndex = getIndexForChannel(channel);
    if (channelIndex < 0)
    {
        log_w("channel %x is not active", channel);
        return;
    }
    _activeAdcChannels[channelIndex].Callback = channelValueChangeCallback;

    updateSubscribedAdcChannels();
}

void SBrickHub::unsubscribeAdcChannel(byte channel)
{
    log_d("channel: %x", channel);

    int channelIndex = getIndexForChannel(channel);
    if (channelIndex < 0)
    {
        return;
    }
    _activeAdcChannels[channelIndex].Callback = nullptr;

    updateSubscribedAdcChannels();
}

float SBrickHub::readAdcChannel(byte adcChannel)
{
    byte queryAdcCommand[2] = {(byte)SBrickCommandType::QUERY_ADC, adcChannel};
    writeValue(queryAdcCommand, 2);

    uint16_t rawAdcValue = readValue<uint16_t>();

    byte channel;
    float voltage;
    parseAdcReading(rawAdcValue, channel, voltage);

    return voltage;
}

// void SBrickHub::getActiveChannels()
// {
// }

// void SBrickHub::getSubscribedChannels()
// {
// }

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
    log_d("notify callback for characteristic %s", pBLERemoteCharacteristic->getUUID().toString().c_str());
    log_d("data: [%s]", getHexString(pData, length).c_str());

    uint8_t curRecordSize;
    SBrickRecordType curRecordType;
    uint16_t rawAdcValue;
    byte channel;
    float voltage;
    for (int i = 0; i < length; i += curRecordSize + 2) // +2 -> size byte and record type byte
    {
        curRecordSize = pData[i] - 1; // subtract 1 to exclude record type bit
        curRecordType = (SBrickRecordType)pData[i + 1];
        uint8_t *pRecordData = pData + i + 2;

        switch (curRecordType)
        {
        case SBrickRecordType::ProductType:
            log_d("product type: %s", getHexString(pRecordData, curRecordSize).c_str());
            break;
        case SBrickRecordType::DeviceIdentifier:
            log_d("device identifier: %s", getHexString(pRecordData, curRecordSize).c_str());
            break;
        case SBrickRecordType::SimpleSecurityStatus:
            log_d("simple security status: %s", getHexString(pRecordData, curRecordSize).c_str());
            break;
        case SBrickRecordType::CommandResponse:
            log_d("command response: %s", getHexString(pRecordData, curRecordSize).c_str());
            break;
        case SBrickRecordType::ThermalProtectionStatus:
            log_d("thermal protection status: %s", getHexString(pRecordData, curRecordSize).c_str());
            break;
        case SBrickRecordType::VoltageMeasurement:
            log_d("voltage measurement: %s", getHexString(pRecordData, curRecordSize).c_str());

            for (int j = 0; j < curRecordSize - 1; j += 2)
            {
                rawAdcValue = pRecordData[j] | (pRecordData[j + 1] << 8); // little endian value
                parseAdcReading(rawAdcValue, channel, voltage);

                log_d("Channel %x = %fV", channel, voltage);

                uint8_t channelIndex = getIndexForChannel(channel);
                if (_activeAdcChannels[channelIndex].Callback != nullptr)
                {
                    log_d("calling callback for channel %x", channel);
                    _activeAdcChannels[channelIndex].Callback(this, channel, voltage);
                }
            }
            break;
        case SBrickRecordType::SignalCompleted:
            log_d("signal completed: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());
            break;
        default:
            log_d("unknown record type: %s", getHexString(pData + i + 2, curRecordSize - 1).c_str());
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
}

uint8_t SBrickHub::getIndexForChannel(byte channel)
{
    log_d("Number of active ADC channels: %d", _numberOfActiveChannels);
    for (int idx = 0; idx < _numberOfActiveChannels; idx++)
    {
        log_d("[%d] channel: %x, callback address: %x", idx, _activeAdcChannels[idx].Channel, _activeAdcChannels[idx].Callback);
        if (_activeAdcChannels[idx].Channel == channel)
        {
            log_d("active channel %x has index %d", channel, idx);
            return idx;
        }
    }
    log_w("channel %x is not active", channel);
    return -1;
}

void SBrickHub::updateActiveAdcChannels()
{
    byte command[1 + SBRICK_ADC_CHANNEL_COUNT] = {(byte)SBrickCommandType::SET_VOLTAGE_MEASURE};
    for (int i = 0; i < _numberOfActiveChannels; i++)
        command[i + 1] = _activeAdcChannels[i].Channel;

    writeValue(command, 1 + _numberOfActiveChannels);
}

void SBrickHub::updateSubscribedAdcChannels()
{
    byte command[1 + SBRICK_ADC_CHANNEL_COUNT] = {(byte)SBrickCommandType::SET_VOLTAGE_NOTIF};

    uint8_t numberOfSubscribedChannels = 0;
    for (int i = 0; i < _numberOfActiveChannels; i++)
    {
        if (_activeAdcChannels[i].Callback != nullptr)
        {
            command[numberOfSubscribedChannels + 1] = _activeAdcChannels[i].Channel;
            numberOfSubscribedChannels++;
        }
    }

    writeValue(command, 1 + numberOfSubscribedChannels);
}