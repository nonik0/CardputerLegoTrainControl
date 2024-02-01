#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "SBrickHubConst.h"

using namespace std::placeholders;

class SBrickHub
{
public:
    SBrickHub();

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
    void rebootHub();
    void setHubName(char name[]);

    void configureSensor(byte port);
    float readSensorData(byte port);

    int getRssi();
    void setWatchdogTimeout(uint8_t tenthOfSeconds);
    uint8_t getWatchdogTimeout();

    void WriteValue(byte command[], int size);

    void stopMotor(byte port);
    void setMotorSpeed(byte port, int speed);
    float getBatteryLevel();
    float getTemperature();

    void notifyCallback(NimBLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);
    NimBLEUUID _bleUuid;
    NimBLEUUID _characteristicRemoteControlUuid;
    NimBLEUUID _characteristicQuickDriveUuid;
    NimBLEClient *_pClient;
    NimBLEAddress *_pServerAddress;
    NimBLEAddress *_requestedDeviceAddress = nullptr;
    NimBLERemoteCharacteristic *_pRemoteCharacteristicRemoteControl;
    NimBLERemoteCharacteristic *_pRemoteCharacteristicQuickDrive;
    std::string _hubName;
    bool _isConnecting;
    bool _isConnected;

private:
    uint32_t _scanDuration = 10;
};