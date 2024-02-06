#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <PowerFunctions.h>
#include <WiFi.h>

PowerFunctions powerFunctionsCtl(IR_TX_PIN);

enum struct PowerFunctionsRepeatMode
{
    Off = 0x00,
    Broadcast = 0x01,
    Repeat = 0x02,
};

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

class PowerFunctionsIrBroadcast
{
private:
    static PowerFunctionsIrMessage _recvMessage;

    PowerFunctionsRepeatMode _mode = PowerFunctionsRepeatMode::Off;
    PowerFunctionsIrMessage _message;
    uint8_t _broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_peer_info_t _broadcastPeerInfo;

    void _broadcastMessage(PowerFunctionsCall call, PowerFunctionsPort port, PowerFunctionsPwm pwm, uint8_t channel)
    {
        _message = {call, port, pwm, channel};
        esp_err_t result = esp_now_send(_broadcastAddress, (uint8_t *)&_message, sizeof(_message));
        if (result != ESP_OK)
        {
            log_e("Error sending message: %s", esp_err_to_name(result));
        }
    }

    static void _onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
    {
        PowerFunctionsIrMessage message = _recvMessage;

        memcpy(&message, incomingData, sizeof(message));
        Serial.printf("\n[P:%d|S:%d|C:%d]\n", message.port, message.pwm, message.channel);

        switch (message.call)
        {
        case PowerFunctionsCall::SinglePwm:
            powerFunctionsCtl.single_pwm(message.port, message.pwm, message.channel);
            break;
        case PowerFunctionsCall::SingleIncrement:
            powerFunctionsCtl.single_increment(message.port, message.channel);
            break;
        case PowerFunctionsCall::SingleDecrement:
            powerFunctionsCtl.single_decrement(message.port, message.channel);
            break;
        }
    }

public:
    PowerFunctionsIrBroadcast() {}

    void single_pwm(PowerFunctionsPort port, PowerFunctionsPwm pwm, uint8_t channel)
    {
        powerFunctionsCtl.single_pwm(port, pwm, channel);

        if (_mode == PowerFunctionsRepeatMode::Broadcast)
        {
            _broadcastMessage(PowerFunctionsCall::SinglePwm, port, pwm, channel);
        }
    }

    void single_increment(PowerFunctionsPort port, uint8_t channel)
    {
        powerFunctionsCtl.single_increment(port, channel);

        if (_mode == PowerFunctionsRepeatMode::Broadcast)
        {
            _broadcastMessage(PowerFunctionsCall::SingleIncrement, port, PowerFunctionsPwm::FLOAT, channel);
        }
    }

    void single_decrement(PowerFunctionsPort port, uint8_t channel)
    {
        powerFunctionsCtl.single_decrement(port, channel);

        if (_mode == PowerFunctionsRepeatMode::Broadcast)
        {
            _broadcastMessage(PowerFunctionsCall::SingleDecrement, port, PowerFunctionsPwm::FLOAT, channel);
        }
    }

    PowerFunctionsPwm speedToPwm(byte speed)
    {
        return powerFunctionsCtl.speedToPwm(speed);
    }

    void setMode(PowerFunctionsRepeatMode mode)
    {
        if (mode == _mode)
        {
            return;
        }
        _mode = mode;

        if (_mode == PowerFunctionsRepeatMode::Off)
        {
            log_w("Disabling broadcast mode");
            esp_now_deinit();
            WiFi.mode(WIFI_OFF);
            return;
        }

        WiFi.mode(WIFI_STA);

        esp_err_t result;
        if ((result = esp_now_init()) != ESP_OK)
        {
            log_e("Error initializing ESP-NOW: %s", esp_err_to_name(result));
            return;
        }

        if (_mode == PowerFunctionsRepeatMode::Broadcast)
        {
            log_w("Enabling broadcast mode");
            memcpy(_broadcastPeerInfo.peer_addr, _broadcastAddress, 6);
            _broadcastPeerInfo.channel = 0;
            _broadcastPeerInfo.encrypt = false;

            if (esp_now_add_peer(&_broadcastPeerInfo) != ESP_OK)
            {
                log_e("Failed to add peer");
                return;
            }
        }
        if (_mode == PowerFunctionsRepeatMode::Repeat)
        {
            log_w("Enabling repeat mode");
            esp_now_register_recv_cb(_onDataRecv);
        }
    }
};