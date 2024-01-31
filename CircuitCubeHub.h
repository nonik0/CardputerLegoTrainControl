#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "CircuitCubeHubConst.h"

using namespace std::placeholders;

class CircuitCubeHub
{
public:
    CircuitCubeHub();

    void init();
    void init(uint32_t scanDuration);
    void init(std::string deviceAddress);
    void init(std::string deviceAddress, uint32_t scanDuration);

    bool connectHub();
    bool isConnected();
    bool isConnecting();
    NimBLEAddress getHubAddress();
    std::string getHubName();
    void setHubName(char name[]);
    void shutDownHub();

    void WriteValue(byte command[], int size);

    void stopAllMotors();
    void stopMotor(byte port);
    void setMotorSpeed(byte port, int speed);

    void notifyCallback(NimBLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);
    BLEUUID _bleUuid;
    BLEUUID _characteristicWriteUuid;
    BLEUUID _characteristicNotifyUuid;
    BLEAddress *_pServerAddress;
    BLEAddress *_requestedDeviceAddress = nullptr;
    BLERemoteCharacteristic *_pRemoteWriteCharacteristic;
    BLERemoteCharacteristic *_pRemoteNotifyCharacteristic;
    std::string _hubName;
    bool _isConnecting;
    bool _isConnected;

private:
    uint32_t _scanDuration = 10;
};