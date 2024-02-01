#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "SBrickHubConst.h"

using namespace std::placeholders;

typedef void (*ChannelValueChangeCallback)(void *hub, byte channelNumber, float voltage);

struct AdcChannel
{
    byte Channel;
    ChannelValueChangeCallback Callback;
};

class SBrickHub
{
public:
    // constructor
    SBrickHub();

    // initializer methods
    void init();
    void init(uint32_t scanDuration);
    void init(std::string deviceAddress);
    void init(std::string deviceAddress, uint32_t scanDuration);

    // hub related methods
    bool connectHub();
    void disconnectHub();
    bool isConnected();
    bool isConnecting();
    NimBLEAddress getHubAddress();
    std::string getHubName();
    int getRssi();
    uint8_t getWatchdogTimeout();
    void rebootHub();
    void setHubName(char name[]);
    void setWatchdogTimeout(uint8_t tenthOfSeconds);

    // ADC related methods
    void activateAdcChannel(byte channel, ChannelValueChangeCallback channelValueChangeCallback = nullptr);
    void deactivateAdcChannel(byte channel);
    float readAdcChannel(byte port);
    // void getActiveChannels();
    // void getActiveChannelNotifications();
    float getBatteryLevel();
    float getTemperature();

    void setMotorSpeed(byte port, int speed);
    void stopMotor(byte port);

    // parse methods
    void parseAdcReading(uint16_t *pData);

    // BLE specific stuff
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
    template <typename T>
    T readValue();
    void writeValue(byte command[], int size);

    void parseAdcReading(uint16_t rawReading, byte &channel, float &voltage);
    void updateActiveAdcChannels();

    // List of active ADC channels
    AdcChannel _activeAdcChannels[SBRICK_ADC_CHANNEL_COUNT];
    int _numberOfActiveChannels = 0;

    uint32_t _scanDuration = 10;
};