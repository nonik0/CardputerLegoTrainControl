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

uint8_t controlSwitchChannel = 1;
PowerFunctionsPort controlSwitchPort = PowerFunctionsPort::BLUE;
unsigned long lastTrainExitMs = 0;
bool redirecting = false;
bool redirected = false;

// TODO: improve readability with enum or value defines
void broadcastDetectionCallback(TrainPosition position, TrainDirection direction)
{
  static uint8_t lastTrainDetection = 0xFF;

  // Undetected is always 0 regardless of direction
  if (position == TrainPosition::Undetected || direction == TrainDirection::Undetected)
  {
    if (lastTrainDetection == 0)
    {
      log_w("Skipping duplicate broadcast of last detection: 0");
      return;
    }
    client.switch_detection(controlSwitchPort, (PowerFunctionsPwm)0, controlSwitchChannel);
    lastTrainDetection = 0;
    return;
  }

  // Forking: 1=entering, 2=passing, 3=exiting
  // Merging: 4=entering, 5=passing, 6=exiting
  uint8_t positionOffset;
  switch (position)
  {
  case TrainPosition::Entering:
    positionOffset = 1;
    break;
  case TrainPosition::Passing:
    positionOffset = 2;
    break;
  case TrainPosition::Exiting:
    positionOffset = 3;
    break;
  default:
    return; // shouldn't happen
  }

  uint8_t directionOffset;
  switch (direction)
  {
  case TrainDirection::Forking:
    directionOffset = 0;
    break;
  case TrainDirection::Merging:
    directionOffset = 3;
    break;
  default:
    return; // shouldn't happen with a valid detected position
  }

  uint8_t trainDetection = positionOffset + directionOffset;

  if (trainDetection == lastTrainDetection)
  {
    log_w("Skipping duplicate broadcast of last detection: %d", trainDetection);
    return;
  }

  client.switch_detection(controlSwitchPort, (PowerFunctionsPwm)trainDetection, controlSwitchChannel);
  lastTrainDetection = trainDetection;
}

void alternatingSwitchCallback(TrainPosition position, TrainDirection direction)
{
  broadcastDetectionCallback(position, direction);

  // no switching behavior possible in reverse direction
  if (direction != TrainDirection::Forking)
    return;

  // clean exit is when position transitions to undetected but direction is not
  if (position == TrainPosition::Undetected && direction != TrainDirection::Undetected)
    trackSwitch.switchTrack();
}

void trainRedirectCallbackImpl(TrainPosition position, TrainDirection direction, bool loop)
{
  broadcastDetectionCallback(position, direction);

  // no switching behavior possible in reverse direction
  if (direction != TrainDirection::Forking)
    return;

  // if second train is entering switch soon after an initial train has exited, redirect it unless already redirected
  if (!redirecting && !redirected && position == TrainPosition::Entering && millis() - lastTrainExitMs < REDIRECT_THRESHOLD_MS)
  {
    log_i("Last train exited %d ms ago (<%d), redirecting", millis() - lastTrainExitMs, REDIRECT_THRESHOLD_MS);
    trackSwitch.switchTrack();
    redirecting = true;
  }
  else if (position == TrainPosition::Undetected)
  {
    // clean exit
    if (direction != TrainDirection::Undetected)
    {
      lastTrainExitMs = millis();
      if (redirecting)
      {
        log_i("Done redirecting, switching back");
        redirecting = false;
        redirected = loop; // if loop allows redirected train to exit redirect loop
        trackSwitch.switchTrack();
      }
      else if (redirected)
      {
        redirected = false;
      }
    }
    else
    {
      if (redirecting)
      {
        log_i("Tracking issue, switching back");
        redirecting = false;
        trackSwitch.switchTrack();
      }
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

void toggle_mode(PowerFunctionsPort port, PowerFunctionsPwm pwm, uint8_t channel)
{
  if (port != controlSwitchPort || channel != controlSwitchChannel)
  {
    log_i("Port and channel do not match control track switch, not toggling mode");
    return;
  }

  if (pwm != PowerFunctionsPwm::FLOAT)
  {
    log_i("Not toggle PWM value, not toggling mode");
  }

  switchBehavior = static_cast<SwitchBehavior>((static_cast<int>(switchBehavior) + 1) % static_cast<int>(SwitchBehavior::NumBehaviors));
  switch (switchBehavior)
  {
  case SwitchBehavior::Manual:
    log_i("Switch behavior: None");
    trackSwitch.registerCallback(broadcastDetectionCallback);
    M5.dis.drawpix(0, REDC);
    break;
  case SwitchBehavior::Alternate:
    log_i("Switch behavior: Alternate");
    trackSwitch.registerCallback(alternatingSwitchCallback);
    M5.dis.drawpix(0, YELLOWC);
    break;
  case SwitchBehavior::Redirect:
    log_i("Switch behavior: Train Redirect");
    trackSwitch.registerCallback(trainRedirectCallback);
    M5.dis.drawpix(0, GREENC);
    break;
  case SwitchBehavior::RedirectLoop:
    log_i("Switch behavior: Train Redirect Loop");
    trackSwitch.registerCallback(trainRedirectLoopCallback);
    M5.dis.drawpix(0, BLUEC);
    break;
  }

  // broadcast state using PWM value
  client.switch_mode(controlSwitchPort, (PowerFunctionsPwm)((uint8_t)switchBehavior + 1), controlSwitchChannel);
}

void toggle_mode()
{
  toggle_mode(controlSwitchPort, PowerFunctionsPwm::FLOAT, controlSwitchChannel);
}

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
  case PowerFunctionsCall::SwitchMode:
    toggle_mode(message.port, message.pwm, message.channel);
    break;
  }
}

bool join_detection()
{
  return irReflective.isDetected();
}

bool fork_detection()
{
  float distance = ultrasonic.getDistance();
  return distance > 10.0f && distance < 165.0f;
}

void setup()
{
  M5.begin(true, false, true);

  client.enableBroadcast();
  client.registerRecvCallback(recvCallback);

  irReflective.begin(IR_DOUT_PIN);            // join sensor
  ultrasonic.begin(US_TRIG_PIN, US_ECHO_PIN); // fork sensor
  trackSwitch.begin(join_detection, fork_detection, client, controlSwitchPort, controlSwitchChannel, true);

  switchBehavior = SwitchBehavior::Manual;
  trackSwitch.registerCallback(broadcastDetectionCallback);
  M5.dis.drawpix(0, REDC);
  log_i("Setup complete");
}

void loop()
{
  trackSwitch.update();

  M5.update();
  if (M5.Btn.wasPressed())
  {
    toggle_mode();
    delay(100);
  }

  delay(10);
}