#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "CircuitCubesHubConst.h"

using namespace std::placeholders;

class CircuitCubesHub
{
public:
    CircuitCubesHub();

    void init();
    void init(uint32_t scanDuration);
    void init(std::string deviceAddress);
    void init(std::string deviceAddress, uint32_t scanDuration);

    bool connectHub();
    void disconnectHub();
    bool isConnected();
    bool isConnecting();
    NimBLEAddress getHubAddress();
    std::string getHubName();
    void setHubName(char name[]);
    //void shutDownHub();

    int getRssi();

    void WriteValue(byte command[], int size);

    void stopAllMotors();
    void stopMotor(byte port);
    void setMotorSpeed(byte port, int speed);
    float getBatteryLevel();

    void notifyCallback(NimBLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);
    NimBLEUUID _bleUuid;
    NimBLEUUID _characteristicWriteUuid;
    NimBLEUUID _characteristicNotifyUuid;
    NimBLEClient *_pClient;
    NimBLEAddress *_pServerAddress;
    NimBLEAddress *_requestedDeviceAddress = nullptr;
    NimBLERemoteCharacteristic *_pRemoteCharacteristicWrite;
    NimBLERemoteCharacteristic *_pRemoteCharacteristicNotify;
    std::string _hubName;
    bool _isConnecting;
    bool _isConnected;

private:
    uint32_t _scanDuration = 10;
    float _batteryVoltage;
};