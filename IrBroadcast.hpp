#include <Arduino.h>
#include <esp_now.h>
#include <PowerFunctions.h>
#include <WiFi.h>

#define IR_TX_PIN 44

PowerFunctions _irTrainCtl(IR_TX_PIN);

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

class IrBroadcast
{
private:
    bool _mode = false;

    PowerFunctionsIrMessage _irBroadcastMsg;
    uint8_t _broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_peer_info_t _peerInfo;

    void _broadcastMessage(PowerFunctionsCall call, PowerFunctionsPort port, PowerFunctionsPwm pwm, uint8_t channel)
    {
        _irBroadcastMsg = {call, port, pwm, channel};
        esp_err_t result = esp_now_send(_broadcastAddress, (uint8_t *)&_irBroadcastMsg, sizeof(_irBroadcastMsg));
        if (result != ESP_OK)
        {
            log_e("Error sending message: %s", esp_err_to_name(result));
        }
    }

public:
    IrBroadcast() {}

    void single_pwm(PowerFunctionsPort port, PowerFunctionsPwm pwm, uint8_t channel)
    {
        _irTrainCtl.single_pwm(port, pwm, channel);

        if (_mode)
        {
            _broadcastMessage(PowerFunctionsCall::SinglePwm, port, pwm, channel);
        }
    }

    void single_increment(PowerFunctionsPort port, uint8_t channel)
    {
        _irTrainCtl.single_increment(port, channel);

        if (_mode)
        {
            _broadcastMessage(PowerFunctionsCall::SingleIncrement, port, PowerFunctionsPwm::FLOAT, channel);
        }
    }

    void single_decrement(PowerFunctionsPort port, uint8_t channel)
    {
        _irTrainCtl.single_decrement(port, channel);

        if (_mode)
        {
            _broadcastMessage(PowerFunctionsCall::SingleDecrement, port, PowerFunctionsPwm::FLOAT, channel);
        }
    }

    PowerFunctionsPwm speedToPwm(byte speed)
    {
        return _irTrainCtl.speedToPwm(speed);
    }

    void setBroadcastMode(bool mode)
    {
        if (mode == _mode)
        {
            return;
        }
        _mode = mode;

        if (mode)
        {
            log_w("Enabling broadcast mode");

            WiFi.mode(WIFI_STA);

            if (esp_now_init() != ESP_OK)
            {
                log_e("Error initializing ESP-NOW");
                return;
            }

            memcpy(_peerInfo.peer_addr, _broadcastAddress, 6);
            _peerInfo.channel = 0;
            _peerInfo.encrypt = false;

            if (esp_now_add_peer(&_peerInfo) != ESP_OK)
            {
                log_e("Failed to add peer");
                return;
            }

            // esp_now_register_send_cb([](const uint8_t *mac_addr, esp_now_send_status_t status)
            // {
            //     log_w("\r\nLast Packet Send Status:\t");
            //     log_w(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
            // });
        }
        else
        {
            log_w("Disabling broadcast mode");
            esp_now_deinit();
            WiFi.mode(WIFI_OFF);
        }
    }
};