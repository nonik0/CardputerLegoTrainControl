#include "CircuitCubesHub.h"

// TODO fix conflict with sbrick
std::string gexHexString2(byte *pData, size_t length)
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

class CircuitCubesHubClientCallback : public NimBLEClientCallbacks
{
    CircuitCubesHub *_circuitCubesHub;

public:
    CircuitCubesHubClientCallback(CircuitCubesHub *circuitCubeHub) : NimBLEClientCallbacks()
    {
        _circuitCubesHub = circuitCubeHub;
    }

    void onConnect(NimBLEClient *bleClient)
    {
    }

    void onDisconnect(NimBLEClient *bleClient)
    {
        _circuitCubesHub->_isConnecting = false;
        _circuitCubesHub->_isConnected = false;
        log_d("disconnected client");
    }
};

class CircuitCubesHubAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
    CircuitCubesHub *_circuitCubesHub;

public:
    CircuitCubesHubAdvertisedDeviceCallbacks(CircuitCubesHub *circuitCubeHub) : NimBLEAdvertisedDeviceCallbacks()
    {
        _circuitCubesHub = circuitCubeHub;
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

        if (advertisedDevice->haveServiceUUID() && advertisedDevice->getServiceUUID().equals(_circuitCubesHub->_bleUuid) 
            && (_circuitCubesHub->_requestedDeviceAddress == nullptr || advertisedDevice->getAddress().equals(*_circuitCubesHub->_requestedDeviceAddress)))
        {
            advertisedDevice->getScan()->stop();
            _circuitCubesHub->_pServerAddress = new NimBLEAddress(advertisedDevice->getAddress());
            _circuitCubesHub->_hubName = advertisedDevice->getName();

            if (advertisedDevice->haveManufacturerData())
            {
                uint8_t *manufacturerData = (uint8_t *)advertisedDevice->getManufacturerData().data();
                uint8_t manufacturerDataLength = advertisedDevice->getManufacturerData().length();
                log_d("manufacturer data: %x", manufacturerData);
            }
            _circuitCubesHub->_isConnecting = true;
        }
    }
};

CircuitCubesHub::CircuitCubesHub() {}

void CircuitCubesHub::init()
{
    _isConnected = false;
    _isConnecting = false;
    _bleUuid = NimBLEUUID(CIRCUITCUBES_SERVICE_UUID);
    _characteristicNotifyUuid = NimBLEUUID(CIRCUITCUBES_NOTIFY_CHARACTERISTIC_UUID);
    _characteristicWriteUuid = NimBLEUUID(CIRCUITCUBES_WRITE_CHARACTERISTIC_UUID);

    NimBLEDevice::init("");
    NimBLEScan *pBLEScan = NimBLEDevice::getScan();

    pBLEScan->setAdvertisedDeviceCallbacks(new CircuitCubesHubAdvertisedDeviceCallbacks(this));

    pBLEScan->setActiveScan(true);
    // start method with callback function to enforce the non blocking scan. If no callback function is used,
    // the scan starts in a blocking manner
    pBLEScan->start(_scanDuration, CircuitCubesHubAdvertisedDeviceCallbacks::scanEndedCallback);
}

void CircuitCubesHub::init(std::string deviceAddress)
{
    _requestedDeviceAddress = new BLEAddress(deviceAddress);
    init();
}

void CircuitCubesHub::init(uint32_t scanDuration)
{
    _scanDuration = scanDuration;
    init();
}

void CircuitCubesHub::init(std::string deviceAddress, uint32_t scanDuration)
{
    _requestedDeviceAddress = new BLEAddress(deviceAddress);
    _scanDuration = scanDuration;
    init();
}

bool CircuitCubesHub::connectHub()
{
    NimBLEAddress pAddress = *_pServerAddress;

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

    _pRemoteCharacteristicWrite = pRemoteService->getCharacteristic(_characteristicWriteUuid);
    _pRemoteCharacteristicNotify = pRemoteService->getCharacteristic(_characteristicNotifyUuid);
    if (_pRemoteCharacteristicWrite == nullptr || _pRemoteCharacteristicNotify == nullptr)
    {
        log_e("failed to get ble service");
        return false;
    }

    // register notifications (callback function) for the characteristic
    if (_pRemoteCharacteristicNotify->canNotify())
    {
        _pRemoteCharacteristicNotify->subscribe(true, std::bind(&CircuitCubesHub::notifyCallback, this, _1, _2, _3, _4), true);
    }

    // add callback instance to get notified if a disconnect event appears
    _pClient->setClientCallbacks(new CircuitCubesHubClientCallback(this));

    // Set states
    _isConnected = true;
    _isConnecting = false;
    return true;
}

void CircuitCubesHub::disconnectHub()
{
    log_d("disconnecting client");
    NimBLEDevice::deleteClient(_pClient);
}

bool CircuitCubesHub::isConnecting()
{
    return _isConnecting;
}

bool CircuitCubesHub::isConnected()
{
    return _isConnected;
}

NimBLEAddress CircuitCubesHub::getHubAddress()
{
    NimBLEAddress pAddress = *_pServerAddress;
    return pAddress;
}

std::string CircuitCubesHub::getHubName() { return _hubName; }

void CircuitCubesHub::setHubName(char name[])
{
}

int CircuitCubesHub::getRssi()
{
    return _pClient->getRssi();
}

void CircuitCubesHub::WriteValue(byte command[], int size)
{
    log_d("writing command: %s", std::string((char*)command, size).c_str());

    byte commandBytes[size];
    memcpy(commandBytes, command, size);
    _pRemoteCharacteristicWrite->writeValue(commandBytes, sizeof(commandBytes), false);
}

void CircuitCubesHub::stopAllMotors()
{
    byte command[1] = {(byte)CircuitCubesCommandType::BRAKEALL};
    WriteValue(command, 1);
}

void CircuitCubesHub::stopMotor(byte port)
{
    setMotorSpeed(port, 0);
}

void CircuitCubesHub::setMotorSpeed(byte port, int speed)
{
    if (speed > CIRCUIT_CUBES_SPEED_MAX)
    {
        speed = CIRCUIT_CUBES_SPEED_MAX;
    }
    else if (speed < CIRCUIT_CUBES_SPEED_MIN)
    {
        speed = CIRCUIT_CUBES_SPEED_MIN;
    }

    char direction = (speed > 0) ? '+' : '-';
    char speedBuf[4];
    sprintf(speedBuf, "%03d", abs(speed));

    byte driveCommand[5] = {direction, speedBuf[0], speedBuf[1], speedBuf[2], (byte)('a' + port)};
    WriteValue(driveCommand, 5);
}

float CircuitCubesHub::getBatteryLevel()
{
    byte command[1] = {(byte)CircuitCubesCommandType::BATTERY};
    WriteValue(command, 1);

    // TODO: improve with callback?
    return _batteryVoltage;
}

void CircuitCubesHub::notifyCallback(
    NimBLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
    log_d("notify callback for characteristic %s", pBLERemoteCharacteristic->getUUID().toString().c_str());

    if (length > 1) {
        _batteryVoltage = std::stof(std::string((char*)pData, length));
    }
}
