#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <PowerFunctions.h>
#include <WiFi.h>

using namespace std::placeholders;

enum struct PowerFunctionsCall
{
    SinglePwm,
    SingleIncrement,
    SingleDecrement,
    ComboPwm
};

struct PowerFunctionsIrMessage
{
    PowerFunctionsCall call;
    PowerFunctionsPort port;
    PowerFunctionsPwm pwm;
    uint8_t channel;
};

typedef void (*PowerFunctionsIrRecvCallback)(PowerFunctionsIrMessage message);

class PowerFunctionsIrBroadcast
{
private:
    static PowerFunctionsIrMessage _recvMessage;

    bool _broadcastEnabled = false;
    PowerFunctionsIrMessage _message;
    uint8_t _broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_peer_info_t _broadcastPeerInfo;

    void _broadcastMessage(PowerFunctionsCall call, PowerFunctionsPort port, PowerFunctionsPwm pwm, uint8_t channel);

public:
    static void _onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);

    PowerFunctionsIrBroadcast() {}
    void single_pwm(PowerFunctionsPort port, PowerFunctionsPwm pwm, uint8_t channel, bool rebroadcast = true);
    void single_increment(PowerFunctionsPort port, uint8_t channel, bool rebroadcast = true);
    void single_decrement(PowerFunctionsPort port, uint8_t channel, bool rebroadcast = true);
    PowerFunctionsPwm speedToPwm(byte speed);
    void registerRecvCallback(PowerFunctionsIrRecvCallback callback = nullptr);
    void unregisterRecvCallback();
    void disableBroadcast();
    void enableBroadcast();
};