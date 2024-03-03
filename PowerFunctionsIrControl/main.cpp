#include <Arduino.h>
#include <esp_now.h>
#include <M5Atom.h>
#include <WiFi.h>

#include "..\IrBroadcast.hpp"
#include "Ultrasonic.h"

// ATOMIC PortABC Extension Base
#define ATOM_PORT_A_Y 25
#define ATOM_PORT_A_W 21
#define ATOM_PORT_B_Y 23
#define ATOM_PORT_B_W 33
#define ATOM_PORT_C_Y 22
#define ATOM_PORT_C_W 19

// Port A: IR TX/RX
//#define IR_TX_PIN ATOM_PORT_A_Y # defined in platformio.ini

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

float distance;
bool switchDirection = false;

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

void switchTrack()
{
  switchDirection = !switchDirection;

  if (switchDirection)
  {
    client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::REVERSE7, 0);
    delay(50);
    client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::FORWARD7, 0);
    delay(500);
    client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::BRAKE, 0);
  }
  else
  {
    client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::FORWARD7, 0);
    delay(50);
    client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::REVERSE7, 0);
    delay(500);
    client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::BRAKE, 0);
  }
}

void setup()
{
  M5.begin(true, false, false);

  client.enableBroadcast();
  client.registerRecvCallback(recvCallback);

  ultrasonic.begin(US_TRIG_PIN, US_ECHO_PIN);
}

// Ultrasonic:
// near track: < 30 mm
// far track: < 165 mm

void loop()
{
  distance = ultrasonic.getDistance();

  if (distance < 4000 && distance > 10) {
    if (distance < 35.0f)
      Serial.printf("US: near (%f cm)\n", distance);
    else if (distance < 165.0f)
      Serial.printf("US: far (%f cm)\n", distance);
  }

  if (digitalRead(IR_DOUT_PIN) == 0)
    Serial.println("IR: detect");

  delay(200);
  // TODO: callbacks for various functionality on repeaters (lights, sounds, etc.)
}