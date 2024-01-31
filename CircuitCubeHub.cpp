#include "CircuitCubeHub.h"
#include <sstream>
#include <iomanip>

#include "CircuitCubeHub.h"

class CircuitCubeHubClientCallback : public NimBLEClientCallbacks
{
    CircuitCubeHub *_circuitCubeHub;

public:
    CircuitCubeHubClientCallback(CircuitCubeHub *circuitCubeHub) : NimBLEClientCallbacks()
    {
        _circuitCubeHub = circuitCubeHub;
    }

    void onConnect(NimBLEClient *bleClient)
    {
    }

    void onDisconnect(NimBLEClient *bleClient)
    {
        _circuitCubeHub->_isConnecting = false;
        _circuitCubeHub->_isConnected = false;
        log_d("disconnected client");
    }
};

class CircuitCubeHubAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
    CircuitCubeHub *_circuitCubeHub;

public:
    CircuitCubeHubAdvertisedDeviceCallbacks(CircuitCubeHub *circuitCubeHub) : NimBLEAdvertisedDeviceCallbacks()
    {
        _circuitCubeHub = circuitCubeHub;
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

        // SBrick does not advertise the service, so we need to connect directly instead of looking for the service in broadcasted devices
        if (_circuitCubeHub->_requestedDeviceAddress && advertisedDevice->getAddress().equals(*_circuitCubeHub->_requestedDeviceAddress))
        {
            advertisedDevice->getScan()->stop();
            _circuitCubeHub->_pServerAddress = new NimBLEAddress(advertisedDevice->getAddress());
            _circuitCubeHub->_hubName = advertisedDevice->getName();

            if (advertisedDevice->haveManufacturerData())
            {
                uint8_t *manufacturerData = (uint8_t *)advertisedDevice->getManufacturerData().data();
                uint8_t manufacturerDataLength = advertisedDevice->getManufacturerData().length();
                log_d("manufacturer data: %x", manufacturerData);
            }
            _circuitCubeHub->_isConnecting = true;
        }
    }
};

CircuitCubeHub::CircuitCubeHub() {}

void CircuitCubeHub::init()
{
    _isConnected = false;
    _isConnecting = false;
    _bleUuid = NimBLEUUID(CIRCUIT_CUBE_SERVICE_UUID);
    _characteristicNotifyUuid = NimBLEUUID(CIRCUIT_CUBE_NOTIFY_CHARACTERISTIC_UUID);
    _characteristicWriteUuid = NimBLEUUID(CIRCUIT_CUBE_WRITE_CHARACTERISTIC_UUID);

    NimBLEDevice::init("");
    NimBLEScan *pBLEScan = NimBLEDevice::getScan();

    pBLEScan->setAdvertisedDeviceCallbacks(new CircuitCubeHubAdvertisedDeviceCallbacks(this));

    pBLEScan->setActiveScan(true);
    // start method with callback function to enforce the non blocking scan. If no callback function is used,
    // the scan starts in a blocking manner
    pBLEScan->start(_scanDuration, CircuitCubeHubAdvertisedDeviceCallbacks::scanEndedCallback);
}

void CircuitCubeHub::init(std::string deviceAddress)
{
    _requestedDeviceAddress = new BLEAddress(deviceAddress);
    init();
}

void CircuitCubeHub::init(uint32_t scanDuration)
{
    _scanDuration = scanDuration;
    init();
}

void CircuitCubeHub::init(std::string deviceAddress, uint32_t scanDuration)
{
    _requestedDeviceAddress = new BLEAddress(deviceAddress);
    _scanDuration = scanDuration;
    init();
}

bool CircuitCubeHub::connectHub()
{
    NimBLEAddress pAddress = *_pServerAddress;
    NimBLEClient *pClient = nullptr;

    log_d("number of ble clients: %d", NimBLEDevice::getClientListSize());

    /** Check if we have a client we should reuse first **/
    if (NimBLEDevice::getClientListSize())
    {
        /** Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        pClient = NimBLEDevice::getClientByPeerAddress(pAddress);
        if (pClient)
        {
            if (!pClient->connect(pAddress, false))
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
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    /** No client to reuse? Create a new one. */
    if (!pClient)
    {
        if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS)
        {
            log_w("max clients reached - no more connections available: %d", NimBLEDevice::getClientListSize());
            return false;
        }

        pClient = NimBLEDevice::createClient();
    }

    if (!pClient->isConnected())
    {
        if (!pClient->connect(pAddress))
        {
            log_e("failed to connect");
            return false;
        }
    }

    log_d("connected to: %s, RSSI: %d", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());
    NimBLERemoteService *pRemoteService = pClient->getService(_bleUuid);
    if (pRemoteService == nullptr)
    {
        log_e("failed to get ble client");
        return false;
    }

    _pRemoteWriteCharacteristic = pRemoteService->getCharacteristic(_characteristicWriteUuid);
    _pRemoteNotifyCharacteristic = pRemoteService->getCharacteristic(_characteristicNotifyUuid);
    if (_pRemoteWriteCharacteristic == nullptr || _pRemoteNotifyCharacteristic == nullptr)
    {
        log_e("failed to get ble service");
        return false;
    }

    // register notifications (callback function) for the characteristic
    if (_pRemoteNotifyCharacteristic->canNotify())
    {
        _pRemoteNotifyCharacteristic->subscribe(true, std::bind(&CircuitCubeHub::notifyCallback, this, _1, _2, _3, _4), true);
    }

    // add callback instance to get notified if a disconnect event appears
    pClient->setClientCallbacks(new CircuitCubeHubClientCallback(this));

    // Set states
    _isConnected = true;
    _isConnecting = false;
    return true;
}

