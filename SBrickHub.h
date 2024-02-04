#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "SBrickHubConst.h"

using namespace std::placeholders;

typedef void (*ChannelValueChangeCallback)(void *hub, byte channel, float voltage);

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
    void activateAdcChannel(byte channel);
    void deactivateAdcChannel(byte channel);
    void subscribeAdcChannel(byte channel, ChannelValueChangeCallback channelValueChangeCallback = nullptr);
    void unsubscribeAdcChannel(byte channel);
    float readAdcChannel(byte port);
    // void getActiveChannels();
    // void getSubscribedChannels();
    float getBatteryLevel();
    float getTemperature();
    
    // sensor related methods
    byte detectPortSensor(byte port);
    void subscribeSensor(byte port, ChannelValueChangeCallback channelValueChangeCallback = nullptr);
    byte interpretSensorMotion(float voltage, float neutralV);
    byte interpretSensorTilt(float voltage, float neutralV);

    void setMotorSpeed(byte port, int speed);
    void stopMotor(byte port);

private:
    friend class SBrickHubClientCallback;
    friend class SBrickHubAdvertisedDeviceCallbacks;
    struct AdcChannel
    {
        byte Channel;
        ChannelValueChangeCallback Callback;
    };

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

    template <typename T>
    T readValue();
    void writeValue(byte command[], int size);

    // ADC specific stuff
    void parseAdcReading(uint16_t rawReading, byte &channel, float &voltage);
    uint8_t getIndexForChannel(byte channel);
    void updateActiveAdcChannels();
    void updateSubscribedAdcChannels();
    AdcChannel _activeAdcChannels[SBRICK_ADC_CHANNEL_COUNT];
    int _numberOfActiveChannels = 0;

    uint32_t _scanDuration = 10;
};