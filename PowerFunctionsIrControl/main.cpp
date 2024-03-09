#include <Arduino.h>
#include <esp_now.h>
#include <M5Atom.h>
#include <WiFi.h>

#include "..\IrBroadcast.hpp"
#include "IrReflective.hpp"
#include "TrackSwitch.h"
#include "Ultrasonic.h"

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

PowerFunctionsIrBroadcast client;
Ultrasonic ultrasonic;
IrReflective irReflective;

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

TrackSwitch trackSwitch;

void setup()
{
  M5.begin(true, false, false);

  client.enableBroadcast();
  client.registerRecvCallback(recvCallback);

  irReflective.begin(IR_DOUT_PIN);
  ultrasonic.begin(US_TRIG_PIN, US_ECHO_PIN);
  trackSwitch.begin(ultrasonic, irReflective, (byte)(PowerFunctionsPort::RED));
}

unsigned long lastTrainExited = 0;
TrainTrack lastTrackExited = TrainTrack::Undetected;

void loop()
{
  trackSwitch.update();

  // when another train enters forward before X time has passed since the last train exited, switch the track

  // trackSwitch.onEnter = [&lastTrainExited, &lastTrackExited](TrainTrack track) {
  //   if (lastTrackExited == TrainTrack::Main && track == TrainTrack::Fork && millis() - lastTrainExited < 5000)
  //   {
  //     client.single_pwm(PowerFunctionsPort::RED, 7, PowerFunctionsChannel::RED, false);
  //     delay(1000);
  //     client.single_pwm(PowerFunctionsPort::RED, 0, PowerFunctionsChannel::RED, false);
  //   }
  // };

  // trackSwitch.onExit = [&lastTrainExited, &lastTrackExited](TrainTrack track) {
  //   lastTrainExited = millis();
  //   lastTrackExited = track;
  // };

  delay(50);
  // TODO: callbacks for various functionality on repeaters (lights, sounds, etc.)
}