bool CircuitCubeHub::isConnecting()
{
    return _isConnecting;
}

bool CircuitCubeHub::isConnected()
{
    return _isConnected;
}

NimBLEAddress CircuitCubeHub::getHubAddress()
{
    NimBLEAddress pAddress = *_pServerAddress;
    return pAddress;
}

std::string CircuitCubeHub::getHubName() { return _hubName; }

void CircuitCubeHub::setHubName(char name[])
{
}

void CircuitCubeHub::shutDownHub()
{
}

void CircuitCubeHub::WriteValue(byte command[], int size)
{
    byte commandWithCommonHeader[size] = {(byte)(size), 0x00};
    memcpy(commandWithCommonHeader, command, size);
    _pRemoteWriteCharacteristic->writeValue(commandWithCommonHeader, sizeof(commandWithCommonHeader), false);
}

void CircuitCubeHub::stopAllMotors()
{
    byte command[1] = {(byte)CircuitCubesCommandType::BRAKEALL};
    WriteValue(command, 1);
}

void CircuitCubeHub::stopMotor(byte port)
{
    setMotorSpeed(port, 0);
}

void CircuitCubeHub::setMotorSpeed(byte port, int speed)
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

    byte driveCommand[5] = {direction, speedBuf[2], speedBuf[1], speedBuf[0], (byte)('a' + port)};
    WriteValue(driveCommand, 5);
}

void CircuitCubeHub::notifyCallback(
    NimBLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
    log_d("notify callback for characteristic %s", pBLERemoteCharacteristic->getUUID().toString().c_str());
    log_d("data: %x", pData);
}

// void CircuitCubeHub::Init()
// {
//     NimBLEDevice::init("CircuitCubeBLE");
//     NimBLEScan *pScan = NimBLEDevice::getScan();

//     // todo: register callback to make non-blocking
//     NimBLEScanResults results = pScan->start(2);

//     NimBLEUUID circuitCubeServiceUuid(CIRCUIT_CUBE_SERVICE_UUID);

//     for (int i = 0; i < results.getCount(); i++)
//     {
//         NimBLEAdvertisedDevice device = results.getDevice(i);
//         log_i("found device: %s", device.toString().c_str());
//         if ((m_Address.empty() || (device.getAddress() == BLEAddress(m_Address))) && (device.isAdvertisingService(circuitCubeServiceUuid)))
//         {
//             m_Client = NimBLEDevice::createClient();
//             log_i("found circuit cube %s", device.getName().c_str());
//             if (m_Client->connect(&device))
//             {
//                 NimBLERemoteService *pService = m_Client->getService(circuitCubeServiceUuid);
//                 if (pService)
//                 {
//                     m_TXChar = pService->getCharacteristic(CIRCUIT_CUBE_TX_CHARACTERISTICS_UUID);
//                     m_RXChar = pService->getCharacteristic(CIRCUIT_CUBE_RX_CHARACTERISTICS_UUID);

//                     if (m_TXChar && m_RXChar)
//                     {
//                         // read battery state
//                         m_TXChar->writeValue("b");
//                         std::string value = m_RXChar->readValue();
//                         log_i("read %s", value.c_str());
//                     }
//                 }
//             }
//         }
//     }
// }

// void CircuitCubeHub::SetVelocities(int vel1, int vel2, int vel3)
// {
//     if (m_TXChar)
//     {
//         m_TXChar->writeValue(
//             BuildVelocityCommand(0, vel1) + BuildVelocityCommand(1, vel2) + BuildVelocityCommand(2, vel3));
//     }
// }

// void CircuitCubeHub::SetVelocity(int channel, int vel)
// {
//     if (m_TXChar)
//     {
//         m_TXChar->writeValue(BuildVelocityCommand(channel, vel));
//     }
// }

// std::string CircuitCubeHub::BuildVelocityCommand(int channel, int velocity)
// {
//     std::stringstream cmd;
//     cmd << ((velocity < 0) ? "-" : "+")
//         << std::setfill('0') << std::setw(3) << ((velocity == 0) ? 0 : (55 + abs(velocity)))
//         << char('a' + channel);
//     return cmd.str();
// }