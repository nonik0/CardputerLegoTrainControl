#include <Arduino.h>
#include <esp_now.h>
#include <M5Atom.h>
#include <WiFi.h>

#include "..\IrBroadcast.h"
#include "IrReflective.h"
#include "TrackSwitch.h"
#include "Ultrasonic.h"

// ATOMIC PortABC Extension Base
#define ATOM_PORT_A_Y 25
#define ATOM_PORT_A_W 21
#define ATOM_PORT_B_Y 23
#define ATOM_PORT_B_W 33
#define ATOM_PORT_C_Y 22
#define ATOM_PORT_C_W 19

#define REDC 0xFF0000
#define YELLOWC 0xFFFF00
#define GREENC 0x00FF00
#define BLUEC 0x0000FF

#define REDIRECT_THRESHOLD_MS 5000

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
  // only manual control from remote
  Manual,
  // track is switched after each train passes
  Alternate,
  // redirect incoming train to longer route if too close behind another train
  Redirect,
  // redirect incoming train to to a loop if too close behind another train
  RedirectLoop,
  NumBehaviors
};

PowerFunctionsIrBroadcast client;
Ultrasonic ultrasonic;
IrReflective irReflective;
TrackSwitch trackSwitch;
SwitchBehavior switchBehavior;

unsigned long lastTrainExitMs = 0;
bool redirecting = false;
bool redirected = false;

void recvCallback(PowerFunctionsIrMessage message)
{
  log_i("\n[P:%d|S:%d|C:%d]\n", message.port, message.pwm, message.channel);

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
  if (direction != TrainDirection::Forking)
    return;

  if (position == TrainPosition::Exiting)
    trackSwitch.switchTrack();
}

void trainRedirectCallbackImpl(TrainPosition position, TrainDirection direction, bool loop)
{
  // no switching behavior possible in reverse direction
  if (direction != TrainDirection::Forking)
    return;

  // if second train is entering switch soon after an initial train has exited, redirect it unless already redirected
  if (!redirecting && !redirected && position == TrainPosition::Entering && millis() - lastTrainExitMs < REDIRECT_THRESHOLD_MS)
  {
    log_w("Last train exited %d ms ago (<%d), redirecting", millis() - lastTrainExitMs, REDIRECT_THRESHOLD_MS);
    trackSwitch.switchTrack();
    redirecting = true;
  }
  else if (position == TrainPosition::Exiting)
  {
    lastTrainExitMs = millis();
    if (redirecting)
    {
      log_w("Done redirecting, switching back");
      redirecting = false;
      redirected = loop; // if loop allows redirected train to exit redirect loop
      trackSwitch.switchTrack();
    }
    else if (redirected)
    {
      redirected = false;
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

void trainRedirectCallback(TrainPosition position, TrainDirection direction)
{
  trainRedirectCallbackImpl(position, direction, false);
}

void trainRedirectLoopCallback(TrainPosition position, TrainDirection direction)
{
  trainRedirectCallbackImpl(position, direction, true);
}

void setup()
{
  M5.begin(true, false, true);

  client.enableBroadcast();
  client.registerRecvCallback(recvCallback);

  irReflective.begin(IR_DOUT_PIN);            // join sensor
  ultrasonic.begin(US_TRIG_PIN, US_ECHO_PIN); // fork sensor
  trackSwitch.begin(
      []()  
      { return irReflective.isDetected(); },
      []()
      { float distance = ultrasonic.getDistance(); return distance > 10.0f && distance < 165.0f; },
      client,
      PowerFunctionsPort::BLUE,
      true);

  M5.dis.drawpix(0, REDC);
  switchBehavior = SwitchBehavior::Manual;
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
    case SwitchBehavior::Manual:
      log_w("Switch behavior: None");
      trackSwitch.registerCallback(nullptr);
      M5.dis.drawpix(0, REDC);
      break;
    case SwitchBehavior::Alternate:
      log_w("Switch behavior: Alternate");
      trackSwitch.registerCallback(alternatingSwitchCallback);
      M5.dis.drawpix(0, YELLOWC);
      break;
    case SwitchBehavior::Redirect:
      log_w("Switch behavior: Train Redirect");
      trackSwitch.registerCallback(trainRedirectCallback);
      M5.dis.drawpix(0, GREENC);
      break;
    case SwitchBehavior::RedirectLoop:
      log_w("Switch behavior: Train Redirect Loop");
      trackSwitch.registerCallback(trainRedirectLoopCallback);
      M5.dis.drawpix(0, BLUEC);
      break;
    }

    delay(100);
  }

  delay(10);
}