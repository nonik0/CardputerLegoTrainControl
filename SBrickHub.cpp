#include "SBrickHub.h"

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
        if (advertisedDevice->getName() == "SBrick") {
            log_d("found SBrick device! [%s]", advertisedDevice->getAddress().toString().c_str());
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
                log_d("manufacturer data: %x", manufacturerData);
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
    _bleUuid = NimBLEUUID(SBRICK_SERVICE_UUID_REMOTE_CONTROL);
    _characteristicUuid = NimBLEUUID(SBRICK_CHARACTERISTIC_UUID_REMOTE_CONTROL);

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

    _pRemoteCharacteristic = pRemoteService->getCharacteristic(_characteristicUuid);
    if (_pRemoteCharacteristic == nullptr)
    {
        log_e("failed to get ble service");
        return false;
    }

    // register notifications (callback function) for the characteristic
    if (_pRemoteCharacteristic->canNotify())
    {
        _pRemoteCharacteristic->subscribe(true, std::bind(&SBrickHub::notifyCallback, this, _1, _2, _3, _4), true);
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

void SBrickHub::configureSensor(byte port)
{
    log_w("configuring sensor on port %d", port);

    byte setPeriodicVoltageMeasurementCommand[2] = {(byte)SBrickCommandType::SET_VOLTAGE_MEASURE, port};
    WriteValue(setPeriodicVoltageMeasurementCommand, 2);

    byte setUpPeriodicVoltageNotificationCommand[2] = {(byte)SBrickCommandType::SET_VOLTAGE_NOTIF, port};
    WriteValue(setUpPeriodicVoltageNotificationCommand, 2);
}

float SBrickHub::readSensorData(byte port)
{
    byte queryAdcCommand[2] = {(byte)SBrickCommandType::QUERY_ADC, port};
    WriteValue(queryAdcCommand, 2);

    uint16_t rawValue = _pRemoteCharacteristic->readValue<uint16_t>();
    log_d("port %x raw value: %04x", port, rawValue);
    if (rawValue)
    {
        float voltage = (rawValue * 0.83875F) / 2047; // 127 is wrong from protocol doc?
        log_i("port %x voltage: %f", port, voltage);
        return voltage;
    }

    return 0;
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

    uint8_t rawValue = _pRemoteCharacteristic->readValue<uint8_t>();
    return rawValue;
}

void SBrickHub::WriteValue(byte command[], int size)
{
    String commandHexString = "0x";
    for (int i = 0; i < size; i++) {
        char byteHexChars[3];
        sprintf(byteHexChars, "%02x", command[i]);
        commandHexString += String(byteHexChars, 2);
    }
    log_d("writing command: %s", commandHexString.c_str());

    byte commandBytes[size];
    memcpy(commandBytes, command, size);
    _pRemoteCharacteristic->writeValue(commandBytes, sizeof(commandBytes), false);
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
    byte voltageCommand[2] = {(byte)SBrickCommandType::QUERY_ADC, (byte)SBrickHubChannel::Voltage};
    WriteValue(voltageCommand, 2);

    uint16_t rawValue = _pRemoteCharacteristic->readValue<uint16_t>();
    if (rawValue)
    {
        float voltage = (rawValue * 0.83875F) / 2047; // 127;
        return voltage;
    }

    return 0;
}

void SBrickHub::notifyCallback(
    NimBLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
    log_w("notify callback for characteristic %s", pBLERemoteCharacteristic->getUUID().toString().c_str());

    log_w("data: [");
    for (int i = 0; i < length; i++)
    {
        log_w("%02x", pData[i]);
    }
    log_w("]");
}