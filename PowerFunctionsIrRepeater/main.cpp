#include <Arduino.h>
#include <esp_now.h>
#include <M5Atom.h>
#include <WiFi.h>

#include "..\IrBroadcast.h"

PowerFunctionsIrBroadcast client;

void recvCallback(PowerFunctionsIrMessage message)
{
  switch (message.call)
  {
  case PowerFunctionsCall::SinglePwm:
    client.single_pwm(message.port, message.pwm, message.channel, false);
    break;
  case PowerFunctionsCall::SingleIncrement:
    client.single_increment(message.port, message.channel, false);
    break;
  case PowerFunctionsCall::SingleDecrement:
    client.single_decrement(message.port, message.channel, false);
    break;
  }
}

void setup()
{
  M5.begin(true, false, false);

  client.enableBroadcast();
  client.registerRecvCallback(recvCallback);
}

void loop()
{
}