#include <Arduino.h>
#include <esp_now.h>
#include <M5Atom.h>
#include <Ultrasonic.h>
#include <WiFi.h>

#include "..\IrBroadcast.hpp"

// TODO: requires updating Legoino source code to build, need to change two method signatures to use explicit int size, e.g.:
// LegoinoCommon:cpp
// L101: static uint32_t ReadUInt32LE(uint8_t *data, int offset);
// L107: static int32_t ReadInt32LE(uint8_t *data, int offset);

PowerFunctionsIrBroadcast client;
Ultrasonic distSensor(32, 26); // ATOM

unsigned int distance;
bool switchDirection = false;

void recvCallback(PowerFunctionsIrMessage message)
{
  // don't rebroadcast on actions taken from messages received

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

void switchTrack()
{
  switchDirection = !switchDirection;

  if (switchDirection)
  {
    client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::FORWARD7, 0);
    delay(500);
    client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::BRAKE, 0);
  }
  else
  {
    client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::REVERSE7, 0);
    delay(500);
    client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::BRAKE, 0);
  }
}

void handleButtonInput()
{
  M5.update();
  if (M5.Btn.wasPressed())
  {
    switchTrack();
  }
}

void handleSensorInput()
{
  if (distance < 10 && !switchDirection)
  {
    switchTrack();
  }
  else if (distance > 10 && switchDirection)
  {
    delay(3000); // TODO: better delay logic?

    // return if dist is too close after waiting
    if (distSensor.read() < 10)
    {
      return;
    }

    switchTrack();
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
  distance = distSensor.read();

  handleButtonInput();
  handleSensorInput();

  delay(100);

  // TODO: callbacks for various functionality on repeaters (lights, sounds, etc.)
}