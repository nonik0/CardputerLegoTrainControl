#include <Arduino.h>
#include <esp_now.h>
#include <M5Atom.h>
#include <WiFi.h>

#include "..\IrBroadcast.h"
#include "IrReflective.hpp"
#include "Ultrasonic.h"
#include "TrackSwitch.h"

// ATOMIC PortABC Extension Base
#define ATOM_PORT_A_Y 25
#define ATOM_PORT_A_W 21
#define ATOM_PORT_B_Y 23
#define ATOM_PORT_B_W 33
#define ATOM_PORT_C_Y 22
#define ATOM_PORT_C_W 19

// Port A: IR TX/RX
// #define IR_TX_PIN ATOM_PORT_A_Y # defined in platformio.ini

// Port B: Reflective IR Sensor
#define IR_DOUT_PIN ATOM_PORT_B_Y

// Port C: Ultrasonic Sensor
#define US_ECHO_PIN ATOM_PORT_C_W
#define US_TRIG_PIN ATOM_PORT_C_Y

// TODO: still required?
// TODO: requires updating Legoino source code to build, need to change two method signatures to use explicit int size, e.g.:
// LegoinoCommon:cpp
// L101: static uint32_t ReadUInt32LE(uint8_t *data, int offset);
// L107: static int32_t ReadInt32LE(uint8_t *data, int offset);

enum class SwitchBehavior
{
  None,
  Alternate,
  Redirect,
  NumBehaviors
};

PowerFunctionsIrBroadcast client;
Ultrasonic ultrasonic;
IrReflective irReflective;
TrackSwitch trackSwitch;
SwitchBehavior switchBehavior = SwitchBehavior::None;

unsigned long lastTrainExited = 0;
bool redirecting = false;
unsigned long logMillis = 5000;

bool readIrReflectiveSensor()
{
  return irReflective.isDetected();
}

bool readUltrasonicSensor()
{
  float distance = ultrasonic.getDistance();
  return distance > 10.0f && distance < 165.0f;

  // if (distance > 10.0f && distance < 35.0f)
  //     return TrainTrack::Fork;
  // else if (distance < 165.0f)
  //     return TrainTrack::Main;
  // else
  //     return TrainTrack::Undetected;
}

void recvCallback(PowerFunctionsIrMessage message)
{
  Serial.printf("\n[P:%d|S:%d|C:%d]\n", message.port, message.pwm, message.channel);

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

void alternatingSwitchCallback(TrainPosition position, TrainDirection direction)
{
  // no switching behavior possible in reverse direction
  if (direction != TrainDirection::Forward)
    return;

  if (position == TrainPosition::Exiting)
    trackSwitch.switchTrack();
}

void trainRedirectCallback(TrainPosition position, TrainDirection direction)
{
  // no switching behavior possible in reverse direction
  if (direction != TrainDirection::Forward)
    return;

  // if second train is entering switch soon after an initial train has exited, redirect it
  if (!redirecting && position == TrainPosition::Entering && millis() - lastTrainExited < 10000)
  {
    log_w("Redirecting train, switching track");
    trackSwitch.switchTrack();
    redirecting = true;
  }
  else if (position == TrainPosition::Exiting)
  {
    lastTrainExited = millis();
    if (redirecting)
    {
      log_w("Done redirecting, switching back");
      redirecting = false;
      trackSwitch.switchTrack();
    }
  }
  else if (position == TrainPosition::Undetected)
  {
    if (redirecting)
    {
      log_w("Tracking issue, switching back");
      redirecting = false;
      trackSwitch.switchTrack();
    }
  }
}

void setup()
{
  M5.begin(true, false, false);

  client.enableBroadcast();
  client.registerRecvCallback(recvCallback);

  irReflective.begin(IR_DOUT_PIN);            // join sensor
  ultrasonic.begin(US_TRIG_PIN, US_ECHO_PIN); // fork sensor
  trackSwitch.begin(readIrReflectiveSensor, readUltrasonicSensor, client, PowerFunctionsPort::BLUE, true);

  log_w("Setup complete");
}

void loop()
{
  trackSwitch.update();

  M5.update();
  if (M5.Btn.wasPressed())
  {
    switchBehavior = static_cast<SwitchBehavior>((static_cast<int>(switchBehavior) + 1) % static_cast<int>(SwitchBehavior::NumBehaviors));
    switch (switchBehavior)
    {
    case SwitchBehavior::None:
      log_w("Switch behavior: None");
      trackSwitch.registerCallback(nullptr);
      break;
    case SwitchBehavior::Alternate:
      log_w("Switch behavior: Alternate");
      trackSwitch.registerCallback(alternatingSwitchCallback);
      break;
    case SwitchBehavior::Redirect:
      log_w("Switch behavior: Train Redirect");
      trackSwitch.registerCallback(trainRedirectCallback);
      break;
    }

    delay(100);
  }

  delay(10);
}