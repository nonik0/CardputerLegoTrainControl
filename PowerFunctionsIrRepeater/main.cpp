#include <Arduino.h>
#include <esp_now.h>
#include <PowerFunctions.h>
#include <WiFi.h>

#include "..\IrBroadcast.hpp"

// TODO: requires updating Legoino source code to build, need to change two method signatures to use explicit int size, e.g.:
// LegoinoCommon:cpp
// L101: static uint32_t ReadUInt32LE(uint8_t *data, int offset);
// L107: static int32_t ReadInt32LE(uint8_t *data, int offset);

PowerFunctionsIrBroadcast _client;

void setup()
{
  Serial.begin(115200);

  _client.setMode(PowerFunctionsRepeatMode::Repeat);
}

void loop()
{
  Serial.print('.');
  delay(1000);
}