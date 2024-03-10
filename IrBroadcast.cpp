#include "IrBroadcast.h"

PowerFunctions powerFunctionsCtl(IR_TX_PIN);
PowerFunctionsIrRecvCallback powerFunctionsIrRecvCallBackInstance;

void PowerFunctionsIrBroadcast::_broadcastMessage(PowerFunctionsCall call, PowerFunctionsPort port, PowerFunctionsPwm pwm, uint8_t channel)
{
    _message = {call, port, pwm, channel};
    esp_err_t result = esp_now_send(_broadcastAddress, (uint8_t *)&_message, sizeof(_message));
    if (result != ESP_OK)
    {
        log_e("Error sending message: %s", esp_err_to_name(result));
    }
    else
    {
        log_d("Broadcast message sent");
    }
}

void PowerFunctionsIrBroadcast::_onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    PowerFunctionsIrMessage message = _recvMessage;

    memcpy(&message, incomingData, sizeof(message));
    log_d("\n[P:%d|S:%d|C:%d]\n", message.port, message.pwm, message.channel);

    if (powerFunctionsIrRecvCallBackInstance != nullptr)
    {
        powerFunctionsIrRecvCallBackInstance(message);
    }
}

void PowerFunctionsIrBroadcast::single_pwm(PowerFunctionsPort port, PowerFunctionsPwm pwm, uint8_t channel, bool rebroadcast)
{
    powerFunctionsCtl.single_pwm(port, pwm, channel);

    if (_broadcastEnabled && rebroadcast)
    {
        _broadcastMessage(PowerFunctionsCall::SinglePwm, port, pwm, channel);
    }
}

void PowerFunctionsIrBroadcast::single_increment(PowerFunctionsPort port, uint8_t channel, bool rebroadcast)
{
    powerFunctionsCtl.single_increment(port, channel);

    if (_broadcastEnabled && rebroadcast)
    {
        _broadcastMessage(PowerFunctionsCall::SingleIncrement, port, PowerFunctionsPwm::FLOAT, channel);
    }
}

void PowerFunctionsIrBroadcast::single_decrement(PowerFunctionsPort port, uint8_t channel, bool rebroadcast)
{
    powerFunctionsCtl.single_decrement(port, channel);

    if (_broadcastEnabled && rebroadcast)
    {
        _broadcastMessage(PowerFunctionsCall::SingleDecrement, port, PowerFunctionsPwm::FLOAT, channel);
    }
}

PowerFunctionsPwm PowerFunctionsIrBroadcast::speedToPwm(byte speed)
{
    return powerFunctionsCtl.speedToPwm(speed);
}

void PowerFunctionsIrBroadcast::registerRecvCallback(PowerFunctionsIrRecvCallback callback)
{
    if (!_broadcastEnabled)
    {
        log_w("Broadcast not enabled, callback will not be registered");
        return;
    }

    if (callback != nullptr)
    {
        powerFunctionsIrRecvCallBackInstance = callback;
    }
}

void PowerFunctionsIrBroadcast::unregisterRecvCallback()
{
    powerFunctionsIrRecvCallBackInstance = nullptr;
}

void PowerFunctionsIrBroadcast::disableBroadcast()
{
    if (!_broadcastEnabled)
    {
        log_w("Broadcast already disabled");
        return;
    }

    log_w("Disabling broadcast");
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
}

void PowerFunctionsIrBroadcast::enableBroadcast()
{
    if (_broadcastEnabled)
    {
        log_w("Broadcast already enabled");
        return;
    }

    log_w("Enabling broadcast");

    WiFi.mode(WIFI_STA);

    esp_err_t result;
    if ((result = esp_now_init()) != ESP_OK)
    {
        log_e("Error initializing ESP-NOW: %s", esp_err_to_name(result));
        return;
    }

    memcpy(_broadcastPeerInfo.peer_addr, _broadcastAddress, 6);
    _broadcastPeerInfo.channel = 0;
    _broadcastPeerInfo.encrypt = false;

    if (esp_now_add_peer(&_broadcastPeerInfo) != ESP_OK)
    {
        log_e("Failed to add peer");
        return;
    }

    esp_now_register_recv_cb(_onDataRecv);
    _broadcastEnabled = true;
}
